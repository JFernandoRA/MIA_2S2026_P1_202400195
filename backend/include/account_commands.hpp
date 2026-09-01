#pragma once
#include "parser.hpp"
#include "commands.hpp"
#include "fs_ops.hpp"
#include "users_file.hpp"
#include "session.hpp"

// Lee el contenido lógico actual de users.txt de una partición ya formateada
inline bool loadUsersFile(FSContext& ctx, std::vector<UserRecord>& out, int& usersInodeIndex, Inode& usersInode) {
    usersInodeIndex = findInFolder(ctx, 0, "users.txt");
    if (usersInodeIndex == -1) return false;
    readStruct(ctx.diskPath, inodeOffset(ctx, usersInodeIndex), usersInode);
    out = parseUsersFile(readFileContent(ctx, usersInode));
    return true;
}

inline bool saveUsersFile(FSContext& ctx, int inodeIndex, Inode& inode, const std::vector<UserRecord>& records) {
    return writeFileContent(ctx, inodeIndex, inode, serializeUsersFile(records));
}

// ---------------------- LOGIN ----------------------
inline CmdResult cmdLogin(const ParsedCommand& cmd) {
    if (!hasParam(cmd, "user")) return {false, "LOGIN: falta el parámetro obligatorio -user"};
    if (!hasParam(cmd, "pass")) return {false, "LOGIN: falta el parámetro obligatorio -pass"};
    if (!hasParam(cmd, "id"))   return {false, "LOGIN: falta el parámetro obligatorio -id"};

    Session& sess = currentSession();
    if (sess.active) return {false, "LOGIN: ya hay una sesión activa, debe cerrarla primero (logout)"};

    std::string id = getParam(cmd, "id");
    FSContext ctx = resolveFS(id);
    if (!ctx.ok) return {false, "LOGIN: " + ctx.error};

    std::vector<UserRecord> records;
    int usersInodeIndex; Inode usersInode;
    if (!loadUsersFile(ctx, records, usersInodeIndex, usersInode)) return {false, "LOGIN: no se encontró users.txt"};

    std::string user = getParam(cmd, "user");
    std::string pass = getParam(cmd, "pass");

    UserRecord* found = findActiveUser(records, user);
    if (!found) return {false, "LOGIN: el usuario \"" + user + "\" no existe"};
    if (found->pass != pass) return {false, "LOGIN: autenticación fallida (contraseña incorrecta)"};

    UserRecord* grp = findActiveGroup(records, found->group);

    sess.active = true;
    sess.user = user;
    sess.uid = found->id;
    sess.gid = grp ? grp->id : 0;
    sess.groupName = found->group;
    sess.partitionId = id;

    return {true, "LOGIN: sesión iniciada como \"" + user + "\" en la partición " + id};
}

// ---------------------- LOGOUT ----------------------
inline CmdResult cmdLogout(const ParsedCommand&) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "LOGOUT: no hay ninguna sesión activa"};
    sess = Session();
    return {true, "LOGOUT: sesión finalizada"};
}

// Valida que exista sesión activa y devuelve el FSContext de su partición.
// El mensaje de error queda en fs.error si !fs.ok.
inline FSContext requireSession(CmdResult& errOut) {
    Session& sess = currentSession();
    if (!sess.active) {
        errOut = {false, "no hay una sesión activa, debe iniciar sesión (login)"};
        return FSContext{};
    }
    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) errOut = {false, ctx.error};
    return ctx;
}

// ---------------------- MKGRP ----------------------
inline CmdResult cmdMkgrp(const ParsedCommand& cmd) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "MKGRP: no hay una sesión activa, debe iniciar sesión (login)"};
    if (sess.user != "root") return {false, "MKGRP: solo el usuario root puede ejecutar este comando"};
    if (!hasParam(cmd, "name")) return {false, "MKGRP: falta el parámetro obligatorio -name"};

    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) return {false, "MKGRP: " + ctx.error};

    std::vector<UserRecord> records;
    int usersInodeIndex; Inode usersInode;
    if (!loadUsersFile(ctx, records, usersInodeIndex, usersInode)) return {false, "MKGRP: no se encontró users.txt"};

    std::string name = getParam(cmd, "name");
    if (findActiveGroup(records, name)) return {false, "MKGRP: el grupo \"" + name + "\" ya existe"};

    UserRecord r;
    r.id = nextId(records);
    r.type = 'G';
    r.group = name;
    records.push_back(r);

    if (!saveUsersFile(ctx, usersInodeIndex, usersInode, records)) return {false, "MKGRP: no se pudo guardar users.txt"};
    return {true, "MKGRP: grupo \"" + name + "\" creado con id " + std::to_string(r.id)};
}

// ---------------------- RMGRP ----------------------
inline CmdResult cmdRmgrp(const ParsedCommand& cmd) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "RMGRP: no hay una sesión activa, debe iniciar sesión (login)"};
    if (sess.user != "root") return {false, "RMGRP: solo el usuario root puede ejecutar este comando"};
    if (!hasParam(cmd, "name")) return {false, "RMGRP: falta el parámetro obligatorio -name"};

    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) return {false, "RMGRP: " + ctx.error};

    std::vector<UserRecord> records;
    int usersInodeIndex; Inode usersInode;
    if (!loadUsersFile(ctx, records, usersInodeIndex, usersInode)) return {false, "RMGRP: no se encontró users.txt"};

    std::string name = getParam(cmd, "name");
    UserRecord* g = findActiveGroup(records, name);
    if (!g) return {false, "RMGRP: el grupo \"" + name + "\" no existe"};
    g->id = 0;

    if (!saveUsersFile(ctx, usersInodeIndex, usersInode, records)) return {false, "RMGRP: no se pudo guardar users.txt"};
    return {true, "RMGRP: grupo \"" + name + "\" eliminado"};
}

// ---------------------- MKUSR ----------------------
inline CmdResult cmdMkusr(const ParsedCommand& cmd) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "MKUSR: no hay una sesión activa, debe iniciar sesión (login)"};
    if (sess.user != "root") return {false, "MKUSR: solo el usuario root puede ejecutar este comando"};
    if (!hasParam(cmd, "user")) return {false, "MKUSR: falta el parámetro obligatorio -user"};
    if (!hasParam(cmd, "pass")) return {false, "MKUSR: falta el parámetro obligatorio -pass"};
    if (!hasParam(cmd, "grp"))  return {false, "MKUSR: falta el parámetro obligatorio -grp"};

    std::string user = getParam(cmd, "user");
    std::string pass = getParam(cmd, "pass");
    std::string grp  = getParam(cmd, "grp");
    if (user.size() > 10) return {false, "MKUSR: -user no puede superar 10 caracteres"};
    if (pass.size() > 10) return {false, "MKUSR: -pass no puede superar 10 caracteres"};
    if (grp.size() > 10)  return {false, "MKUSR: -grp no puede superar 10 caracteres"};

    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) return {false, "MKUSR: " + ctx.error};

    std::vector<UserRecord> records;
    int usersInodeIndex; Inode usersInode;
    if (!loadUsersFile(ctx, records, usersInodeIndex, usersInode)) return {false, "MKUSR: no se encontró users.txt"};

    if (findActiveUser(records, user)) return {false, "MKUSR: el usuario \"" + user + "\" ya existe"};
    if (!findActiveGroup(records, grp)) return {false, "MKUSR: el grupo \"" + grp + "\" no existe"};

    UserRecord r;
    r.id = nextId(records);
    r.type = 'U';
    r.group = grp;
    r.user = user;
    r.pass = pass;
    records.push_back(r);

    if (!saveUsersFile(ctx, usersInodeIndex, usersInode, records)) return {false, "MKUSR: no se pudo guardar users.txt"};
    return {true, "MKUSR: usuario \"" + user + "\" creado con id " + std::to_string(r.id)};
}

// ---------------------- RMUSR ----------------------
inline CmdResult cmdRmusr(const ParsedCommand& cmd) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "RMUSR: no hay una sesión activa, debe iniciar sesión (login)"};
    if (sess.user != "root") return {false, "RMUSR: solo el usuario root puede ejecutar este comando"};
    if (!hasParam(cmd, "user")) return {false, "RMUSR: falta el parámetro obligatorio -user"};

    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) return {false, "RMUSR: " + ctx.error};

    std::vector<UserRecord> records;
    int usersInodeIndex; Inode usersInode;
    if (!loadUsersFile(ctx, records, usersInodeIndex, usersInode)) return {false, "RMUSR: no se encontró users.txt"};

    std::string user = getParam(cmd, "user");
    UserRecord* u = findActiveUser(records, user);
    if (!u) return {false, "RMUSR: el usuario \"" + user + "\" no existe"};
    u->id = 0;

    if (!saveUsersFile(ctx, usersInodeIndex, usersInode, records)) return {false, "RMUSR: no se pudo guardar users.txt"};
    return {true, "RMUSR: usuario \"" + user + "\" eliminado"};
}

// ---------------------- CHGRP ----------------------
inline CmdResult cmdChgrp(const ParsedCommand& cmd) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "CHGRP: no hay una sesión activa, debe iniciar sesión (login)"};
    if (sess.user != "root") return {false, "CHGRP: solo el usuario root puede ejecutar este comando"};
    if (!hasParam(cmd, "user")) return {false, "CHGRP: falta el parámetro obligatorio -user"};
    if (!hasParam(cmd, "grp"))  return {false, "CHGRP: falta el parámetro obligatorio -grp"};

    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) return {false, "CHGRP: " + ctx.error};

    std::vector<UserRecord> records;
    int usersInodeIndex; Inode usersInode;
    if (!loadUsersFile(ctx, records, usersInodeIndex, usersInode)) return {false, "CHGRP: no se encontró users.txt"};

    std::string user = getParam(cmd, "user");
    std::string grp  = getParam(cmd, "grp");

    UserRecord* u = findActiveUser(records, user);
    if (!u) return {false, "CHGRP: el usuario \"" + user + "\" no existe"};
    if (!findActiveGroup(records, grp)) return {false, "CHGRP: el grupo \"" + grp + "\" no existe o está eliminado"};

    u->group = grp;

    if (!saveUsersFile(ctx, usersInodeIndex, usersInode, records)) return {false, "CHGRP: no se pudo guardar users.txt"};
    return {true, "CHGRP: usuario \"" + user + "\" ahora pertenece al grupo \"" + grp + "\""};
}

// ---------------------- CAT ----------------------
inline CmdResult cmdCat(const ParsedCommand& cmd) {
    Session& sess = currentSession();
    if (!sess.active) return {false, "CAT: no hay una sesión activa, debe iniciar sesión (login)"};

    FSContext ctx = resolveFS(sess.partitionId);
    if (!ctx.ok) return {false, "CAT: " + ctx.error};

    std::vector<std::string> outputs;
    int i = 1;
    while (hasParam(cmd, "file" + std::to_string(i))) {
        std::string path = getParam(cmd, "file" + std::to_string(i));
        int idx = resolvePath(ctx, path);
        if (idx == -1) return {false, "CAT: el archivo \"" + path + "\" no existe"};

        Inode inode;
        readStruct(ctx.diskPath, inodeOffset(ctx, idx), inode);
        if (inode.i_type != '1') return {false, "CAT: \"" + path + "\" no es un archivo"};

        // permiso de lectura: root siempre puede; owner/group/other según i_perm
        bool canRead;
        if (sess.user == "root") canRead = true;
        else if (sess.uid == inode.i_uid) canRead = (inode.i_perm[0] - '0') & 4;
        else if (sess.gid == inode.i_gid) canRead = (inode.i_perm[1] - '0') & 4;
        else canRead = (inode.i_perm[2] - '0') & 4;
        if (!canRead) return {false, "CAT: no tiene permiso de lectura sobre \"" + path + "\""};

        outputs.push_back(readFileContent(ctx, inode));
        i++;
    }
    if (outputs.empty()) return {false, "CAT: falta al menos el parámetro -file1"};

    std::string joined;
    for (size_t k = 0; k < outputs.size(); k++) {
        if (k > 0) joined += "\n";
        joined += outputs[k];
    }
    return {true, joined};
}