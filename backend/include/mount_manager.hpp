#pragma once
#include <string>
#include <map>

struct MountedPartition {
    std::string diskPath;
    std::string partitionName;
    int partitionIndex; // índice 0-3 dentro de mbr_partitions
};

// Estado en memoria (RAM). Se pierde al reiniciar el servidor, tal como pide el enunciado.
struct MountState {
    std::map<std::string, MountedPartition> mounted; // id -> info
    std::map<std::string, char> diskLetters;          // path del disco -> letra asignada (A, B, C...)
    std::map<std::string, int> diskNextNumber;         // path del disco -> siguiente número de partición
    char nextLetter = 'A';
};

inline MountState& mountState() {
    static MountState state;
    return state;
}

// Últimos dos dígitos del carnet, usados para el ID: carnet + numero + letra
inline std::string CARNET_SUFFIX = "95"; // carnet 202400195

// Genera el siguiente ID para montar una partición del disco indicado.
inline std::string generateMountId(const std::string& diskPath) {
    MountState& st = mountState();

    if (st.diskLetters.find(diskPath) == st.diskLetters.end()) {
        st.diskLetters[diskPath] = st.nextLetter;
        st.diskNextNumber[diskPath] = 1;
        st.nextLetter = (char)(st.nextLetter + 1);
    }

    char letter = st.diskLetters[diskPath];
    int number = st.diskNextNumber[diskPath];
    st.diskNextNumber[diskPath] = number + 1;

    return CARNET_SUFFIX + std::to_string(number) + letter;
}