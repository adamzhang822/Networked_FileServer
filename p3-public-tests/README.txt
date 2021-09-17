===========
HOW TO RUN
===========

*** General:

1. Copy the contests of this .tar archive to your working directory

2. Each test expects a server name and a port passed by MFS_SERVER and MFS_PORT
i.e you have to create these environment variables and set correct values.
(Note: To avoid setting them every time you could modify ~/.bashrc by adding 
these two lines
	export MFS_SERVER=localhost
	export MFS_PORT=10003

at the top and source it (e.g., 
	$.> source ~/.bashrc

However, remember to revert to the original ~/.bashrc version once you are 
done with testing this project)

3. To run all test at once, use
     $.> ./run-test.py

   Or, to run a specific test,
     $.> ./run-test.py [1-46]


*** Tips for running individual test cases:

1. check if testclient is compiled successfully

2. check if your MFS_SERVER and MFS_PORT is configure correctly:
$.> echo $MFS_SERVER
$.> echo $MFS_PORT

3. Check out the test code (i.e if you test test X, you should look into 
./testcases/testX.c) to understand how it works and what it expects.
Try to write a small code snippet (client-side) that does similar thing and 
run the test separately (i.e, w/o the test framework).


===========
TESTS SPECS
===========

Basic
-----
1 - README or README.txt
2 - Makefile or makefile
3 - make generate server and libmfs.so


MFS interface
-------------
5- MFS_Lookup: name does not exists
8- MFS_Lookup: a directory
9- MFS_Lookup: a file

11- MFS_Stat: for new dir
12- MFS_Stat: for dir having some entries
13- MFS_Stat: stat for an empy file

15- MFS_Write: invalid inum
18- MFS_Write: write single block
19- MFS_Write: write multiple blocks 

26- MFS_Read: a block in directory
27- MFS_Read: a block normal file

29- MFS_Creat: create new a directory
30- MFS_Creat: create new a file

37- MFS_Unlink: an empty directory
38- MFS_Unlink: a file

Client timeout
-------------
- Check if client timeout and retry correctly with each MFS_XYZ
41+ lookup
42+ write
43+ read
44+ stat
45+ creat
46+ unlink