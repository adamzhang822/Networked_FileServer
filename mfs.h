#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)
#define BUFFER_SIZE (4096)

#define MFS_BLOCK_SIZE   (4096)
#define MFS_INODE_BLOCKS_SIZE (10)
#define MFS_INODE_TABLE_SIZE (4096)
#define MFS_DATA_REGION_SIZE (4096)



typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes 
    int blocks; // number of blocks allocated to file
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    int  inum;      // inode number of entry (-1 means entry not used)
    char name[252]; // up to 252 bytes of name in directory (including \0)
    // one datablock can contain 16 directory entries 
} MFS_DirEnt_t;

//*********** Structs for file system image ****************/

typedef struct __MFS_Data_Block_t {
    char data[MFS_BLOCK_SIZE];
} MFS_Data_Block_t;

typedef struct __MFS_Inode_t {
    int type; // directory (0), regular file (1)
    int size; // number of bytes in the file
    int real_size; // real size of directory file
    int block_nums; // number of blocks allocated to the file
    int blocks[MFS_INODE_BLOCKS_SIZE]; // array of block offsets
} MFS_Inode_t;

typedef struct __MFS_Message_t {
    char message_type; // 'I', 'L', 'S', 'W', 'R','C','U'
    int inum; // for lookup (pinum), stat lookup, write, read 
    int type; // for create (directory or regular file)
    int block; // block offset for write, read
    char name[252]; // file or directory name to lookup
    char buffer[MFS_BLOCK_SIZE]; //content of write
} MFS_Message_t;

typedef struct __MFS_Reply_t {
    char reply_type; // 'I', 'L', 'S', 'W', 'R','C','U'
    int success; // -1: failure, 0: success; for lookup, return the inode
    MFS_Stat_t stats; // for lookup
    char buffer[BUFFER_SIZE]; // for read 
} MFS_Reply_t;


//*********** Interface for file system image ****************/


int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);

//*********** Helper functions ****************/

int MFS_Send_Message(MFS_Message_t* message);
int MFS_Receive_Reply(MFS_Reply_t* reply);
void print_DirEnt(MFS_DirEnt_t* dirent);

#endif // __MFS_h__
