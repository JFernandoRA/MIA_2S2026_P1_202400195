#pragma once
#include <string>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include "users_file.hpp"

// Extrae la extensión del path (sin el punto), en minúsculas. "jpg" por defecto.
inline std::string extensionOf(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos == std::string::npos) return "jpg";
    std::string ext = path.substr(pos + 1);
    for (auto& c : ext) c = std::tolower(c);
    return ext;
}

// Escribe el .dot a un archivo temporal y llama a `dot` para generar la imagen final.
inline bool renderGraphviz(const std::string& dotContent, const std::string& outputPath) {
    std::string tmpPath = outputPath + ".dot";
    std::ofstream out(tmpPath);
    if (!out.is_open()) return false;
    out << dotContent;
    out.close();

    std::string ext = extensionOf(outputPath);
    std::string cmd = "dot -T" + ext + " \"" + tmpPath + "\" -o \"" + outputPath + "\" 2>/dev/null";
    int rc = std::system(cmd.c_str());
    std::remove(tmpPath.c_str());
    return rc == 0;
}

inline bool writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << content;
    return true;
}

inline std::string usernameFromUid(std::vector<UserRecord>& records, int uid) {
    for (auto& r : records) if (r.type == 'U' && r.id == uid) return r.user;
    return "?";
}

inline std::string groupnameFromId(std::vector<UserRecord>& records, int gid) {
    for (auto& r : records) if (r.type == 'G' && r.id == gid) return r.group;
    return "?";
}

inline std::string formatTime(time_t t) {
    if (t == 0) return "-";
    char buf[32];
    struct tm* tmv = localtime(&t);
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", tmv);
    return std::string(buf);
}