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
    std::string perr;
    if (!validateParams(cmd, {"size", "unit", "fit", "path"}, perr)) return {false, "MKDISK: " + perr};
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
    std::string perr;
    if (!validateParams(cmd, {"path"}, perr)) return {false, "RMDISK: " + perr};
    if (!hasParam(cmd, "path")) return {false, "RMDISK: falta el parámetro obligatorio -path"};
    std::string path = getParam(cmd, "path");
    if (!fileExists(path)) return {false, "RMDISK: el archivo " + path + " no existe"};
    if (std::remove(path.c_str()) != 0) return {false, "RMDISK: no se pudo eliminar " + path};
    return {true, "RMDISK: disco " + path + " eliminado correctamente"};
}

// ---------------------- FDISK ----------------------
inline CmdResult cmdFdisk(const ParsedCommand& cmd) {
    std::string perr;
    if (!validateParams(cmd, {"size", "unit", "path", "type", "fit", "name"}, perr)) return {false, "FDISK: " + perr};
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

    // Ubica la partición extendida del disco (si existe), para chequear nombres de
    // lógicas y, si type=L, para saber dónde encadenar la nueva lógica.
    int extIndex = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_start != -1 && mbr.mbr_partitions[i].part_type == 'E') extIndex = i;
    }

    // Validar nombre no repetido: entre particiones primarias/extendida...
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_start != -1 &&
            std::string(mbr.mbr_partitions[i].part_name) == name) {
            return {false, "FDISK: ya existe una partición llamada \"" + name + "\" en este disco"};
        }
    }
    // ...y entre las lógicas ya creadas dentro de la extendida (si existe).
    if (extIndex != -1) {
        long cursor = mbr.mbr_partitions[extIndex].part_start;
        while (cursor != -1) {
            EBR ebr;
            readEBR(path, cursor, ebr);
            if (ebr.part_start != -1 && std::string(ebr.part_name) == name) {
                return {false, "FDISK: ya existe una partición lógica llamada \"" + name + "\" en este disco"};
            }
            cursor = ebr.part_next;
        }
    }

    // -size es obligatorio al crear (P, E o L)
    if (!hasParam(cmd, "size")) return {false, "FDISK: falta el parámetro obligatorio -size"};
    int sizeVal;
    try {
        sizeVal = std::stoi(getParam(cmd, "size"));
    } catch (...) {
        return {false, "FDISK: -size debe ser un número"};
    }
    if (sizeVal <= 0) return {false, "FDISK: -size debe ser positivo y mayor que cero"};
    int neededBytes = (int)(sizeVal * unitMultiplier);

    if (typeChar == 'L') {
        if (extIndex == -1) return {false, "FDISK: no existe una partición extendida en este disco"};
        Partition& ext = mbr.mbr_partitions[extIndex];

        long cursor = ext.part_start;
        EBR ebr;
        readEBR(path, cursor, ebr);
        while (ebr.part_next != -1) {
            cursor = ebr.part_next;
            readEBR(path, cursor, ebr);
        }

        if (ebr.part_start == -1) {
            // Primer EBR de la extendida, todavía vacío: se llena en el mismo lugar.
            long dataStart = cursor + (long)sizeof(EBR);
            if (dataStart + neededBytes > ext.part_start + (long)ext.part_s)
                return {false, "FDISK: no hay espacio suficiente en la partición extendida"};
            ebr.part_mount = '0';
            ebr.part_fit = fitChar;
            ebr.part_start = dataStart;
            ebr.part_s = neededBytes;
            ebr.part_next = -1;
            memset(ebr.part_name, 0, sizeof(ebr.part_name));
            strncpy(ebr.part_name, name.c_str(), sizeof(ebr.part_name) - 1);
            writeEBR(path, cursor, ebr);
        } else {
            // Ya hay al menos una lógica: se agrega un nuevo EBR justo después de sus datos.
            long newEbrOffset = ebr.part_start + ebr.part_s;
            long newDataStart = newEbrOffset + (long)sizeof(EBR);
            if (newDataStart + neededBytes > ext.part_start + (long)ext.part_s)
                return {false, "FDISK: no hay espacio suficiente en la partición extendida"};

            EBR newEbr;
            newEbr.part_mount = '0';
            newEbr.part_fit = fitChar;
            newEbr.part_start = newDataStart;
            newEbr.part_s = neededBytes;
            newEbr.part_next = -1;
            memset(newEbr.part_name, 0, sizeof(newEbr.part_name));
            strncpy(newEbr.part_name, name.c_str(), sizeof(newEbr.part_name) - 1);
            writeEBR(path, newEbrOffset, newEbr);

            ebr.part_next = newEbrOffset;
            writeEBR(path, cursor, ebr);
        }

        return {true, "FDISK: partición lógica \"" + name + "\" creada correctamente (" +
                      std::to_string(neededBytes) + " bytes)"};
    }

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
    std::string perr;
    if (!validateParams(cmd, {"path", "name"}, perr)) return {false, "MOUNT: " + perr};
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

    MountState& st = mountState();
    for (auto& kv : st.mounted) {
        if (kv.second.diskPath == path && kv.second.partitionIndex == foundIndex) {
            return {false, "MOUNT: la partición \"" + name + "\" ya está montada (id " + kv.first + ")"};
        }
    }

    std::string id = generateMountId(path);

    p.part_status = '1';
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
    std::string perr;
    if (!validateParams(cmd, {}, perr)) return {false, "MOUNTED: " + perr};
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