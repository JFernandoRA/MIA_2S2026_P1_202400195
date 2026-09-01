#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

struct UserRecord {
    int id;
    char type; // 'G' o 'U'
    std::string group;
    std::string user;  // solo para type 'U'
    std::string pass;  // solo para type 'U'
};

inline std::string trimSpaces(const std::string& s) {
    size_t a = s.find_first_not_of(' ');
    size_t b = s.find_last_not_of(' ');
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

inline std::vector<UserRecord> parseUsersFile(const std::string& content) {
    std::vector<UserRecord> records;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        std::vector<std::string> fields;
        std::stringstream ls(line);
        std::string field;
        while (std::getline(ls, field, ',')) fields.push_back(trimSpaces(field));
        if (fields.size() < 3) continue;

        UserRecord r;
        r.id = std::atoi(fields[0].c_str());
        r.type = fields[1].empty() ? ' ' : std::toupper(fields[1][0]);
        r.group = fields[2];
        if (r.type == 'U' && fields.size() >= 5) {
            r.user = fields[3];
            r.pass = fields[4];
        }
        records.push_back(r);
    }
    return records;
}

inline std::string serializeUsersFile(const std::vector<UserRecord>& records) {
    std::ostringstream oss;
    for (auto& r : records) {
        if (r.type == 'G') {
            oss << r.id << ", G, " << r.group << "\n";
        } else {
            oss << r.id << ", U, " << r.group << ", " << r.user << ", " << r.pass << "\n";
        }
    }
    return oss.str();
}

// Siguiente id a usar (los ids nunca se reutilizan, solo suben)
inline int nextId(const std::vector<UserRecord>& records) {
    int maxId = 0;
    for (auto& r : records) maxId = std::max(maxId, r.id);
    return maxId + 1;
}

inline UserRecord* findActiveGroup(std::vector<UserRecord>& records, const std::string& name) {
    for (auto& r : records) {
        if (r.type == 'G' && r.id != 0 && r.group == name) return &r;
    }
    return nullptr;
}

inline UserRecord* findActiveUser(std::vector<UserRecord>& records, const std::string& name) {
    for (auto& r : records) {
        if (r.type == 'U' && r.id != 0 && r.user == name) return &r;
    }
    return nullptr;
}