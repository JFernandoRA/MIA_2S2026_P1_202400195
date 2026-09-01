#pragma once
#include <cstring>
#include <ctime>

#pragma pack(push, 1)

struct Superblock {
    int s_filesystem_type;
    int s_inodes_count;
    int s_blocks_count;
    int s_free_blocks_count;
    int s_free_inodes_count;
    time_t s_mtime;
    time_t s_umtime;
    int s_mnt_count;
    int s_magic;
    int s_inode_s;
    int s_block_s;
    int s_firts_ino;
    int s_first_blo;
    int s_bm_inode_start;
    int s_bm_block_start;
    int s_inode_start;
    int s_block_start;
};

struct Inode {
    int i_uid;
    int i_gid;
    int i_s;
    time_t i_atime;
    time_t i_ctime;
    time_t i_mtime;
    int i_block[15]; // 0-11 directos, 12 simple, 13 doble, 14 triple
    char i_type;      // '1' archivo, '0' carpeta
    char i_perm[3];   // ej. {'6','6','4'}

    Inode() {
        i_uid = 0; i_gid = 0; i_s = 0;
        i_atime = i_ctime = i_mtime = 0;
        for (int i = 0; i < 15; i++) i_block[i] = -1;
        i_type = '0';
        i_perm[0] = i_perm[1] = i_perm[2] = '0';
    }
};

struct FolderEntry {
    char b_name[12];
    int b_inodo;
};

struct FolderBlock { // 4 * (12 + 4) = 64 bytes
    FolderEntry b_content[4];
};

struct FileBlock { // 64 bytes
    char b_content[64];
};

struct PointerBlock { // 16 * 4 = 64 bytes
    int b_pointers[16];
};

#pragma pack(pop)