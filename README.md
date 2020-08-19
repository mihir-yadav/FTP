# CS 349 - Computer Networks Lab  
170101006 - Aman Raj  
170123029 - Kushagra Mahajan  
170123034 - Mihir Yadav  

FTP(File Transfer Protocol) implementation using Socket Programming  

GET <FILE_NAME> : downloads the specified file from the server  
PUT <FILE_NAME> : uploads the specified file to the server  
MGET <EXTENSION> : downloads all the files with the specified extension from the server  
MPUT <EXTENSION> : uploads all the files with the specified extension to the server  

1) If the mentioned file is already present client will be asked whether to overwrite or not.  
2) Multithreading has also been implemented, enabling multiple clients to connect to server and transfer files simultaneously.  

Commands (to be executed in this order) :  
Server terminal:  
$ gcc server.c -o server -lpthread  
$ ./server 8080  

client terminal   
$ gcc client.c -o client   
$ gcc client.c -o client  
$ ./client 127.0.0.1 8080  

Place client.c and server.c in separate folders to check that the files are transferred correctly.  
One can use any other port number in place of 8080   
One should insert server IP in place of localhost if transferring files to some other PC,  
given that both server and client are connected to same network.  

Sample commands:  
GET a.c  
MPUT .txt  
exit  

Restarting may be done by the exit command or  
by simply killing the process (ctrl+C) and starting it again.  

Assumptions :  
1) File names do not contain space.  
2) Files that are to be transferred are not opened in some other application (like gedit).  
3) If a port is not properly closed, running the executable ./a.out (renamed as server) might cause 	port binding to fail. In such a case, one can use some other port (like 8081) or close the 			specified port properly.  
4) All the files are transferred to the folder containing the source files (client.c and server.c).  
5) Sometimes the TCP socket may overflow causing issues in file transfer, in such cases restarting the server and client solves the issue.  
