#pragma once
#include "parser.hpp"
#include "commands.hpp"
#include "fs_ops.hpp"
#include "session.hpp"
#include <fstream>
#include <sstream>

// ---------------------- MKFILE ----------------------
inline CmdResult cmdMkfile(const ParsedCommand& cmd) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "MKFILE: no hay una sesión activa, debe iniciar sesión (login)"};
    if (!hasParam(cmd, "path")) return {false, "MKFILE: falta el parámetro obligatorio -path"};

    if (hasParam(cmd, "r") && !getParam(cmd, "r").empty())
        return {false, "MKFILE: -r no debe recibir ningún valor"};

    std::string path = getParam(cmd, "path");
    bool isRoot = (sess.user == "root");

    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) return {false, "MKFILE: " + ctx.error};

    std::string parentPath, name;
    splitPath(path, parentPath, name);

    std::string err;
    int parentIndex = ensurePath(ctx, parentPath, hasParam(cmd, "r"), sess.uid, sess.gid, err);
    if (parentIndex == -1) return {false, "MKFILE: " + err};

    Inode parentInode;
    readStruct(ctx.diskPath, inodeOffset(ctx, parentIndex), parentInode);
    if (!hasPermission(parentInode, sess.uid, sess.gid, isRoot, 2))
        return {false, "MKFILE: no tiene permiso de escritura en la carpeta padre"};

    // Determina el contenido: -cont tiene prioridad sobre -size
    std::string content;
    if (hasParam(cmd, "cont")) {
        std::ifstream in(getParam(cmd, "cont"), std::ios::binary);
        if (!in.is_open()) return {false, "MKFILE: la ruta de -cont no existe en la computadora"};
        std::ostringstream ss;
        ss << in.rdbuf();
        content = ss.str();
    } else if (hasParam(cmd, "size")) {
        int sizeVal;
        try { sizeVal = std::stoi(getParam(cmd, "size")); }
        catch (...) { return {false, "MKFILE: -size debe ser un número"}; }
        if (sizeVal < 0) return {false, "MKFILE: -size no puede ser negativo"};
        for (int i = 0; i < sizeVal; i++) content += ('0' + (i % 10));
    }

    if (content.size() > 768)
        return {false, "MKFILE: el contenido excede 768 bytes (sin bloques indirectos en este bloque)"};

    int existing = findInFolder(ctx, parentIndex, name);
    if (existing != -1) {
        Inode inode;
        readStruct(ctx.diskPath, inodeOffset(ctx, existing), inode);
        if (inode.i_type != '1') return {false, "MKFILE: ya existe una carpeta con ese nombre"};
        if (!writeFileContent(ctx, existing, inode, content)) return {false, "MKFILE: no se pudo escribir el archivo"};
        return {true, "MKFILE: archivo \"" + path + "\" sobrescrito (" + std::to_string(content.size()) + " bytes)"};
    }

    int newInode = allocateInode(ctx);
    if (newInode == -1) return {false, "MKFILE: no hay inodos libres"};
    ctx.sb.s_free_inodes_count--;

    Inode inode;
    inode.i_uid = sess.uid; inode.i_gid = sess.gid;
    inode.i_atime = inode.i_ctime = inode.i_mtime = time(nullptr);
    inode.i_type = '1';
    inode.i_perm[0] = '6'; inode.i_perm[1] = '6'; inode.i_perm[2] = '4';
    writeStruct(ctx.diskPath, ctx.partStart, ctx.sb);

    if (!writeFileContent(ctx, newInode, inode, content))
        return {false, "MKFILE: no se pudo escribir el contenido del archivo"};

    if (!addFolderEntry(ctx, parentIndex, parentInode, name, newInode))
        return {false, "MKFILE: no hay espacio en la carpeta padre para el nuevo archivo"};

    return {true, "MKFILE: archivo \"" + path + "\" creado (" + std::to_string(content.size()) + " bytes)"};
}

// ---------------------- MKDIR ----------------------
inline CmdResult cmdMkdir(const ParsedCommand& cmd) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "MKDIR: no hay una sesión activa, debe iniciar sesión (login)"};
    if (!hasParam(cmd, "path")) return {false, "MKDIR: falta el parámetro obligatorio -path"};

    if (hasParam(cmd, "p") && !getParam(cmd, "p").empty())
        return {false, "MKDIR: -p no debe recibir ningún valor"};

    std::string path = getParam(cmd, "path");
    bool isRoot = (sess.user == "root");

    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) return {false, "MKDIR: " + ctx.error};

    std::string parentPath, name;
    splitPath(path, parentPath, name);

    std::string err;
    int parentIndex = ensurePath(ctx, parentPath, hasParam(cmd, "p"), sess.uid, sess.gid, err);
    if (parentIndex == -1) return {false, "MKDIR: " + err};

    Inode parentInode;
    readStruct(ctx.diskPath, inodeOffset(ctx, parentIndex), parentInode);
    if (!hasPermission(parentInode, sess.uid, sess.gid, isRoot, 2))
        return {false, "MKDIR: no tiene permiso de escritura en la carpeta padre"};

    int existing = findInFolder(ctx, parentIndex, name);
    if (existing != -1) {
        Inode inode;
        readStruct(ctx.diskPath, inodeOffset(ctx, existing), inode);
        if (inode.i_type == '0' && hasParam(cmd, "p"))
            return {true, "MKDIR: la carpeta \"" + path + "\" ya existía, no se hizo nada"};
        return {false, "MKDIR: ya existe un archivo o carpeta con ese nombre"};
    }

    int newInode = createFolderUnder(ctx, parentIndex, name, sess.uid, sess.gid);
    if (newInode == -1) return {false, "MKDIR: no se pudo crear la carpeta (sin espacio)"};

    return {true, "MKDIR: carpeta \"" + path + "\" creada correctamente"};
}