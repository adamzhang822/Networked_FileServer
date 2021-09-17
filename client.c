/*
#include <stdio.h>
#include "udp.h"
#include "mfs.h"

int
main(int argc, char *argv[])
{
    if(argc<3)
    {
      printf("Usage: client server-name server-port \n");
      exit(1);
    }

    char* host_name = argv[1];
    int server_port = atoi(argv[2]);

    int init = MFS_Init(host_name, server_port);
    assert(init >= 0);

    int lookup = MFS_Lookup(0, ".");
    int lookup2 = MFS_Lookup(0, "..");
    printf("CLIENT:: The result of lookup and lookup2: %d, %d \n", lookup, lookup2);

    MFS_Stat_t stat_root;
    MFS_Stat(0, &stat_root);
    printf("CLIENT:: looked up stat of inum 0, type: %d, size: %d, blocks: %d \n", stat_root.type, stat_root.size, stat_root.blocks);
    int stat2 = MFS_Stat(1, &stat_root);
    printf("CLIENT:: looked up inum 1 stat, the return code is : %d \n", stat2);

    MFS_DirEnt_t entry1;
    MFS_DirEnt_t entry2;
    char read_buffer[BUFFER_SIZE];
    MFS_Read(0, read_buffer, 0);
    memcpy(&entry1, read_buffer, sizeof(MFS_DirEnt_t));
    memcpy(&entry2, read_buffer + sizeof(MFS_DirEnt_t), sizeof(MFS_DirEnt_t));
    printf("CLIENT:: read in first 2 entries \n");
    printf("CLIENT:: 1st entry inum: %d, name: %s\n", entry1.inum, entry1.name);
    printf("CLIENT:: 2nd entry inum: %d, name: %s\n", entry2.inum, entry2.name);

    if (MFS_Creat(0, MFS_DIRECTORY, "bin") != 0){
      printf("CLIENT:; Creat failed! \n");
    }

    int rc = MFS_Lookup(0, "bin");
    printf("CLIENT:: tried to look up bin after creat, the inum of bin is: %d", rc);

    return 0;

}
*/

#include "mfs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//
// MFS_Unlink: a file
//

int main(int argc, char* argv[]) {
  MFS_Init(argv[1], atoi(argv[2]));

  // /dir1
  int rc;
  rc = MFS_Creat(0, MFS_DIRECTORY, "dir1");
  if (rc == -1){
    printf("CLIENT:: Failed at creat \n");
    return -1;
  }

  int inum = MFS_Lookup(0, "dir1");
  if (inum <=0){
    printf("CLIENT:; failed at first dir1 lookup \n");
    return -1;
  }
	printf("inum = %d\n", inum);
	
	rc = MFS_Creat(0, MFS_REGULAR_FILE, "whatever");
	if (rc == -1){
    printf("CLIENT:; Failed at creat for whatever \n");
    return -1;
  }
	int inum3 = MFS_Lookup(0, "whatever");
	printf("inum3 = %d\n", inum3);
	if (inum3 <= 0 || inum3 == inum){
    return -1;
  }


	rc = MFS_Creat(0, MFS_DIRECTORY, "dir2");
	if (rc == -1){
    return -1;
  }


	int inum2 = MFS_Lookup(0, "dir2");
	if (inum2 <=0 || inum2 == inum || inum3 == inum2) {
    return -1;
  }

	rc = MFS_Unlink(0, "whatever");
	if (rc == -1) {
    return -1;
  }

  rc = MFS_Unlink(0, "dir1");
  if (rc == -1) {
    return -1;
  }

  rc = MFS_Lookup(0, "dir1");
  if (rc >= 0) {
    return -1;
  }

	rc = MFS_Lookup(0, "dir2");
	if (rc < 0) {
    return -1;
  }

	MFS_Stat_t m;	  
  rc = MFS_Stat(inum, &m);
  if (rc == 0) {
    return -1;
  }

	rc = MFS_Stat(0, &m);
	if (rc == -1) {
    return -1;
  }
	printf("m.size = %d\n", m.size);

  printf("CLIENT:: Passed everything before sanity check \n");

// sanity check
	if (m.size != 3 * sizeof(MFS_DirEnt_t)) {
    printf("CLIENT:; Failed at first sanity check \n");
    return -1;
  }
	char buf[MFS_BLOCK_SIZE];
	if ((rc = MFS_Read(0, buf, 0)) < 0) {
    return -1;
  }
	int i;
	MFS_DirEnt_t* e;
	int fd2, fc, fp;
	fd2 = fc = fp = 0;
	for (i = 0; i < 5; i++) {
		e = buf + i * sizeof(MFS_DirEnt_t);
		printf("e.inum = %d\n", e->inum);	
		if (e->inum >= 0) {
			if (strcmp(e->name, ".") == 0) fc = 1;
			if (strcmp(e->name, "..") == 0) fp = 1;
			if (strcmp(e->name, "dir2") == 0) fd2 = 1;
		}	
	}

	if (!fc || !fp || !fd2) {
    return -1;
  }

  return 0;
}