CC   = gcc
OPTS = -Wall

all: server client

# for mfs
mfs.o : mfs.c
	$(CC) $(OPTS) -c -fpic $< -o $@

libmfs.so : mfs.o 
	$(CC) $(OPTS) -shared -o $@ $<

# this generates the target executables
server: server.o udp.o libmfs.so
	$(CC) -o server server.o udp.o -L. -lmfs

client: client.o udp.o mfs.o
	$(CC) -o client client.o udp.o -L. -lmfs

# this is a generic rule for .o files 
%.o: %.c 
	$(CC) $(OPTS) -c $< -o $@

clean:
	rm -rf *.o *.so server client



