#pragma once
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>

struct ParsedCommand {
    std::string name;                       // comando en minúsculas, ej "mkdisk"
    std::map<std::string, std::string> params; // claves en minúsculas -> valor tal cual (case-sensitive en el valor)
    bool ok = true;
    std::string error;
};

inline std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
    return r;
}

inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Tokeniza respetando comillas dobles: "valor con espacios" cuenta como un solo token.
inline std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
            continue; // no incluimos las comillas en el token
        }
        if (std::isspace((unsigned char)c) && !inQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Parsea una línea de comando tipo: mkdisk -size=3000 -unit=K -path="/mi carpeta/d.mia"
inline ParsedCommand parseCommand(const std::string& rawLine) {
    ParsedCommand result;
    std::string line = trim(rawLine);

    if (line.empty() || line[0] == '#') {
        result.ok = false; // línea vacía o comentario: se maneja aparte, no es un error real
        result.name = line[0] == '#' ? "#comment" : "#blank";
        result.error = line;
        return result;
    }

    std::vector<std::string> tokens = tokenize(line);
    if (tokens.empty()) {
        result.ok = false;
        result.name = "#blank";
        return result;
    }

    result.name = toLower(tokens[0]);

    for (size_t i = 1; i < tokens.size(); i++) {
        const std::string& tok = tokens[i];
        // formato esperado: -key=value  (puede venir con mayúsculas: -Size=300)
        size_t dashPos = tok.find('-');
        if (dashPos == std::string::npos || dashPos != 0) {
            result.ok = false;
            result.error = "Parámetro mal formado: \"" + tok + "\" (se esperaba -clave=valor)";
            return result;
        }
        std::string body = tok.substr(1); // quita el '-'
        size_t eqPos = body.find('=');
        if (eqPos == std::string::npos) {
            // bandera sin valor, ej. -r o -p
            result.params[toLower(body)] = "";
            continue;
        }
        std::string key = toLower(body.substr(0, eqPos));
        std::string value = body.substr(eqPos + 1);
        result.params[key] = value;
    }

    return result;
}

// Helpers de acceso a parámetros
inline bool hasParam(const ParsedCommand& cmd, const std::string& key) {
    return cmd.params.find(key) != cmd.params.end();
}

inline std::string getParam(const ParsedCommand& cmd, const std::string& key, const std::string& def = "") {
    auto it = cmd.params.find(key);
    return it != cmd.params.end() ? it->second : def;
}