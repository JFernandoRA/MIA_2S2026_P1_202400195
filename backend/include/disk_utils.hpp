#pragma once
#include "structs.hpp"
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>

struct FreeGap {
    int start;
    int size;
};

// Lee el MBR desde el inicio del archivo de disco.
inline bool readMBR(const std::string& path, MBR& mbr) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    return file.good() || file.eof();
}

// Escribe el MBR al inicio del archivo de disco (sobreescribe, no cambia tamaño del archivo).
inline bool writeMBR(const std::string& path, const MBR& mbr) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return false;
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
    return file.good();
}

inline bool readEBR(const std::string& path, int offset, EBR& ebr) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(offset);
    file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
    return file.good() || file.eof();
}

inline bool writeEBR(const std::string& path, int offset, const EBR& ebr) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return false;
    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(&ebr), sizeof(EBR));
    return file.good();
}

// Calcula los huecos libres del disco fuera del MBR
inline std::vector<FreeGap> findFreeGaps(const MBR& mbr) {
    std::vector<FreeGap> gaps;
    std::vector<std::pair<int,int>> used; // start, size
    for (int i = 0; i < 4; i++) {
        const Partition& p = mbr.mbr_partitions[i];
        if (p.part_start != -1) {
            used.push_back({p.part_start, p.part_s});
        }
    }
    std::sort(used.begin(), used.end());

    int cursor = (int)sizeof(MBR);
    for (auto& u : used) {
        if (u.first > cursor) {
            gaps.push_back({cursor, u.first - cursor});
        }
        cursor = std::max(cursor, u.first + u.second);
    }
    if (cursor < mbr.mbr_tamano) {
        gaps.push_back({cursor, mbr.mbr_tamano - cursor});
    }
    return gaps;
}

// Aplica el fit (BF/FF/WF) y devuelve el offset elegido, o -1 si no alcanza
inline int chooseOffsetByFit(std::vector<FreeGap> gaps, int neededSize, char fit) {
    std::vector<FreeGap> candidates;
    for (auto& g : gaps) {
        if (g.size >= neededSize) candidates.push_back(g);
    }
    if (candidates.empty()) return -1;

    if (fit == 'F') { // First Fit
        return candidates.front().start;
    } else if (fit == 'B') { // Best Fit
        auto best = std::min_element(candidates.begin(), candidates.end(),
            [](const FreeGap& a, const FreeGap& b) { return a.size < b.size; });
        return best->start;
    } else { // Worst Fit
        auto worst = std::max_element(candidates.begin(), candidates.end(),
            [](const FreeGap& a, const FreeGap& b) { return a.size < b.size; });
        return worst->start;
    }
}