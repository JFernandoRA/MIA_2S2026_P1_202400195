#include "httplib.h"
#include "json.hpp"
#include "parser.hpp"
#include "commands.hpp"
#include <sstream>
#include <iostream>

using json = nlohmann::json;

// Ejecuta un comando ya parseado
std::string dispatch(const ParsedCommand& cmd) {
    if (cmd.name == "mkdisk")  return cmdMkdisk(cmd).message;
    if (cmd.name == "rmdisk")  return cmdRmdisk(cmd).message;
    if (cmd.name == "fdisk")   return cmdFdisk(cmd).message;
    if (cmd.name == "mount")   return cmdMount(cmd).message;
    if (cmd.name == "mounted") return cmdMounted(cmd).message;

    return "ERROR: comando \"" + cmd.name + "\" no reconocido";
}

// Procesa un script: varios comandos, uno por línea
std::string runScript(const std::string& input) {
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
            continue;
        }

        output << dispatch(cmd) << "\n";
    }

    return output.str();
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
            std::string output = runScript(commands);

            json resp;
            resp["output"] = output;
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