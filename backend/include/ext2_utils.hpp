#pragma once
#include <fstream>
#include <string>

template<typename T>
inline bool readStruct(const std::string& path, long offset, T& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(offset);
    file.read(reinterpret_cast<char*>(&out), sizeof(T));
    return file.good() || file.eof();
}

template<typename T>
inline bool writeStruct(const std::string& path, long offset, const T& in) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return false;
    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(&in), sizeof(T));
    return file.good();
}

// Escribe "count" bytes con el valor "value" a partir de offset (para bitmaps)
inline bool writeBytes(const std::string& path, long offset, char value, long count) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return false;
    file.seekp(offset);
    for (long i = 0; i < count; i++) file.put(value);
    return file.good();
}

inline bool readByte(const std::string& path, long offset, char& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(offset);
    file.read(&out, 1);
    return file.good() || file.eof();
}

inline bool writeByte(const std::string& path, long offset, char value) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) return false;
    file.seekp(offset);
    file.put(value);
    return file.good();
}