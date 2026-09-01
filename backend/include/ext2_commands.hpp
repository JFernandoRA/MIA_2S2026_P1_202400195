#pragma once
#include "parser.hpp"
#include "commands.hpp"
#include "ext2_structs.hpp"
#include "ext2_utils.hpp"
#include "mount_manager.hpp"
#include <cstring>

inline CmdResult cmdMkfs(const ParsedCommand& cmd) {
    if (!hasParam(cmd, "id")) return {false, "MKFS: falta el parámetro obligatorio -id"};
    std::string id = getParam(cmd, "id");

    MountState& st = mountState();
    auto it = st.mounted.find(id);
    if (it == st.mounted.end()) return {false, "MKFS: no existe una partición montada con id " + id};

    MountedPartition& mp = it->second;
    MBR mbr;
    if (!readMBR(mp.diskPath, mbr)) return {false, "MKFS: no se pudo leer el MBR del disco"};

    Partition& p = mbr.mbr_partitions[mp.partitionIndex];
    long partStart = p.part_start;
    long partSize = p.part_s;

    // n = floor((partSize - sizeOf(SB)) / (1 + 3 + sizeOf(inodo) + 3*64))
    long sbSize = sizeof(Superblock);
    long inodeSize = sizeof(Inode);
    long denom = 1 + 3 + inodeSize + 3 * 64;
    long n = (partSize - sbSize) / denom;
    if (n <= 0) return {false, "MKFS: la partición es demasiado pequeña para formatearse en EXT2"};

    long numInodes = n;
    long numBlocks = 3 * n;

    long bmInodeStart = partStart + sbSize;
    long bmBlockStart = bmInodeStart + numInodes;
    long inodeStart = bmBlockStart + numBlocks;
    long blockStart = inodeStart + numInodes * inodeSize;

    // Bitmaps: todo libre ('0') salvo los 2 primeros (root y users.txt)
    writeBytes(mp.diskPath, bmInodeStart, '0', numInodes);
    writeBytes(mp.diskPath, bmBlockStart, '0', numBlocks);
    writeByte(mp.diskPath, bmInodeStart + 0, '1');
    writeByte(mp.diskPath, bmInodeStart + 1, '1');
    writeByte(mp.diskPath, bmBlockStart + 0, '1');
    writeByte(mp.diskPath, bmBlockStart + 1, '1');

    time_t now = time(nullptr);

    // Inodo 0: carpeta raíz "/"
    Inode rootInode;
    rootInode.i_uid = 1;
    rootInode.i_gid = 1;
    rootInode.i_s = sizeof(FolderBlock);
    rootInode.i_atime = rootInode.i_ctime = rootInode.i_mtime = now;
    rootInode.i_block[0] = 0; // primer bloque de datos (índice relativo, 0-based)
    rootInode.i_type = '0';
    rootInode.i_perm[0] = '7'; rootInode.i_perm[1] = '7'; rootInode.i_perm[2] = '7';
    writeStruct(mp.diskPath, inodeStart + 0 * inodeSize, rootInode);

    // Bloque 0: contenido de la carpeta raíz (., .., users.txt -> inodo 1)
    FolderBlock rootBlock;
    for (auto& e : rootBlock.b_content) { memset(e.b_name, 0, sizeof(e.b_name)); e.b_inodo = -1; }
    strncpy(rootBlock.b_content[0].b_name, ".", sizeof(rootBlock.b_content[0].b_name) - 1);
    rootBlock.b_content[0].b_inodo = 0;
    strncpy(rootBlock.b_content[1].b_name, "..", sizeof(rootBlock.b_content[1].b_name) - 1);
    rootBlock.b_content[1].b_inodo = 0;
    strncpy(rootBlock.b_content[2].b_name, "users.txt", sizeof(rootBlock.b_content[2].b_name) - 1);
    rootBlock.b_content[2].b_inodo = 1;
    writeStruct(mp.diskPath, blockStart + 0 * 64, rootBlock);

    // Inodo 1: archivo users.txt
    std::string usersContent = "1, G, root\n1, U, root, root, 123\n";
    Inode usersInode;
    usersInode.i_uid = 1;
    usersInode.i_gid = 1;
    usersInode.i_s = (int)usersContent.size();
    usersInode.i_atime = usersInode.i_ctime = usersInode.i_mtime = now;
    usersInode.i_block[0] = 1;
    usersInode.i_type = '1';
    usersInode.i_perm[0] = '6'; usersInode.i_perm[1] = '6'; usersInode.i_perm[2] = '4';
    writeStruct(mp.diskPath, inodeStart + 1 * inodeSize, usersInode);

    // Bloque 1: contenido de users.txt
    FileBlock usersBlock;
    memset(usersBlock.b_content, 0, sizeof(usersBlock.b_content));
    memcpy(usersBlock.b_content, usersContent.c_str(), usersContent.size());
    writeStruct(mp.diskPath, blockStart + 1 * 64, usersBlock);

    // Superbloque
    Superblock sb;
    sb.s_filesystem_type = 2;
    sb.s_inodes_count = (int)numInodes;
    sb.s_blocks_count = (int)numBlocks;
    sb.s_free_blocks_count = (int)numBlocks - 2;
    sb.s_free_inodes_count = (int)numInodes - 2;
    sb.s_mtime = now;
    sb.s_umtime = 0;
    sb.s_mnt_count = 1;
    sb.s_magic = 0xEF53;
    sb.s_inode_s = (int)inodeSize;
    sb.s_block_s = 64;
    sb.s_firts_ino = 2;
    sb.s_first_blo = 2;
    sb.s_bm_inode_start = (int)bmInodeStart;
    sb.s_bm_block_start = (int)bmBlockStart;
    sb.s_inode_start = (int)inodeStart;
    sb.s_block_start = (int)blockStart;
    writeStruct(mp.diskPath, partStart, sb);

    return {true, "MKFS: partición formateada como EXT2 (" + std::to_string(numInodes) +
                  " inodos, " + std::to_string(numBlocks) + " bloques)"};
}