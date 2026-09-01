#pragma once
#include <string>

struct Session {
    bool active = false;
    std::string user;
    int uid = -1;
    int gid = -1;
    std::string groupName;
    std::string partitionId; // id de la partición montada sobre la que opera la sesión
};

inline Session& currentSession() {
    static Session s;
    return s;
}