#pragma once
#include "ext2_structs.hpp"
#include "ext2_utils.hpp"
#include "mount_manager.hpp"
#include "disk_utils.hpp"
#include <string>
#include <vector>
#include <cstring>
#include <cmath>

struct FSContext {
    bool ok = false;
    std::string error;
    std::string diskPath;
    long partStart = 0;
    Superblock sb;
};

// Ubica el disco/superbloque de la partición montada con ese id
inline FSContext resolveFS(const std::string& id) {
    FSContext ctx;
    MountState& st = mountState();
    auto it = st.mounted.find(id);
    if (it == st.mounted.end()) {
        ctx.error = "no existe una partición montada con id " + id;
        return ctx;
    }
    MountedPartition& mp = it->second;
    MBR mbr;
    if (!readMBR(mp.diskPath, mbr)) { ctx.error = "no se pudo leer el MBR"; return ctx; }

    Partition& p = mbr.mbr_partitions[mp.partitionIndex];
    Superblock sb;
    if (!readStruct(mp.diskPath, p.part_start, sb)) { ctx.error = "no se pudo leer el superbloque"; return ctx; }
    if (sb.s_magic != 0xEF53) { ctx.error = "la partición no ha sido formateada (usa mkfs)"; return ctx; }

    ctx.ok = true;
    ctx.diskPath = mp.diskPath;
    ctx.partStart = p.part_start;
    ctx.sb = sb;
    return ctx;
}

inline long inodeOffset(const FSContext& ctx, int index) {
    return ctx.sb.s_inode_start + (long)index * ctx.sb.s_inode_s;
}

inline long blockOffset(const FSContext& ctx, int index) {
    return ctx.sb.s_block_start + (long)index * 64;
}

// Busca "name" entre los bloques directos de una carpeta. Devuelve el inodo o -1.
inline int findInFolder(const FSContext& ctx, int folderInodeIndex, const std::string& name) {
    Inode folderInode;
    readStruct(ctx.diskPath, inodeOffset(ctx, folderInodeIndex), folderInode);
    for (int i = 0; i < 12; i++) {
        if (folderInode.i_block[i] == -1) continue;
        FolderBlock fb;
        readStruct(ctx.diskPath, blockOffset(ctx, folderInode.i_block[i]), fb);
        for (auto& e : fb.b_content) {
            if (e.b_inodo == -1) continue;
            if (std::string(e.b_name) == name) return e.b_inodo;
        }
    }
    return -1;
}

// Resuelve una ruta absoluta (ej "/users.txt", "/carpeta/archivo") a un índice de inodo.
// Recorre desde la raíz (inodo 0) componente por componente. -1 si no existe.
inline int resolvePath(const FSContext& ctx, const std::string& path) {
    if (path.empty() || path[0] != '/') return -1;
    int current = 0; // inodo raíz
    if (path == "/") return current;

    std::string remaining = path.substr(1);
    size_t pos = 0;
    while (pos < remaining.size()) {
        size_t slash = remaining.find('/', pos);
        std::string component = (slash == std::string::npos) ? remaining.substr(pos) : remaining.substr(pos, slash - pos);
        if (!component.empty()) {
            current = findInFolder(ctx, current, component);
            if (current == -1) return -1;
        }
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    return current;
}


// Verifica permiso (r=4,w=2,x=1) de un inodo para la sesión actual. root siempre puede.
inline bool hasPermission(const Inode& inode, int uid, int gid, bool isRoot, int bit) {
    if (isRoot) return true;
    int permDigit;
    if (uid == inode.i_uid) permDigit = inode.i_perm[0] - '0';
    else if (gid == inode.i_gid) permDigit = inode.i_perm[1] - '0';
    else permDigit = inode.i_perm[2] - '0';
    return (permDigit & bit) != 0;
}

inline int allocateInode(const FSContext& ctx) {
    for (int i = 0; i < ctx.sb.s_inodes_count; i++) {
        char bit;
        readByte(ctx.diskPath, ctx.sb.s_bm_inode_start + i, bit);
        if (bit == '0') {
            writeByte(ctx.diskPath, ctx.sb.s_bm_inode_start + i, '1');
            return i;
        }
    }
    return -1;
}

// Reserva el primer bloque libre según el bitmap; -1 si no hay.
inline int allocateBlock(const FSContext& ctx) {
    for (int i = 0; i < ctx.sb.s_blocks_count; i++) {
        char bit;
        readByte(ctx.diskPath, ctx.sb.s_bm_block_start + i, bit);
        if (bit == '0') {
            writeByte(ctx.diskPath, ctx.sb.s_bm_block_start + i, '1');
            return i;
        }
    }
    return -1;
}

// Agrega una entrada (nombre -> inodo) a una carpeta, reutilizando slots libres
// o reservando un nuevo bloque directo si hace falta (máx 12 bloques = 48 entradas).
inline bool addFolderEntry(FSContext& ctx, int folderInodeIndex, Inode& folderInode,
                            const std::string& name, int targetInodeIndex) {
    for (int i = 0; i < 12; i++) {
        if (folderInode.i_block[i] == -1) {
            int b = allocateBlock(ctx);
            if (b == -1) return false;
            folderInode.i_block[i] = b;
            ctx.sb.s_free_blocks_count--;
            FolderBlock fb;
            for (auto& e : fb.b_content) { memset(e.b_name, 0, sizeof(e.b_name)); e.b_inodo = -1; }
            strncpy(fb.b_content[0].b_name, name.c_str(), sizeof(fb.b_content[0].b_name) - 1);
            fb.b_content[0].b_inodo = targetInodeIndex;
            writeStruct(ctx.diskPath, blockOffset(ctx, b), fb);
            folderInode.i_mtime = time(nullptr);
            writeStruct(ctx.diskPath, inodeOffset(ctx, folderInodeIndex), folderInode);
            writeStruct(ctx.diskPath, ctx.partStart, ctx.sb);
            return true;
        }
        FolderBlock fb;
        readStruct(ctx.diskPath, blockOffset(ctx, folderInode.i_block[i]), fb);
        for (auto& e : fb.b_content) {
            if (e.b_inodo == -1) {
                memset(e.b_name, 0, sizeof(e.b_name));
                strncpy(e.b_name, name.c_str(), sizeof(e.b_name) - 1);
                e.b_inodo = targetInodeIndex;
                writeStruct(ctx.diskPath, blockOffset(ctx, folderInode.i_block[i]), fb);
                folderInode.i_mtime = time(nullptr);
                writeStruct(ctx.diskPath, inodeOffset(ctx, folderInodeIndex), folderInode);
                return true;
            }
        }
    }
    return false; // sin espacio (requeriría bloques indirectos)
}

// Crea una carpeta nueva (inodo + bloque con . y ..) dentro de otra, y la registra.
inline int createFolderUnder(FSContext& ctx, int parentIndex, const std::string& name, int uid, int gid) {
    int newInode = allocateInode(ctx);
    if (newInode == -1) return -1;
    ctx.sb.s_free_inodes_count--;

    int newBlock = allocateBlock(ctx);
    if (newBlock == -1) return -1;
    ctx.sb.s_free_blocks_count--;

    Inode inode;
    inode.i_uid = uid; inode.i_gid = gid;
    inode.i_s = sizeof(FolderBlock);
    inode.i_atime = inode.i_ctime = inode.i_mtime = time(nullptr);
    inode.i_block[0] = newBlock;
    inode.i_type = '0';
    inode.i_perm[0] = '6'; inode.i_perm[1] = '6'; inode.i_perm[2] = '4';
    writeStruct(ctx.diskPath, inodeOffset(ctx, newInode), inode);

    FolderBlock fb;
    for (auto& e : fb.b_content) { memset(e.b_name, 0, sizeof(e.b_name)); e.b_inodo = -1; }
    strncpy(fb.b_content[0].b_name, ".", sizeof(fb.b_content[0].b_name) - 1);
    fb.b_content[0].b_inodo = newInode;
    strncpy(fb.b_content[1].b_name, "..", sizeof(fb.b_content[1].b_name) - 1);
    fb.b_content[1].b_inodo = parentIndex;
    writeStruct(ctx.diskPath, blockOffset(ctx, newBlock), fb);

    writeStruct(ctx.diskPath, ctx.partStart, ctx.sb);

    Inode parentInode;
    readStruct(ctx.diskPath, inodeOffset(ctx, parentIndex), parentInode);
    if (!addFolderEntry(ctx, parentIndex, parentInode, name, newInode)) return -1;

    return newInode;
}

// Separa "/a/b/c" en parentPath="/a/b" y name="c"
inline void splitPath(const std::string& path, std::string& parentPath, std::string& name) {
    size_t pos = path.find_last_of('/');
    name = path.substr(pos + 1);
    parentPath = (pos == 0) ? "/" : path.substr(0, pos);
}

// Navega parentPath desde la raíz, creando carpetas intermedias si createMissing es true.
inline int ensurePath(FSContext& ctx, const std::string& parentPath, bool createMissing,
                       int uid, int gid, std::string& err) {
    if (parentPath == "/" || parentPath.empty()) return 0;
    int current = 0;
    std::string remaining = parentPath.substr(1);
    size_t pos = 0;
    while (pos <= remaining.size()) {
        size_t slash = remaining.find('/', pos);
        std::string comp = (slash == std::string::npos) ? remaining.substr(pos) : remaining.substr(pos, slash - pos);
        if (!comp.empty()) {
            int next = findInFolder(ctx, current, comp);
            if (next == -1) {
                if (!createMissing) { err = "la carpeta \"" + comp + "\" no existe (use -r o -p)"; return -1; }
                next = createFolderUnder(ctx, current, comp, uid, gid);
                if (next == -1) { err = "no se pudo crear la carpeta intermedia \"" + comp + "\""; return -1; }
            } else {
                Inode tmp;
                readStruct(ctx.diskPath, inodeOffset(ctx, next), tmp);
                if (tmp.i_type != '0') { err = "\"" + comp + "\" no es una carpeta"; return -1; }
            }
            current = next;
        }
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    return current;
}

// Lee el contenido completo de un inodo tipo archivo (solo bloques directos, máx 12*64=768 bytes)
inline std::string readFileContent(const FSContext& ctx, const Inode& inode) {
    std::string content;
    int remaining = inode.i_s;
    for (int i = 0; i < 12 && remaining > 0; i++) {
        if (inode.i_block[i] == -1) break;
        FileBlock fb;
        readStruct(ctx.diskPath, blockOffset(ctx, inode.i_block[i]), fb);
        int chunk = std::min(64, remaining);
        content.append(fb.b_content, chunk);
        remaining -= chunk;
    }
    return content;
}

// Sobreescribe el contenido de un archivo, reutilizando bloques directos ya asignados
// y reservando nuevos si el contenido creció (soporta hasta 12 bloques directos = 768 bytes).
inline bool writeFileContent(FSContext& ctx, int inodeIndex, Inode& inode, const std::string& content) {
    int neededBlocks = (int)std::ceil(content.size() / 64.0);
    if (neededBlocks > 12) return false; // fuera de alcance de este bloque (sin indirectos)

    for (int i = 0; i < neededBlocks; i++) {
        if (inode.i_block[i] == -1) {
            int b = allocateBlock(ctx);
            if (b == -1) return false;
            inode.i_block[i] = b;
            ctx.sb.s_free_blocks_count--;
        }
        FileBlock fb;
        memset(fb.b_content, 0, sizeof(fb.b_content));
        size_t start = i * 64;
        size_t len = std::min((size_t)64, content.size() - start);
        memcpy(fb.b_content, content.data() + start, len);
        writeStruct(ctx.diskPath, blockOffset(ctx, inode.i_block[i]), fb);
    }

    inode.i_s = (int)content.size();
    inode.i_mtime = time(nullptr);
    writeStruct(ctx.diskPath, inodeOffset(ctx, inodeIndex), inode);
    writeStruct(ctx.diskPath, ctx.partStart, ctx.sb);
    return true;
}