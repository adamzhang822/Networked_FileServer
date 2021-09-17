#include <stdio.h>
#include <string.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)
int FD_FILESYSTEM;

// ***----------------Initialization ----------------*** //
int MFS_File_System_Init(char* filename) {

    int fd_exist = open(filename, O_RDWR);
    if (fd_exist > 0) {
        return fd_exist;
    }

    // file system image does not exist, creat new file system image, initialize
    // and push it to disk. 
    int fd = open(filename, O_CREAT|O_RDWR);

    char* bitmap_inodes = malloc(MFS_INODE_TABLE_SIZE);
    bzero(bitmap_inodes, sizeof(MFS_INODE_TABLE_SIZE));
    bitmap_inodes[0] = 1;
    write(fd, bitmap_inodes, MFS_INODE_TABLE_SIZE);
    free(bitmap_inodes);

    char* bitmap_blocks = malloc(MFS_DATA_REGION_SIZE);
    bzero(bitmap_blocks, sizeof(MFS_DATA_REGION_SIZE));
    bitmap_blocks[0] = 1;
    write(fd, bitmap_blocks, MFS_DATA_REGION_SIZE);
    free(bitmap_blocks);

    MFS_Inode_t* inode_table = malloc(sizeof(MFS_Inode_t) * MFS_INODE_TABLE_SIZE);
    bzero(inode_table, sizeof(MFS_Inode_t) * MFS_INODE_TABLE_SIZE);
    for (int i = 0; i < MFS_INODE_TABLE_SIZE; i++) {
        MFS_Inode_t* current_inode = &inode_table[i];
        for (int j = 0; j < 10; j++) {
            current_inode->blocks[j] = -1; // initially all block pointers points to invalid blocks
        }
    }
    MFS_Inode_t* directory_node = &inode_table[0];
    directory_node->type = 0; // directory 
    directory_node->size = sizeof(MFS_DirEnt_t) * 2; // "." and ".." entries
    directory_node->real_size = sizeof(MFS_DirEnt_t) * 2; // same as size
    directory_node->block_nums = 1; // 1 blocks needed for 2 entries
    directory_node->blocks[0] = 0; // first block is used
    write(fd, (char*)inode_table,sizeof(MFS_Inode_t) * MFS_INODE_TABLE_SIZE);
    free(inode_table);

    MFS_Data_Block_t* data_region = malloc(sizeof(MFS_Data_Block_t) * MFS_DATA_REGION_SIZE);
    bzero(data_region, sizeof(MFS_Data_Block_t) * MFS_DATA_REGION_SIZE);
    MFS_DirEnt_t root_self;
    MFS_DirEnt_t root_parent;
    root_self.inum = 0;
    strcpy(root_self.name, ".");
    root_parent.inum = 0;
    strcpy(root_parent.name, "..");
    memcpy(&data_region[0], &root_self, sizeof(MFS_DirEnt_t));
    memcpy((char*)&data_region[0]+sizeof(MFS_DirEnt_t), &root_parent, sizeof(MFS_DirEnt_t));
    write(fd, (char*) data_region, sizeof(MFS_Data_Block_t) * MFS_DATA_REGION_SIZE);
    free(data_region);
    return fd;
}

// ***---------------- Loading data structures from file ----------------*** //


void load_inode_bitmap(char* inode_bitmap){
    char buffer[MFS_INODE_TABLE_SIZE];
    lseek(FD_FILESYSTEM, 0, SEEK_SET);
    read(FD_FILESYSTEM, buffer,MFS_INODE_TABLE_SIZE);
    memcpy(inode_bitmap, buffer, MFS_INODE_TABLE_SIZE);
    return;
}

void load_data_bitmap(char* data_bitmap){
    char buffer[MFS_DATA_REGION_SIZE];
    lseek(FD_FILESYSTEM, MFS_INODE_TABLE_SIZE, SEEK_SET);
    read(FD_FILESYSTEM, buffer, MFS_DATA_REGION_SIZE);
    memcpy(data_bitmap, buffer, MFS_DATA_REGION_SIZE);
    return;
}

void load_inode(int pinum, MFS_Inode_t* inode_holder){
    char buffer[sizeof(MFS_Inode_t)];
    int inode_table_start = MFS_INODE_TABLE_SIZE + MFS_DATA_REGION_SIZE;
    int inode_table_offset = pinum * sizeof(MFS_Inode_t);
    int offset = inode_table_start + inode_table_offset;
    lseek(FD_FILESYSTEM, offset, SEEK_SET);
    read(FD_FILESYSTEM, buffer, sizeof(MFS_Inode_t));
    memcpy(inode_holder, buffer, sizeof(MFS_Inode_t));
    return;
}

// ***---------------- General functions for sending and receiving message to/from client ----------------*** //


int Server_Receive_Message(MFS_Message_t* message, int sd, struct sockaddr_in* receipt_sockaddr){
    char* message_buffer = malloc(sizeof(MFS_Message_t));
    bzero(message_buffer, sizeof(MFS_Message_t));
    int read_status = UDP_Read(sd, receipt_sockaddr, message_buffer, sizeof(MFS_Message_t));
    if (read_status > 0) {
        memcpy(message, message_buffer, sizeof(MFS_Message_t));
    }
    free(message_buffer);
    return read_status;
}

int Server_Send_Reply(MFS_Reply_t* reply, int sd, struct sockaddr_in* receipt_sockaddr) {
    char* reply_buffer = malloc(sizeof(MFS_Reply_t));
    bzero(reply_buffer, sizeof(MFS_Reply_t));
    memcpy(reply_buffer, reply, sizeof(MFS_Reply_t));
    int write_status = UDP_Write(sd, receipt_sockaddr, reply_buffer, sizeof(MFS_Reply_t));
    free(reply_buffer);
    free(reply);
    return write_status;
}

// ***---------------- Helper functions for crafting reply message ----------------*** //

int valid_inum(MFS_Message_t* message){
    int inum = message->inum;
    if ( (inum < 0) || (inum > MFS_INODE_TABLE_SIZE)) {
        return -1;
    }
    char inode_bitmap[MFS_INODE_TABLE_SIZE];
    load_inode_bitmap(inode_bitmap);
    if (inode_bitmap[inum] == 1) {
        return inum;
    } else {
        return -1;
    }
}

int valid_block(int inum, MFS_Message_t* message){
    // check that block table index is valid 
    int block_table_index = message->block;
    if ( (block_table_index < 0) || (block_table_index >= 10)) {
        return -1;
    }
    //printf("SERVER:: Checked that the block_table_index is valid \n");

    MFS_Inode_t target_inode;
    load_inode(inum, &target_inode);
    int data_region_offset = target_inode.blocks[block_table_index];
    if ( (data_region_offset < 0) || (data_region_offset > MFS_DATA_REGION_SIZE)){
        //printf("SERVER:; The data_region_offset is not valid, it is: %d\n",data_region_offset);
        return -1;
    }
    //printf("SERVER:: Checked that the block_table entry is valid \n");

    char data_bitmap[MFS_DATA_REGION_SIZE];
    load_data_bitmap(data_bitmap);
    if (data_bitmap[data_region_offset] == 1) {
        //printf("SERVER:; The data_region_offset at bitmap is 1, so valid! \n");
        return data_region_offset;
    } else {
        //printf("SERVER:; The data region offset at bitmap is 0, not valid!\n");
        return -1;
    }
}

int match_DirEnt(int offset, char* name){
    //printf("SERVER:: trying to match directory, the name to be searched is: %s \n", name);
    lseek(FD_FILESYSTEM, offset, SEEK_SET);
    MFS_DirEnt_t DirEnt;
    bzero(&DirEnt, sizeof(MFS_DirEnt_t));
    char buffer[sizeof(MFS_DirEnt_t)];
    bzero(buffer, sizeof(MFS_DirEnt_t));
    read(FD_FILESYSTEM, buffer, sizeof(MFS_DirEnt_t));
    memcpy(&DirEnt, buffer, sizeof(MFS_DirEnt_t));
    //printf("SERVER:: DirEnt read in successfully, \n");

    char name_stored[252];
    strcpy(name_stored, DirEnt.name);
    //printf("SERVER::matching dirent name now, the name read is: %s \n", name_stored);
    
    if(!strcmp(name_stored, name)){
        //printf("SERVER:: Matched!!! the inum is: %d \n", DirEnt.inum);
        return DirEnt.inum; // a match 
    }
    return -1; // not a match
}

int invalid_DirEnt(int offset){
    lseek(FD_FILESYSTEM, offset, SEEK_SET);
    MFS_DirEnt_t DirEnt;
    bzero(&DirEnt, sizeof(MFS_DirEnt_t));
    char buffer[sizeof(MFS_DirEnt_t)];
    bzero(buffer, sizeof(MFS_DirEnt_t));
    read(FD_FILESYSTEM, buffer, sizeof(MFS_DirEnt_t));
    memcpy(&DirEnt, buffer, sizeof(MFS_DirEnt_t));
    return DirEnt.inum;
}

int search_Directory(MFS_Inode_t* inode, char* name, int* entry_offset){
    int num_entries = (inode->size) / sizeof(MFS_DirEnt_t);
    if (num_entries == 0) {
        return -1;
    }
    //printf("SERVER:: In search directory, the num_entries is: %d \n", num_entries);
    int data_region_offset = 4096 + 4096 + sizeof(MFS_Inode_t) * MFS_INODE_TABLE_SIZE;
    for (int i = 0; i < num_entries; i++) {
        int block_num = i / 16; // 16 entries per block
        int block_offset = inode->blocks[block_num];
        int within_block_offset = i % 16;
        int offset = data_region_offset + block_offset * sizeof(MFS_Data_Block_t) + within_block_offset * sizeof(MFS_DirEnt_t);
        int match = match_DirEnt(offset, name);
        if(match >= 0) {
            if (entry_offset) {
                *entry_offset = offset;
            }
            return match;
        }
    }
    return -1; // match failed
}

int empty_Directory(MFS_Inode_t* inode) {
    int num_entries = (inode->size) / sizeof(MFS_DirEnt_t);
    if (num_entries == 2) {
        return 1;
    }

    int valid_inum;
    int data_region_offset = 4096 + 4096 + sizeof(MFS_Inode_t) * MFS_INODE_TABLE_SIZE;
    for (int i = 2; i < num_entries; i++) {
        int block_num = i /16; // 16 entries per block
        int block_offset = inode->blocks[block_num];
        int within_block_offset = i % 16;
        int offset = data_region_offset + block_offset * sizeof(MFS_Data_Block_t) + within_block_offset *sizeof(MFS_DirEnt_t);
        valid_inum = invalid_DirEnt(offset);
        if (valid_inum >= 0) {
            return 0;
        }
    }

    return 1;
}

int next_available_block(int inplace, char* bitmap_inplace){

    if (inplace) {
        //printf("SERVER:: Finding next available block in place \n");
        for(int i = 0; i < MFS_DATA_REGION_SIZE; i++) {
            if (bitmap_inplace[i] == 0) {
                bitmap_inplace[i] = 1;
                return i;
            }
        }
        return -1; //all blocks are occupie
    }

    char data_bitmap[MFS_DATA_REGION_SIZE];
    load_data_bitmap(data_bitmap);
    for (int i = 0; i < MFS_DATA_REGION_SIZE; i++) {
        if (data_bitmap[i] == 0) {
            //printf("SERVER:; Found unoccupied block: %d\n", i);
            return i;
        }
    }
    return -1; //all blocks are occupied
}

int next_available_inode(int inplace, char* bitmap_inplace){

    if (inplace) {
        for (int i = 0; i<MFS_INODE_TABLE_SIZE; i++) {
            if (bitmap_inplace[i] == 0) {
                bitmap_inplace[i] = 1;
                return i;
            }
        }
        return -1; //all blocks are occupied
    }
    char inode_bitmap[MFS_INODE_TABLE_SIZE];
    load_inode_bitmap(inode_bitmap);
    for (int i = 0; i <MFS_INODE_TABLE_SIZE; i++) {
        if (inode_bitmap[i] == 0) {
            return i;
        }
    }
    return -1; //all blocks are occupied
}

void decomission_file(int inum, MFS_Inode_t* target_inode, char* inode_bitmap, char* data_bitmap) {
    inode_bitmap[inum] = 0;
    for (int i = 0; i < 10; i++) {
        int block_offset = target_inode->blocks[i];
        if (block_offset >= 0) {
            data_bitmap[block_offset] = 0;
        }
        target_inode->blocks[i] = -1;
    }
    return;
}

void push_inode_update(MFS_Inode_t* target_inode, int inum, int block_table_offset, int data_region_offset){
    target_inode->blocks[block_table_offset] = data_region_offset;
    target_inode->block_nums = target_inode->block_nums + 1;
    target_inode->size += BUFFER_SIZE;
    if (! target_inode->type) {
        target_inode->real_size += sizeof(MFS_DirEnt_t);
    }

    char buffer[sizeof(MFS_Inode_t)];
    memcpy(buffer, target_inode, sizeof(MFS_Inode_t));

    int offset = MFS_INODE_TABLE_SIZE + MFS_DATA_REGION_SIZE + sizeof(MFS_Inode_t) * inum;
    lseek(FD_FILESYSTEM, offset, SEEK_SET);
    int write_status = write(FD_FILESYSTEM, buffer, sizeof(MFS_Inode_t));
    assert(write_status >= 0);
}

void push_inode_bitmap_update(int inum) {
    lseek(FD_FILESYSTEM, inum, SEEK_SET);
    char onebyte = 1;
    int write_status = write(FD_FILESYSTEM, &onebyte, 1);
    assert(write_status == 1);
}

void push_data_bitmap_update(int data_block_num){
    lseek(FD_FILESYSTEM, 4096+data_block_num, SEEK_SET);
    char onebyte = 1;
    int write_status = write(FD_FILESYSTEM, &onebyte, 1);
    assert(write_status == 1);
}


// ***---------------- Crafting reply for each request type ----------------*** //

int reply_Init(MFS_Reply_t* reply, int sd, struct sockaddr_in* receipt_sockaddr){
    reply->reply_type = 'I';
    reply->success = 0;
    int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
    return write_status;
}

int reply_Lookup(MFS_Message_t* message, MFS_Reply_t* reply, int sd, struct sockaddr_in* receipt_sockaddr) {
    //printf("SERVER:: Gonna be replying to lookup \n");
    reply->reply_type = 'L';

    int pinum = message->inum;
    char* name = malloc(252);
    strcpy(name, message->name);
    //printf("SERVER:: Replying to lookup, the name to search for is: %s \n", name);
    
    char inode_bitmap[MFS_INODE_TABLE_SIZE];
    load_inode_bitmap(inode_bitmap);

    if (inode_bitmap[pinum] == 0) {
        free(name);
        //printf("SERVER:: pinum not occupied, invalid inum! \n");
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: checked inode bitmap valid \n");

    MFS_Inode_t target_inode;
    load_inode(pinum, &target_inode);
    int file_type = target_inode.type;
    if (file_type == 1) {
        free(name);
        //printf("SERVER:: file type is not directory, invalid! \n");
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:; Checked file type valid \n");

    int match = search_Directory(&target_inode, name, NULL);
    free(name);
    if(match >= 0) {
        reply->success = match;
        //printf("SERVER:: Lookup was a success, sending reply now \n");
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }else{
        reply->success = -1;
        //printf("SERVER:: Lookup failed! \n");
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
}


int reply_Stat(MFS_Message_t* message, MFS_Reply_t* reply, int sd, struct sockaddr_in* receipt_sockaddr){
    //printf("SERVER:; Gonna reply to Stat \n");
    reply->reply_type = 'S';

    // check that inum is valid 
    int inum = message->inum;
    char inode_bitmap[MFS_INODE_TABLE_SIZE];
    load_inode_bitmap(inode_bitmap);
    if (inode_bitmap[inum] == 0) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Checked that inum is valid \n");

    // load stats info 
    MFS_Inode_t target_inode;
    load_inode(inum, &target_inode);
    reply->stats.type = target_inode.type;
    reply->stats.size = target_inode.size;
    if (! reply->stats.type) {
        reply->stats.size = target_inode.real_size;
    }
    reply->stats.blocks = target_inode.block_nums;
    //printf("SERVER:: Loaded stats into reply \n");
    int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
    return write_status;
}


int reply_Read(MFS_Message_t* message, MFS_Reply_t* reply, int sd, struct sockaddr_in* receipt_sockaddr) {
    //printf("SERVER:; Gonna reply to Read \n");
    reply->reply_type = 'R';

    // check that inum is valid
    int inum = valid_inum(message);
    if (inum < 0) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: checked that inum is valid, and the inum is: %d\n", inum);

    // check that block is valid
    int data_region_offset = valid_block(inum, message);
    if (data_region_offset < 0) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Checked that block is valid, and the block is: %d\n", data_region_offset);

    // inum and block both valid, so load data to buffer
    int data_region_start = MFS_INODE_TABLE_SIZE + MFS_DATA_REGION_SIZE + sizeof(MFS_Inode_t) * MFS_INODE_TABLE_SIZE;
    int offset = data_region_start + sizeof(MFS_Data_Block_t) * data_region_offset;
    lseek(FD_FILESYSTEM, offset, SEEK_SET);
    read(FD_FILESYSTEM, reply->buffer, BUFFER_SIZE);
    int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
    return write_status;
}

int reply_Write(MFS_Message_t* message, MFS_Reply_t* reply, int sd, struct sockaddr_in* receipt_sockaddr){
    //printf("SERVER:: Gonna write to file based on message \n");
    reply->reply_type = 'W';

    //check that inum is valid
    int inum = valid_inum(message);
    if (inum < 0) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: checked inum valid \n");

    //check that file type is valid
    MFS_Inode_t target_inode;
    load_inode(inum, &target_inode);
    if (target_inode.type == MFS_DIRECTORY) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: checked file type is valid \n");

    //check that block table offset is valid
    int block_table_offset = message->block;
    if (block_table_offset < 0 || block_table_offset > 10) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:; checked block offset is valid \n");

    // everything is valid, write buffer to the file and flush to disk
    int new_block_written = 0; // flag, if 1, then new block is allocated and used
    int data_region_offset = target_inode.blocks[block_table_offset];
    //printf("SERVER:: Before anything, data region offset is: %d\n",data_region_offset);
    if (data_region_offset == -1) {
        // new data block needs to be allocated
        //printf("SERVER:: Determined 'W' instruction needs new block \n");
        data_region_offset = next_available_block(0, NULL);
        if (data_region_offset == -1) {
            //printf("SERVER:: All data blocks are full already! \n");
            // all data blocks are occupied
            reply->success = -1;
            int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
            return write_status;
        }
        new_block_written = 1;
    } 
    //printf("SERVER:: Gonna write to a new block, the in region offset is: %d\n", data_region_offset);

    int data_region_start = MFS_INODE_TABLE_SIZE + MFS_DATA_REGION_SIZE + MFS_INODE_TABLE_SIZE * sizeof(MFS_Inode_t);
    int offset = data_region_start + MFS_BLOCK_SIZE * data_region_offset;
    lseek(FD_FILESYSTEM, offset, SEEK_SET);
    int write_to_file = write(FD_FILESYSTEM, message->buffer, BUFFER_SIZE);
    assert(write_to_file >= 0);

    if (new_block_written) {
        //printf("SERVER:: New block written so pushing those updates \n");
        push_inode_update(&target_inode, inum, block_table_offset, data_region_offset);
        push_data_bitmap_update(data_region_offset);
    }

    int synced = fsync(FD_FILESYSTEM);
    assert(synced != -1);
    int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
    return write_status;
}

int reply_Creat(MFS_Message_t* message, MFS_Reply_t* reply, int sd, struct sockaddr_in* receipt_sockaddr){
    //printf("SERVER:: Gonna reply to Creat \n");
    reply->reply_type = 'C';

    //check that inum is valid 
    // 1. inode is occupied 
    int inum = valid_inum(message);
    if (inum < 0) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:; Checked that inum is valid \n");

    // 2. inode is for directory
    MFS_Inode_t target_inode;
    load_inode(inum, &target_inode);
    if ( (target_inode.type == 1) ) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Checked that inum is for directory \n");

    // Check if name already exist
    int already_exists = search_Directory(&target_inode, message->name, NULL);
    if (already_exists != (-1)) {
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Checked that name does not already exist \n");

    // does not exist, need to creat new directory entry
    // 3. check if directory is full
    if (target_inode.size == (MFS_INODE_BLOCKS_SIZE * MFS_BLOCK_SIZE)) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Checked that directory is not full \n");

    /* -----------------------------------------------------------------------
    Writing to file:
    1. Check if there is inode table space
    2. Create the new directory entry data structure 
    3. Update target_inode (directory inode) accordingly 
    4. Create new inode for the new directory / file
    (4.5) If new file, initialize the data block pointers to -1
    5. Create default directory entries if new directory 
    6. Flush everything
    */

    /* Step 1: Check for inode space */
    char inode_bitmap[MFS_INODE_TABLE_SIZE];
    load_inode_bitmap(inode_bitmap);
    int new_inum = next_available_inode(1, inode_bitmap);
    //printf("SERVER:: Next available inode is %d! \n", new_inum);
    if (new_inum < 0) {
        reply->success = -1; //inode table is full
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Checked that inode space is not full! \n");
    
    /* Step 2: Create new directory entry data structure*/
    MFS_DirEnt_t new_entry;
    bzero(&new_entry, sizeof(MFS_DirEnt_t));
    new_entry.inum = new_inum;
    strcpy(new_entry.name, message->name);
    
    /* Step 3: Update parent inode accordingly */
    char data_bitmap[MFS_DATA_REGION_SIZE];
    load_data_bitmap(data_bitmap);
    int current_size = target_inode.size;
    int num_entries = current_size / sizeof(MFS_DirEnt_t);
    int new_block_needed = ((num_entries + 1) > 16 * target_inode.block_nums);
    if (new_block_needed) {
        int new_block_num_for_dirent = next_available_block(1, data_bitmap);
        target_inode.blocks[target_inode.block_nums] = new_block_num_for_dirent;
        target_inode.block_nums++;
    }
    target_inode.size = target_inode.size + sizeof(MFS_DirEnt_t);
    target_inode.real_size = target_inode.real_size + sizeof(MFS_DirEnt_t);

    /* Step 4: Create new inode entry */
    MFS_Inode_t new_inode;
    bzero(&new_inode, sizeof(MFS_Inode_t));
    int type = message->type;
    new_inode.type = type;
    for (int i = 0; i < MFS_INODE_BLOCKS_SIZE; i++) {
        new_inode.blocks[i] = -1;
    }
    int new_block_for_file = 0;
    if (!type) {
        // this is a directory file, by default has two entries
        new_inode.size = 2 * sizeof(MFS_DirEnt_t);
        new_inode.real_size = 2 * sizeof(MFS_DirEnt_t);
        new_inode.block_nums = 1;
        new_block_for_file = next_available_block(1, data_bitmap);
        new_inode.blocks[0] = new_block_for_file;
    } 

    /* Step 5 : Create default directory entries for new directory */
    MFS_DirEnt_t new_self_ent;
    new_self_ent.inum = new_inum;
    strcpy(new_self_ent.name, ".");
    MFS_DirEnt_t new_parent_ent;
    new_parent_ent.inum = message->inum;
    strcpy(new_parent_ent.name, "..");

    /* Step 6: Flush everything */
    int data_region_start = 4096 + 4096 + sizeof(MFS_Inode_t) * 4096; 

    lseek(FD_FILESYSTEM, 0, SEEK_SET);
    write(FD_FILESYSTEM, inode_bitmap, MFS_INODE_TABLE_SIZE);
    lseek(FD_FILESYSTEM, MFS_INODE_TABLE_SIZE, SEEK_SET);
    write(FD_FILESYSTEM, data_bitmap, MFS_DATA_REGION_SIZE);
    lseek(FD_FILESYSTEM, 4096 + 4096 + sizeof(MFS_Inode_t) * inum, SEEK_SET);
    write(FD_FILESYSTEM, &target_inode, sizeof(MFS_Inode_t));

    if (new_block_needed) {
        int data_region_offset = sizeof(MFS_Data_Block_t) * target_inode.blocks[target_inode.block_nums - 1];
        int offset = data_region_start + data_region_offset;
        lseek(FD_FILESYSTEM, offset, SEEK_SET);
        write(FD_FILESYSTEM, &new_entry, sizeof(MFS_DirEnt_t));
    } else {
        int within_block_offset = num_entries % 16;
        int data_region_offset = target_inode.blocks[target_inode.block_nums - 1];
        int offset = data_region_start + data_region_offset * 4096 + within_block_offset * sizeof(MFS_DirEnt_t);
        lseek(FD_FILESYSTEM, offset, SEEK_SET);
        write(FD_FILESYSTEM, &new_entry, sizeof(MFS_DirEnt_t));
    }

    lseek(FD_FILESYSTEM, 4096 + 4096 + sizeof(MFS_Inode_t) * new_inum, SEEK_SET);
    write(FD_FILESYSTEM, &new_inode, sizeof(MFS_Inode_t));

    if (!type) {
        char buffer[sizeof(MFS_DirEnt_t) * 2];
        memcpy(buffer, &new_self_ent, sizeof(MFS_DirEnt_t));
        memcpy(buffer + sizeof(MFS_DirEnt_t), &new_parent_ent, sizeof(MFS_DirEnt_t));
        int offset = data_region_start + sizeof(MFS_Data_Block_t) * new_block_for_file;
        lseek(FD_FILESYSTEM, offset, sizeof(MFS_DirEnt_t) * 2);
        write(FD_FILESYSTEM, buffer, sizeof(MFS_DirEnt_t));
    }    

    int synced = fsync(FD_FILESYSTEM);
    assert(synced == 0);
    //printf("SERVER:: Creat should be successful, flushed everything! \n");

    /* Send final reply after flushing everything */
    int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
    return write_status;
}

int reply_Unlink(MFS_Message_t* message, MFS_Reply_t* reply, int sd, struct sockaddr_in* receipt_sockaddr){
    //printf("SERVER:: Gonna reply to Unlink \n");
    reply->reply_type = 'U';

    // check pinum exist
    int pinum = valid_inum(message);
    if (pinum < 0) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Unlink, checked pinum exist \n");

    // check pinum represents a directory
    MFS_Inode_t target_inode;
    load_inode(pinum, &target_inode);
    if (target_inode.type) {
        reply->success = -1;
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Checked that pinum represents a directory \n");

    // if name does not exist in directory, just return success and exit 
    int* entry_offset = malloc(sizeof(int));
    int matched_inum = search_Directory(&target_inode, message->name, entry_offset);
    if (matched_inum < 0) {
       //printf("SERVER:: Name does not exist in directory \n");
        int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
        return write_status;
    }
    //printf("SERVER:: Name does exist in the directory, the offset is at: %d \n", *entry_offset);

    // check that the unlinked directory is not empty
    MFS_Inode_t unlinked_inode;
    load_inode(matched_inum, &unlinked_inode);
    if (!unlinked_inode.type){
        int isEmpty = empty_Directory(&unlinked_inode);
        if (!isEmpty) { //directory is not empty
            reply->success = -1;
            int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
            return write_status;
        }
    }
    //printf("SERVER:: Checked that unlinked directory is empty \n");

    // everything is valid so far, prepare to unlink
    MFS_DirEnt_t updated_entry;
    bzero(&updated_entry, sizeof(MFS_DirEnt_t));
    updated_entry.inum = -1;
    lseek(FD_FILESYSTEM, *entry_offset, SEEK_SET);
    write(FD_FILESYSTEM, &updated_entry, sizeof(MFS_DirEnt_t));
    free(entry_offset);
    target_inode.real_size -= sizeof(MFS_DirEnt_t);
    lseek(FD_FILESYSTEM, 4096 + 4096 + pinum * sizeof(MFS_Inode_t), SEEK_SET);
    write(FD_FILESYSTEM, &target_inode, sizeof(MFS_Inode_t));

    // decomission unlinked file 
    char data_bitmap_holder[MFS_DATA_REGION_SIZE];
    load_data_bitmap(data_bitmap_holder);
    char inode_bitmap_holder[MFS_INODE_TABLE_SIZE];
    load_inode_bitmap(inode_bitmap_holder);
    decomission_file(matched_inum, &unlinked_inode, inode_bitmap_holder, data_bitmap_holder);
    lseek(FD_FILESYSTEM, 0, SEEK_SET);
    write(FD_FILESYSTEM, inode_bitmap_holder, MFS_INODE_TABLE_SIZE);
    lseek(FD_FILESYSTEM, MFS_INODE_TABLE_SIZE, SEEK_SET);
    write(FD_FILESYSTEM, data_bitmap_holder, MFS_DATA_REGION_SIZE);
    lseek(FD_FILESYSTEM, 4096 + 4096 + sizeof(MFS_Inode_t) * matched_inum, SEEK_SET);
    write(FD_FILESYSTEM, &unlinked_inode, sizeof(MFS_Inode_t));

    int synced = fsync(FD_FILESYSTEM);
    assert(synced == 0);
    int write_status = Server_Send_Reply(reply, sd, receipt_sockaddr);
    return write_status;
}

// ***---------------- Main function ----------------*** //

int
main(int argc, char *argv[])
{

    if(argc<3)
    {
      printf("Usage: server server-port-number file-system-image\n");
      exit(1);
    }

    int portid = atoi(argv[1]);
    int sd = UDP_Open(portid); //port # 
    assert(sd > -1);
    FD_FILESYSTEM = MFS_File_System_Init(argv[2]);
    
    printf("waiting in loop\n");

    while (1) {

        // set up receipt sockaddr
        struct sockaddr_in receipt_sockaddr; // for receipt 
        bzero(&receipt_sockaddr, sizeof(struct sockaddr_in));

        // receive message 
        MFS_Message_t* message = malloc(sizeof(MFS_Message_t));
        bzero(message, sizeof(MFS_Message_t));
        int read_status = Server_Receive_Message(message, sd, &receipt_sockaddr); //read message buffer from port sd
        
        if (read_status > 0) {
            char message_type = message->message_type;
            printf("SERVER: received message type: %c \n", message_type);
            MFS_Reply_t* reply = malloc(sizeof(MFS_Reply_t));
            bzero(reply, sizeof(MFS_Reply_t));
            if(message_type == 'I') {
                reply_Init(reply, sd, &receipt_sockaddr);
            }else if(message_type == 'L'){
                reply_Lookup(message, reply, sd, &receipt_sockaddr);
            }else if(message_type == 'S') {
                reply_Stat(message, reply, sd, &receipt_sockaddr);
            }else if(message_type == 'R') {
                reply_Read(message, reply, sd, &receipt_sockaddr);
            }else if(message_type == 'W') {
                reply_Write(message, reply, sd, &receipt_sockaddr);
            }else if(message_type == 'C') {
                reply_Creat(message, reply, sd, &receipt_sockaddr);
            }else if(message_type == 'U') {
                reply_Unlink(message, reply, sd, &receipt_sockaddr);
            }else{
                printf("SERVER:: Invalid message type! \n");
            }
	    }
        free(message);
    }
    return 0;
}


