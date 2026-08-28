#pragma once
#include "parser.hpp"
#include "structs.hpp"
#include "disk_utils.hpp"
#include "mount_manager.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstdlib>
#include <ctime>

struct CmdResult {
    bool success;
    std::string message;
};

// Crea las carpetas padre de "path", como mkdir -p
inline void ensureParentDirs(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return;
    std::string dir = path.substr(0, pos);
    std::string current;
    std::stringstream ss(dir);
    std::string part;
    if (!dir.empty() && dir[0] == '/') current = "/";
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        current += part + "/";
        mkdir(current.c_str(), 0755);
    }
}

inline bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

// ---------------------- MKDISK ----------------------
inline CmdResult cmdMkdisk(const ParsedCommand& cmd) {
    if (!hasParam(cmd, "size")) return {false, "MKDISK: falta el parámetro obligatorio -size"};
    if (!hasParam(cmd, "path")) return {false, "MKDISK: falta el parámetro obligatorio -path"};

    int sizeVal;
    try {
        sizeVal = std::stoi(getParam(cmd, "size"));
    } catch (...) {
        return {false, "MKDISK: -size debe ser un número"};
    }
    if (sizeVal <= 0) return {false, "MKDISK: -size debe ser positivo y mayor que cero"};

    std::string unit = toLower(getParam(cmd, "unit", "m"));
    if (unit != "k" && unit != "m") return {false, "MKDISK: -unit inválido, use K o M"};

    std::string fit = toLower(getParam(cmd, "fit", "ff"));
    char fitChar;
    if (fit == "bf") fitChar = 'B';
    else if (fit == "ff") fitChar = 'F';
    else if (fit == "wf") fitChar = 'W';
    else return {false, "MKDISK: -fit inválido, use BF, FF o WF"};

    std::string path = getParam(cmd, "path");

    long bytes = (unit == "k") ? (long)sizeVal * 1024L : (long)sizeVal * 1024L * 1024L;

    ensureParentDirs(path);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return {false, "MKDISK: no se pudo crear el archivo en " + path};

    char buffer[1024] = {0};
    long written = 0;
    while (written < bytes) {
        long toWrite = std::min((long)sizeof(buffer), bytes - written);
        file.write(buffer, toWrite);
        written += toWrite;
    }
    file.close();

    MBR mbr;
    mbr.mbr_tamano = (int)bytes;
    mbr.mbr_fecha_creacion = time(nullptr);
    mbr.mbr_dsk_signature = rand();
    mbr.dsk_fit = fitChar;

    if (!writeMBR(path, mbr)) return {false, "MKDISK: disco creado pero falló al escribir el MBR"};

    return {true, "MKDISK: disco creado correctamente en " + path + " (" + std::to_string(bytes) + " bytes)"};
}

// ---------------------- RMDISK ----------------------
inline CmdResult cmdRmdisk(const ParsedCommand& cmd) {
    if (!hasParam(cmd, "path")) return {false, "RMDISK: falta el parámetro obligatorio -path"};
    std::string path = getParam(cmd, "path");
    if (!fileExists(path)) return {false, "RMDISK: el archivo " + path + " no existe"};
    if (std::remove(path.c_str()) != 0) return {false, "RMDISK: no se pudo eliminar " + path};
    return {true, "RMDISK: disco " + path + " eliminado correctamente"};
}

// ---------------------- FDISK ----------------------
inline CmdResult cmdFdisk(const ParsedCommand& cmd) {
    if (!hasParam(cmd, "path")) return {false, "FDISK: falta el parámetro obligatorio -path"};
    if (!hasParam(cmd, "name")) return {false, "FDISK: falta el parámetro obligatorio -name"};

    std::string path = getParam(cmd, "path");
    if (!fileExists(path)) return {false, "FDISK: el disco " + path + " no existe"};

    std::string name = getParam(cmd, "name");

    std::string type = toLower(getParam(cmd, "type", "p"));
    char typeChar;
    if (type == "p") typeChar = 'P';
    else if (type == "e") typeChar = 'E';
    else if (type == "l") typeChar = 'L';
    else return {false, "FDISK: -type inválido, use P, E o L"};

    std::string fit = toLower(getParam(cmd, "fit", "wf"));
    char fitChar;
    if (fit == "bf") fitChar = 'B';
    else if (fit == "ff") fitChar = 'F';
    else if (fit == "wf") fitChar = 'W';
    else return {false, "FDISK: -fit inválido, use BF, FF o WF"};

    std::string unit = toLower(getParam(cmd, "unit", "k"));
    long unitMultiplier;
    if (unit == "b") unitMultiplier = 1;
    else if (unit == "k") unitMultiplier = 1024;
    else if (unit == "m") unitMultiplier = 1024L * 1024L;
    else return {false, "FDISK: -unit inválido, use B, K o M"};

    MBR mbr;
    if (!readMBR(path, mbr)) return {false, "FDISK: no se pudo leer el MBR de " + path};

    // Validar nombre no repetido entre particiones ya creadas en este disco
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_start != -1 &&
            std::string(mbr.mbr_partitions[i].part_name) == name) {
            return {false, "FDISK: ya existe una partición llamada \"" + name + "\" en este disco"};
        }
    }

    if (typeChar == 'L') {
        return {false, "FDISK: creación de particiones lógicas (-type=L) aún no implementada en este bloque"};
    }

    // -size es obligatorio al crear (P o E)
    if (!hasParam(cmd, "size")) return {false, "FDISK: falta el parámetro obligatorio -size"};
    int sizeVal;
    try {
        sizeVal = std::stoi(getParam(cmd, "size"));
    } catch (...) {
        return {false, "FDISK: -size debe ser un número"};
    }
    if (sizeVal <= 0) return {false, "FDISK: -size debe ser positivo y mayor que cero"};
    int neededBytes = (int)(sizeVal * unitMultiplier);

    // Contar particiones existentes (primarias + extendida) y validar restricciones
    int usedSlots = 0;
    bool hasExtended = false;
    int freeSlot = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_start != -1) {
            usedSlots++;
            if (mbr.mbr_partitions[i].part_type == 'E') hasExtended = true;
        } else if (freeSlot == -1) {
            freeSlot = i;
        }
    }
    if (usedSlots >= 4) return {false, "FDISK: el disco ya tiene el máximo de 4 particiones (primarias+extendida)"};
    if (typeChar == 'E' && hasExtended) return {false, "FDISK: ya existe una partición extendida en este disco"};
    if (freeSlot == -1) return {false, "FDISK: no hay slots de partición disponibles"};

    // Buscar espacio libre según el fit indicado
    auto gaps = findFreeGaps(mbr);
    int offset = chooseOffsetByFit(gaps, neededBytes, fitChar);
    if (offset == -1) return {false, "FDISK: no hay espacio suficiente en el disco para la partición solicitada"};

    Partition p;
    p.part_status = '0'; // aún no montada
    p.part_type = typeChar;
    p.part_fit = fitChar;
    p.part_start = offset;
    p.part_s = neededBytes;
    strncpy(p.part_name, name.c_str(), sizeof(p.part_name) - 1);
    p.part_correlative = -1;
    memset(p.part_id, 0, sizeof(p.part_id));

    mbr.mbr_partitions[freeSlot] = p;

    if (!writeMBR(path, mbr)) return {false, "FDISK: falló al escribir el MBR actualizado"};

    // Si es extendida, se escribe el primer EBR vacío
    if (typeChar == 'E') {
        EBR ebr;
        if (!writeEBR(path, offset, ebr)) {
            return {false, "FDISK: partición extendida creada pero falló al escribir el EBR inicial"};
        }
    }

    std::string tipoTexto = (typeChar == 'P') ? "primaria" : "extendida";
    return {true, "FDISK: partición " + tipoTexto + " \"" + name + "\" creada correctamente (" +
                  std::to_string(neededBytes) + " bytes, inicia en byte " + std::to_string(offset) + ")"};
}

// ---------------------- MOUNT ----------------------
inline CmdResult cmdMount(const ParsedCommand& cmd) {
    if (!hasParam(cmd, "path")) return {false, "MOUNT: falta el parámetro obligatorio -path"};
    if (!hasParam(cmd, "name")) return {false, "MOUNT: falta el parámetro obligatorio -name"};

    std::string path = getParam(cmd, "path");
    std::string name = getParam(cmd, "name");

    if (!fileExists(path)) return {false, "MOUNT: el disco " + path + " no existe"};

    MBR mbr;
    if (!readMBR(path, mbr)) return {false, "MOUNT: no se pudo leer el MBR de " + path};

    int foundIndex = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_start != -1 &&
            std::string(mbr.mbr_partitions[i].part_name) == name) {
            foundIndex = i;
            break;
        }
    }
    if (foundIndex == -1) return {false, "MOUNT: no existe una partición llamada \"" + name + "\" en " + path};

    Partition& p = mbr.mbr_partitions[foundIndex];
    if (p.part_type != 'P') {
        return {false, "MOUNT: solo se permite montar particiones primarias"};
    }

    std::string id = generateMountId(path);

    p.part_status = '1';
    MountState& st = mountState();
    p.part_correlative = st.diskNextNumber[path] - 1;
    strncpy(p.part_id, id.c_str(), sizeof(p.part_id));

    if (!writeMBR(path, mbr)) return {false, "MOUNT: falló al actualizar el MBR"};

    MountedPartition mp;
    mp.diskPath = path;
    mp.partitionName = name;
    mp.partitionIndex = foundIndex;
    st.mounted[id] = mp;

    return {true, "MOUNT: partición \"" + name + "\" montada con id " + id};
}

// ---------------------- MOUNTED ----------------------
inline CmdResult cmdMounted(const ParsedCommand& cmd) {
    MountState& st = mountState();
    if (st.mounted.empty()) return {true, "MOUNTED: no hay particiones montadas"};

    std::ostringstream oss;
    oss << "MOUNTED: ";
    bool first = true;
    for (auto& kv : st.mounted) {
        if (!first) oss << ", ";
        oss << kv.first;
        first = false;
    }
    return {true, oss.str()};
}