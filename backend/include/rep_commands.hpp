#pragma once
#include "parser.hpp"
#include "commands.hpp"
#include "disk_utils.hpp"
#include "fs_ops.hpp"
#include "rep_utils.hpp"
#include "account_commands.hpp"
#include "session.hpp"
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

// -------- helpers comunes --------

inline std::string diskPathFromId(const std::string& id, std::string& err) {
    MountState& st = mountState();
    auto it = st.mounted.find(id);
    if (it == st.mounted.end()) { err = "no existe una partición montada con id " + id; return ""; }
    return it->second.diskPath;
}

// -------- REPORTE MBR --------
inline CmdResult repMbr(const std::string& diskPath, const std::string& outPath) {
    MBR mbr;
    if (!readMBR(diskPath, mbr)) return {false, "REP: no se pudo leer el MBR"};

    std::ostringstream dot;
    dot << "digraph G {\nnode [shape=plain]\nrankdir=TB\n";
    dot << "a [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">";
    dot << "<tr><td bgcolor=\"#2c3e50\"><font color=\"white\">REPORTE DE MBR</font></td><td bgcolor=\"#2c3e50\"></td></tr>";
    dot << "<tr><td>mbr_tamano</td><td>" << mbr.mbr_tamano << "</td></tr>";
    dot << "<tr><td>mbr_fecha_creacion</td><td>" << formatTime(mbr.mbr_fecha_creacion) << "</td></tr>";
    dot << "<tr><td>mbr_dsk_signature</td><td>" << mbr.mbr_dsk_signature << "</td></tr>";

    for (int i = 0; i < 4; i++) {
        Partition& p = mbr.mbr_partitions[i];
        if (p.part_start == -1) continue;
        dot << "<tr><td bgcolor=\"#8e44ad\" colspan=\"2\"><font color=\"white\">Particion</font></td></tr>";
        dot << "<tr><td>part_status</td><td>" << p.part_status << "</td></tr>";
        dot << "<tr><td>part_type</td><td>" << p.part_type << "</td></tr>";
        dot << "<tr><td>part_fit</td><td>" << p.part_fit << "</td></tr>";
        dot << "<tr><td>part_start</td><td>" << p.part_start << "</td></tr>";
        dot << "<tr><td>part_size</td><td>" << p.part_s << "</td></tr>";
        dot << "<tr><td>part_name</td><td>" << std::string(p.part_name) << "</td></tr>";

        if (p.part_type == 'E') {
            int cursor = p.part_start;
            while (cursor != -1) {
                EBR ebr;
                readEBR(diskPath, cursor, ebr);
                dot << "<tr><td bgcolor=\"#c0392b\" colspan=\"2\"><font color=\"white\">Particion Logica (EBR)</font></td></tr>";
                dot << "<tr><td>part_mount</td><td>" << ebr.part_mount << "</td></tr>";
                dot << "<tr><td>part_fit</td><td>" << ebr.part_fit << "</td></tr>";
                dot << "<tr><td>part_start</td><td>" << ebr.part_start << "</td></tr>";
                dot << "<tr><td>part_s</td><td>" << ebr.part_s << "</td></tr>";
                dot << "<tr><td>part_next</td><td>" << ebr.part_next << "</td></tr>";
                dot << "<tr><td>part_name</td><td>" << std::string(ebr.part_name) << "</td></tr>";
                cursor = ebr.part_next;
            }
        }
    }
    dot << "</table>>]\n}\n";

    ensureParentDirs(outPath);
    if (!renderGraphviz(dot.str(), outPath)) return {false, "REP: falló al generar la imagen (¿está instalado graphviz? sudo apt install graphviz)"};
    return {true, "REP: reporte mbr generado en " + outPath};
}

// -------- REPORTE DISK --------
inline CmdResult repDisk(const std::string& diskPath, const std::string& outPath) {
    MBR mbr;
    if (!readMBR(diskPath, mbr)) return {false, "REP: no se pudo leer el MBR"};

    struct Segment { std::string label; long size; std::string color; };
    std::vector<Segment> segs;
    segs.push_back({"MBR", (long)sizeof(MBR), "#7f8c8d"});

    std::vector<std::pair<int,int>> used; // start,size (solo primarias/extendida)
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_start != -1)
            used.push_back({mbr.mbr_partitions[i].part_start, i});
    }
    std::sort(used.begin(), used.end());

    long cursor = sizeof(MBR);
    for (auto& u : used) {
        Partition& p = mbr.mbr_partitions[u.second];
        if (p.part_start > cursor) segs.push_back({"Libre", p.part_start - cursor, "#ecf0f1"});
        std::string label = (p.part_type == 'E' ? "Extendida " : "Primaria ") + std::string(p.part_name);
        segs.push_back({label, p.part_s, p.part_type == 'E' ? "#e67e22" : "#3498db"});
        cursor = p.part_start + p.part_s;
    }
    if (cursor < mbr.mbr_tamano) segs.push_back({"Libre", mbr.mbr_tamano - cursor, "#ecf0f1"});

    std::ostringstream dot;
    dot << "digraph G {\nnode [shape=plain]\n";
    dot << "a [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\"><tr>";
    for (auto& s : segs) {
        double pct = 100.0 * s.size / mbr.mbr_tamano;
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f%%", pct);
        dot << "<td bgcolor=\"" << s.color << "\">" << s.label << "<br/>" << buf << "</td>";
    }
    dot << "</tr></table>>]\n}\n";

    ensureParentDirs(outPath);
    if (!renderGraphviz(dot.str(), outPath)) return {false, "REP: falló al generar la imagen (¿está instalado graphviz?)"};
    return {true, "REP: reporte disk generado en " + outPath};
}

// -------- REPORTE SB --------
inline CmdResult repSb(FSContext& ctx, const std::string& outPath) {
    Superblock& sb = ctx.sb;
    std::ostringstream dot;
    dot << "digraph G {\nnode [shape=plain]\n";
    dot << "a [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">";
    dot << "<tr><td bgcolor=\"#16a085\" colspan=\"2\"><font color=\"white\">Reporte de SUPERBLOQUE</font></td></tr>";
    auto row = [&](const std::string& k, long v) { dot << "<tr><td>" << k << "</td><td>" << v << "</td></tr>"; };
    row("s_filesystem_type", sb.s_filesystem_type);
    row("s_inodes_count", sb.s_inodes_count);
    row("s_blocks_count", sb.s_blocks_count);
    row("s_free_blocks_count", sb.s_free_blocks_count);
    row("s_free_inodes_count", sb.s_free_inodes_count);
    dot << "<tr><td>s_mtime</td><td>" << formatTime(sb.s_mtime) << "</td></tr>";
    dot << "<tr><td>s_umtime</td><td>" << formatTime(sb.s_umtime) << "</td></tr>";
    row("s_mnt_count", sb.s_mnt_count);
    dot << "<tr><td>s_magic</td><td>0x" << std::hex << sb.s_magic << std::dec << "</td></tr>";
    row("s_inode_s", sb.s_inode_s);
    row("s_block_s", sb.s_block_s);
    row("s_firts_ino", sb.s_firts_ino);
    row("s_first_blo", sb.s_first_blo);
    row("s_bm_inode_start", sb.s_bm_inode_start);
    row("s_bm_block_start", sb.s_bm_block_start);
    row("s_inode_start", sb.s_inode_start);
    row("s_block_start", sb.s_block_start);
    dot << "</table>>]\n}\n";

    ensureParentDirs(outPath);
    if (!renderGraphviz(dot.str(), outPath)) return {false, "REP: falló al generar la imagen (¿está instalado graphviz?)"};
    return {true, "REP: reporte sb generado en " + outPath};
}

// -------- REPORTE bm_inode / bm_block (texto plano, 20 por línea) --------
inline CmdResult repBitmap(FSContext& ctx, bool isInode, const std::string& outPath) {
    long start = isInode ? ctx.sb.s_bm_inode_start : ctx.sb.s_bm_block_start;
    long count = isInode ? ctx.sb.s_inodes_count : ctx.sb.s_blocks_count;

    std::ostringstream oss;
    for (long i = 0; i < count; i++) {
        char bit;
        readByte(ctx.diskPath, start + i, bit);
        oss << bit << " ";
        if ((i + 1) % 20 == 0) oss << "\n";
    }
    oss << "\n";

    ensureParentDirs(outPath);
    if (!writeTextFile(outPath, oss.str())) return {false, "REP: no se pudo escribir el archivo de texto"};
    return {true, std::string("REP: reporte ") + (isInode ? "bm_inode" : "bm_block") + " generado en " + outPath};
}

// -------- Recorre el árbol de inodos usados, clasificando bloques --------
struct FSInventory {
    std::vector<int> usedInodes;
    std::map<int, char> blockType; // 'F' carpeta, 'A' archivo
};

inline void collectInventory(FSContext& ctx, int inodeIndex, FSInventory& inv, int depth = 0) {
    if (depth > 64) return; // por si acaso, evita loops infinitos
    if (std::find(inv.usedInodes.begin(), inv.usedInodes.end(), inodeIndex) != inv.usedInodes.end()) return;
    inv.usedInodes.push_back(inodeIndex);

    Inode inode;
    readStruct(ctx.diskPath, inodeOffset(ctx, inodeIndex), inode);

    if (inode.i_type == '0') {
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) continue;
            inv.blockType[inode.i_block[i]] = 'F';
            FolderBlock fb;
            readStruct(ctx.diskPath, blockOffset(ctx, inode.i_block[i]), fb);
            for (auto& e : fb.b_content) {
                if (e.b_inodo == -1) continue;
                std::string name(e.b_name);
                if (name == "." || name == "..") continue;
                collectInventory(ctx, e.b_inodo, inv, depth + 1);
            }
        }
    } else {
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) continue;
            inv.blockType[inode.i_block[i]] = 'A';
        }
    }
}

// -------- REPORTE INODE --------
inline CmdResult repInode(FSContext& ctx, const std::string& outPath) {
    FSInventory inv;
    collectInventory(ctx, 0, inv);
    std::sort(inv.usedInodes.begin(), inv.usedInodes.end());

    std::ostringstream dot;
    dot << "digraph G {\nrankdir=LR\nnode [shape=plain]\n";
    std::string prevNode;
    for (int idx : inv.usedInodes) {
        Inode inode;
        readStruct(ctx.diskPath, inodeOffset(ctx, idx), inode);
        std::string nodeName = "n" + std::to_string(idx);
        dot << nodeName << " [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">";
        dot << "<tr><td bgcolor=\"#2980b9\" colspan=\"2\"><font color=\"white\">Inodo " << idx << "</font></td></tr>";
        dot << "<tr><td>i_uid</td><td>" << inode.i_uid << "</td></tr>";
        dot << "<tr><td>i_gid</td><td>" << inode.i_gid << "</td></tr>";
        dot << "<tr><td>i_s</td><td>" << inode.i_s << "</td></tr>";
        dot << "<tr><td>i_atime</td><td>" << formatTime(inode.i_atime) << "</td></tr>";
        for (int b = 0; b < 15; b++) {
            if (inode.i_block[b] == -1) continue;
            dot << "<tr><td>i_block[" << b << "]</td><td>" << inode.i_block[b] << "</td></tr>";
        }
        dot << "<tr><td>i_type</td><td>" << inode.i_type << "</td></tr>";
        dot << "<tr><td>i_perm</td><td>" << inode.i_perm[0] << inode.i_perm[1] << inode.i_perm[2] << "</td></tr>";
        dot << "</table>>]\n";
        if (!prevNode.empty()) dot << prevNode << " -> " << nodeName << "\n";
        prevNode = nodeName;
    }
    dot << "}\n";

    ensureParentDirs(outPath);
    if (!renderGraphviz(dot.str(), outPath)) return {false, "REP: falló al generar la imagen (¿está instalado graphviz?)"};
    return {true, "REP: reporte inode generado en " + outPath};
}

// -------- REPORTE BLOCK --------
inline CmdResult repBlock(FSContext& ctx, const std::string& outPath) {
    FSInventory inv;
    collectInventory(ctx, 0, inv);

    std::ostringstream dot;
    dot << "digraph G {\nrankdir=LR\nnode [shape=plain]\n";
    std::string prevNode;
    for (auto& kv : inv.blockType) {
        int idx = kv.first;
        std::string nodeName = "b" + std::to_string(idx);
        dot << nodeName << " [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">";
        if (kv.second == 'F') {
            dot << "<tr><td bgcolor=\"#27ae60\" colspan=\"2\"><font color=\"white\">Bloque Carpeta " << idx << "</font></td></tr>";
            dot << "<tr><td>b_name</td><td>b_inodo</td></tr>";
            FolderBlock fb;
            readStruct(ctx.diskPath, blockOffset(ctx, idx), fb);
            for (auto& e : fb.b_content) {
                if (e.b_inodo == -1) continue;
                dot << "<tr><td>" << std::string(e.b_name) << "</td><td>" << e.b_inodo << "</td></tr>";
            }
        } else {
            dot << "<tr><td bgcolor=\"#d35400\"><font color=\"white\">Bloque Archivo " << idx << "</font></td></tr>";
            FileBlock fbk;
            readStruct(ctx.diskPath, blockOffset(ctx, idx), fbk);
            std::string content(fbk.b_content, strnlen(fbk.b_content, 64));
            dot << "<tr><td>" << content << "</td></tr>";
        }
        dot << "</table>>]\n";
        if (!prevNode.empty()) dot << prevNode << " -> " << nodeName << "\n";
        prevNode = nodeName;
    }
    dot << "}\n";

    ensureParentDirs(outPath);
    if (!renderGraphviz(dot.str(), outPath)) return {false, "REP: falló al generar la imagen (¿está instalado graphviz?)"};
    return {true, "REP: reporte block generado en " + outPath};
}

// -------- REPORTE TREE --------
inline void buildTreeDot(FSContext& ctx, int inodeIndex, std::ostringstream& dot, int depth = 0) {
    if (depth > 32) return;
    Inode inode;
    readStruct(ctx.diskPath, inodeOffset(ctx, inodeIndex), inode);
    std::string nodeName = "t" + std::to_string(inodeIndex);

    dot << nodeName << " [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">";
    dot << "<tr><td bgcolor=\"#8e44ad\" colspan=\"2\"><font color=\"white\">Inodo " << inodeIndex
        << (inode.i_type == '0' ? " (carpeta)" : " (archivo)") << "</font></td></tr>";
    dot << "<tr><td>i_uid</td><td>" << inode.i_uid << "</td></tr>";
    dot << "<tr><td>i_s</td><td>" << inode.i_s << "</td></tr>";
    dot << "</table>>]\n";

    if (inode.i_type == '0') {
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) continue;
            FolderBlock fb;
            readStruct(ctx.diskPath, blockOffset(ctx, inode.i_block[i]), fb);
            for (auto& e : fb.b_content) {
                if (e.b_inodo == -1) continue;
                std::string name(e.b_name);
                if (name == "." || name == "..") continue;
                dot << nodeName << " -> t" << e.b_inodo << " [label=\"" << name << "\"]\n";
                buildTreeDot(ctx, e.b_inodo, dot, depth + 1);
            }
        }
    }
}

inline CmdResult repTree(FSContext& ctx, const std::string& outPath) {
    std::ostringstream dot;
    dot << "digraph G {\nnode [shape=plain]\n";
    buildTreeDot(ctx, 0, dot);
    dot << "}\n";

    ensureParentDirs(outPath);
    if (!renderGraphviz(dot.str(), outPath)) return {false, "REP: falló al generar la imagen (¿está instalado graphviz?)"};
    return {true, "REP: reporte tree generado en " + outPath};
}

// -------- REPORTE FILE --------
inline CmdResult repFile(FSContext& ctx, const std::string& filePath, const std::string& outPath) {
    int idx = resolvePath(ctx, filePath);
    if (idx == -1) return {false, "REP: el archivo \"" + filePath + "\" no existe"};
    Inode inode;
    readStruct(ctx.diskPath, inodeOffset(ctx, idx), inode);
    if (inode.i_type != '1') return {false, "REP: \"" + filePath + "\" no es un archivo"};

    std::string content = readFileContent(ctx, inode);
    ensureParentDirs(outPath);
    if (!writeTextFile(outPath, filePath + "\n\n" + content + "\n")) return {false, "REP: no se pudo escribir el archivo"};
    return {true, "REP: reporte file generado en " + outPath};
}

// -------- REPORTE LS --------
inline CmdResult repLs(FSContext& ctx, const std::string& folderPath, const std::string& outPath) {
    int idx = resolvePath(ctx, folderPath);
    if (idx == -1) return {false, "REP: la ruta \"" + folderPath + "\" no existe"};
    Inode folderInode;
    readStruct(ctx.diskPath, inodeOffset(ctx, idx), folderInode);
    if (folderInode.i_type != '0') return {false, "REP: \"" + folderPath + "\" no es una carpeta"};

    // Carga users.txt para traducir uid/gid a nombres
    std::vector<UserRecord> records;
    int usersInodeIndex; Inode usersInode;
    loadUsersFile(ctx, records, usersInodeIndex, usersInode);

    std::ostringstream dot;
    dot << "digraph G {\nnode [shape=plain]\n";
    dot << "a [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">";
    dot << "<tr><td bgcolor=\"#34495e\"><font color=\"white\">Permisos</font></td>"
        << "<td bgcolor=\"#34495e\"><font color=\"white\">Owner</font></td>"
        << "<td bgcolor=\"#34495e\"><font color=\"white\">Grupo</font></td>"
        << "<td bgcolor=\"#34495e\"><font color=\"white\">Size</font></td>"
        << "<td bgcolor=\"#34495e\"><font color=\"white\">Fecha</font></td>"
        << "<td bgcolor=\"#34495e\"><font color=\"white\">Tipo</font></td>"
        << "<td bgcolor=\"#34495e\"><font color=\"white\">Name</font></td></tr>";

    for (int i = 0; i < 12; i++) {
        if (folderInode.i_block[i] == -1) continue;
        FolderBlock fb;
        readStruct(ctx.diskPath, blockOffset(ctx, folderInode.i_block[i]), fb);
        for (auto& e : fb.b_content) {
            if (e.b_inodo == -1) continue;
            std::string name(e.b_name);
            if (name == "." || name == "..") continue;
            Inode entryInode;
            readStruct(ctx.diskPath, inodeOffset(ctx, e.b_inodo), entryInode);
            dot << "<tr><td>" << (entryInode.i_type == '0' ? "d" : "-")
                << "rw" << (entryInode.i_perm[0] >= '6' ? "x" : "-")
                << "r" << (entryInode.i_perm[1] >= '4' ? "-" : "-") << "r--"
                << "</td><td>" << usernameFromUid(records, entryInode.i_uid)
                << "</td><td>" << groupnameFromId(records, entryInode.i_gid)
                << "</td><td>" << entryInode.i_s
                << "</td><td>" << formatTime(entryInode.i_mtime)
                << "</td><td>" << (entryInode.i_type == '0' ? "Carpeta" : "Archivo")
                << "</td><td>" << name << "</td></tr>";
        }
    }
    dot << "</table>>]\n}\n";

    ensureParentDirs(outPath);
    if (!renderGraphviz(dot.str(), outPath)) return {false, "REP: falló al generar la imagen (¿está instalado graphviz?)"};
    return {true, "REP: reporte ls generado en " + outPath};
}

// -------- Dispatcher --------
inline CmdResult cmdRep(const ParsedCommand& cmd) {
    if (!hasParam(cmd, "name")) return {false, "REP: falta el parámetro obligatorio -name"};
    if (!hasParam(cmd, "path")) return {false, "REP: falta el parámetro obligatorio -path"};
    if (!hasParam(cmd, "id"))   return {false, "REP: falta el parámetro obligatorio -id"};

    std::string name = toLower(getParam(cmd, "name"));
    std::string outPath = getParam(cmd, "path");
    std::string id = getParam(cmd, "id");

    if (name == "mbr" || name == "disk") {
        std::string err;
        std::string diskPath = diskPathFromId(id, err);
        if (diskPath.empty()) return {false, "REP: " + err};
        return name == "mbr" ? repMbr(diskPath, outPath) : repDisk(diskPath, outPath);
    }

    if (name == "sb" || name == "bm_inode" || name == "bm_block" || name == "inode" ||
        name == "block" || name == "tree" || name == "file" || name == "ls") {
        FSContext ctx = resolveFS(id);
        if (!ctx.ok) return {false, "REP: " + ctx.error};

        if (name == "sb")       return repSb(ctx, outPath);
        if (name == "bm_inode") return repBitmap(ctx, true, outPath);
        if (name == "bm_block") return repBitmap(ctx, false, outPath);
        if (name == "inode")    return repInode(ctx, outPath);
        if (name == "block")    return repBlock(ctx, outPath);
        if (name == "tree")     return repTree(ctx, outPath);

        if (name == "file" || name == "ls") {
            if (!hasParam(cmd, "path_file_ls")) return {false, "REP: falta el parámetro -path_file_ls"};
            std::string target = getParam(cmd, "path_file_ls");
            return name == "file" ? repFile(ctx, target, outPath) : repLs(ctx, target, outPath);
        }
    }

    return {false, "REP: -name inválido, use mbr, disk, inode, block, bm_inode, bm_block, tree, sb, file o ls"};
}