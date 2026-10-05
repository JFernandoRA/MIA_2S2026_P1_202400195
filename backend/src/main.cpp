#include "httplib.h"
#include "json.hpp"
#include "parser.hpp"
#include "commands.hpp"
#include "ext2_commands.hpp"
#include "account_commands.hpp"
#include "path_commands.hpp"
#include "rep_commands.hpp"
#include <sstream>
#include <iostream>

using json = nlohmann::json;

// Ejecuta un comando ya parseado
CmdResult dispatch(const ParsedCommand& cmd) {
    if (cmd.name == "mkdisk")  return cmdMkdisk(cmd);
    if (cmd.name == "rmdisk")  return cmdRmdisk(cmd);
    if (cmd.name == "fdisk")   return cmdFdisk(cmd);
    if (cmd.name == "mount")   return cmdMount(cmd);
    if (cmd.name == "mounted") return cmdMounted(cmd);
    if (cmd.name == "mkfs")    return cmdMkfs(cmd);
    if (cmd.name == "login")   return cmdLogin(cmd);
    if (cmd.name == "logout")  return cmdLogout(cmd);
    if (cmd.name == "mkgrp")   return cmdMkgrp(cmd);
    if (cmd.name == "rmgrp")   return cmdRmgrp(cmd);
    if (cmd.name == "mkusr")   return cmdMkusr(cmd);
    if (cmd.name == "rmusr")   return cmdRmusr(cmd);
    if (cmd.name == "chgrp")   return cmdChgrp(cmd);
    if (cmd.name == "cat")     return cmdCat(cmd);
    if (cmd.name == "mkfile")  return cmdMkfile(cmd);
    if (cmd.name == "mkdir")   return cmdMkdir(cmd);
    if (cmd.name == "rep")     return cmdRep(cmd);

    return {false, "ERROR: comando \"" + cmd.name + "\" no reconocido"};
}

struct ScriptResult {
    std::string output;
    int ok = 0;
    int errors = 0;
};

// Procesa un script: varios comandos, uno por línea.
// Los comentarios y las líneas en blanco no cuentan como comando ejecutado.
ScriptResult runScript(const std::string& input) {
    ScriptResult result;
    std::ostringstream output;
    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            output << "\n";
            continue;
        }
        if (trimmed[0] == '#') {
            output << trimmed << "\n";
            continue;
        }

        ParsedCommand cmd = parseCommand(trimmed);
        if (!cmd.ok) {
            output << "ERROR: " << cmd.error << "\n";
            result.errors++;
            continue;
        }

        CmdResult res = dispatch(cmd);
        output << res.message << "\n";
        if (res.success) result.ok++; else result.errors++;
    }

    result.output = output.str();
    return result;
}

int main() {
    httplib::Server svr;

    // CORS abierto para que el frontend en React pueda llamar sin lío
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS") {
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ExtreamFS backend corriendo. POST /execute con {\"commands\": \"...\"}", "text/plain");
    });

    svr.Post("/execute", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string commands = body.value("commands", "");
            ScriptResult result = runScript(commands);

            json resp;
            resp["output"] = result.output;
            resp["ok"] = result.ok;
            resp["errors"] = result.errors;
            resp["total"] = result.ok + result.errors;
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            json err;
            err["error"] = std::string("Error procesando la petición: ") + e.what();
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    std::cout << "ExtreamFS backend escuchando en http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}