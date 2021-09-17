#include <stdio.h>
#include "udp.h"

#define BUFFER_SIZE (4096)

int
main(int argc, char *argv[])
{

    if(argc<2)
    {
      printf("Usage: server server-port-number\n");
      exit(1);
    }

    int portid = atoi(argv[1]);
    int sd = UDP_Open(portid); //port # 
    assert(sd > -1);

    printf("waiting in loop\n");

    while (1) {
	struct sockaddr_in s;
	char buffer[BUFFER_SIZE];
	int rc = UDP_Read(sd, &s, buffer, BUFFER_SIZE); //read message buffer from port sd
	if (rc > 0) {
	    printf("SERVER:: read %d bytes (message: '%s')\n", rc, buffer);
	    char reply[BUFFER_SIZE];
	    sprintf(reply, "reply");
	    rc = UDP_Write(sd, &s, reply, BUFFER_SIZE); //write message buffer to port sd
	}
    }

    return 0;
}


