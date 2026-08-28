#pragma once
#include <cstring>
#include <ctime>

// pack(1): sin padding, para que el layout de bytes sea exacto en el .mia
#pragma pack(push, 1)

struct Partition {
    char part_status;      // '0' = inactiva/no creada, '1' = activa/montada... (ver notas)
    char part_type;        // 'P' = primaria, 'E' = extendida
    char part_fit;         // 'B','F','W'
    int  part_start;       // byte del disco donde inicia
    int  part_s;           // tamaño total en bytes
    char part_name[16];
    int  part_correlative; // -1 hasta que se monte
    char part_id[4];       // id generado al montar

    Partition() {
        part_status = '0';
        part_type = ' ';
        part_fit = 'W';
        part_start = -1;
        part_s = 0;
        memset(part_name, 0, sizeof(part_name));
        part_correlative = -1;
        memset(part_id, 0, sizeof(part_id));
    }
};

struct MBR {
    int   mbr_tamano;
    time_t mbr_fecha_creacion;
    int   mbr_dsk_signature;
    char  dsk_fit;             // 'B','F','W'
    Partition mbr_partitions[4];

    MBR() {
        mbr_tamano = 0;
        mbr_fecha_creacion = 0;
        mbr_dsk_signature = 0;
        dsk_fit = 'F';
    }
};

struct EBR {
    char part_mount; // '0' o '1'
    char part_fit;   // 'B','F','W'
    int  part_start;
    int  part_s;
    int  part_next;  // -1 si no hay siguiente
    char part_name[16];

    EBR() {
        part_mount = '0';
        part_fit = 'W';
        part_start = -1;
        part_s = 0;
        part_next = -1;
        memset(part_name, 0, sizeof(part_name));
    }
};

#pragma pack(pop)