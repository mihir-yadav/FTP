#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <dirent.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>

#define LEN 100000
#define THREADS_NUM 4
#define MAX_LINE 512
ssize_t total=0;

int PORT;															//Port for Welcome Socket
int activeThreads = 0;												//Number of active threads for the process
bool threadStatus[LEN];												//Thread status

void *connectionHandler(void *);

void writeMsg(int sockfd, char *msg, int n) {						//Writing to socket and handling errors
	if (write(sockfd, msg, n) < 0) {
		perror("Write Failed!!");
		exit(EXIT_FAILURE);
	}
}

void parse(char *buff, char *cmnd, char *arg)						//Function to parse the recieved command
{
	int i = 0, j=0;
	while (i < LEN && (buff[i] != ' ' && buff[i]!='\0')) {
		cmnd[i] = buff[i];
		i++;
	}

	cmnd[i] = '\0';
	i++;

	if (buff[i]=='\0')
		return;
	
	while(i<LEN && buff[i] !='\0') {
		arg[j] = buff[i];
		i++;
		j++;
	}
	arg[j] = '\0';
}

void writefile(int sockfd, char * filename,long int sz)				//Function to send a file in chunks of fixed size
{
	int fileDesc = open(filename,  O_WRONLY | O_CREAT | O_TRUNC,	//File Descriptor for the file to be sent
                S_IRUSR | S_IWUSR);

	char buffer[MAX_LINE];

	long int total = sz;
	ssize_t read_return;
	 while (total > 0) {											//Sending outstanding chunks

        if (fileDesc == -1) {										//If the file is not accessible
            perror("open");
            exit(EXIT_FAILURE);
        }
        do {
            read_return = read(sockfd, buffer, MAX_LINE);			// Reading from socket
            if (read_return == -1) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (write(fileDesc, buffer, read_return) == -1) {		// Writing to file
                perror("write");
                exit(EXIT_FAILURE);
            }
            total -= read_return;
        } while(read_return > 0 && total > 0);
        close(fileDesc);
    }
    printf("File Received Successfully!!\n");

}

int get(char *fileName, int sockfd)									//Implementing the GET command							
{
	struct stat obj;
	int fileDesc;
	long int fileSize;
	char *msg = "OK";

	stat(fileName, &obj);
	fileDesc = open(fileName, O_RDONLY);							//Opening the required file
	if (fileDesc < 0) {												//Error if the file failed to open
		return 2;
	}

	writeMsg(sockfd, msg, sizeof(msg));

	fileSize = obj.st_size;											//Getting the file Size and converting to a character array to send
	printf("File size to be sent : %ld\n", fileSize);
	char fsize[256];
	sprintf(fsize, "%ld", fileSize);
	int ss = strlen(fsize);
	fsize[ss] = '\0';

	send(sockfd, fsize, sizeof(fsize), 0);							//Communicating the file size
	char response[5];
	read(sockfd, response, 4);										//Waiting for an ACK by client
	response[4] = '\0';

	if (strcmp(response, "SEND") == 0) {							
		printf("Sending file...\n");
		if (sendfile(sockfd, fileDesc, NULL, fileSize) <0 ) {		//Sending file into the socket
			perror("Send File Failed!!");
			exit(EXIT_FAILURE);
		}
	}
	printf("File Sent Succesfully\n");
	return 1;
}

int put(char *fileName, int sockfd)									//Implementing the PUT command
{
	char msg[LEN], *data;
	int fileSize, fileDesc;
	
	bzero(msg, sizeof(msg));

	if (open(fileName, O_RDONLY) >=0) {								//Opening the file and checking if it already exists
		strcpy(msg, "File Already Exists.");
		writeMsg(sockfd, msg, strlen(msg));							//Prompting the client to check if file should be replaced
		char response[2];
		read(sockfd, response, 2);
		if (strcmp(response, "N") == 0) {
			return 0;
		}
	}

	strcpy(msg, "OK");												//Server ready to recieve file
	writeMsg(sockfd, msg, strlen(msg));	
	recv(sockfd, &fileSize, sizeof(int), 0);
	printf("Recieving %s...\n", fileName);
	FILE *fp = fopen(fileName, "w+");
	writefile(sockfd,fileName,(fileSize));							//Writing the file to disk

	fclose(fp);
	strcpy(msg, "File Received Succesfully");
	msg[strlen(msg)] = '\0';
	writeMsg(sockfd, msg, strlen(msg));								//Acknowledging the recieved data.
	return 1;
}

void mget(char *extension, int sockfd)								//Function to implement the mget command
{
	DIR *dir;
	struct dirent *file;
	dir = opendir(".");												//Browsing in the current directory
	if (dir)
	{
		while ((file = readdir(dir)) != NULL)						//Getting a file object for each file in directory 
		{
			char temp[LEN];
			strcpy(temp, file->d_name);
			char *fname = strtok(temp, ".");
			char *fext = strtok(NULL, ".");
			if(fext != NULL && strcmp(fext, extension) == 0) {		// If required extension matches the file extension
				char *name = file->d_name, reply[15], ack[15];
				printf("Sending %s\n", name);
				writeMsg(sockfd, name, strlen(name));
				read(sockfd, reply, 5);
				if (strcmp(reply, "N") == 0)						//Checking if the server need the mentioned file
					continue;
				get(file->d_name, sockfd);							//Invoking the get function and reading an ACK
				read(sockfd, ack, 15);
			}			
		}
		closedir(dir);

		char *fin = "Finished";
		writeMsg(sockfd, fin, strlen(fin));							//Communicating that no more files are present
	}
	else {
		perror("Failed to retrieve Files!");
		exit(EXIT_FAILURE);
	}
}

int welcomeSocket;
struct sockaddr_in address;

int main(int argc, char **argv)
{
	assert(argc == 2);												// Validity check of Run command
	PORT = atoi(argv[1]);

	char *hello = "Connection Succesful", buff[LEN], *fileTransferFail = "File Transfer Failed!!", *notFound = "File Not Found!";
	if ((welcomeSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {			//Creating an IPv4 socket for TCP connection
		perror("Socket Creation Failed");
		exit(EXIT_FAILURE);
	}
    	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	if ( bind(welcomeSocket, (struct sockaddr *)&address, sizeof( address )) < 0 ) {		//Binding the created socket
		perror( "Bind Failed !! " );
		exit(EXIT_FAILURE);

	}
	
  	// THREADS_NUM threads declared
	pthread_t threads[THREADS_NUM];

	int rc;
	while(1) {														// This loop creates thread as soon as any thread exits
		if(activeThreads < THREADS_NUM) {
			for(long i=0; i<THREADS_NUM; i++) {
				if(!threadStatus[i]) {								// Free thread is selected 
					threadStatus[i] = true;
					activeThreads++;
                  	// Create new thread which on creation call function connectionHandler
					rc = pthread_create(&threads[i], NULL, connectionHandler, (void *)i);
					if (rc){
						threadStatus[i] = false;
						activeThreads--;
						printf("ERROR; return code from pthread_create() is %d\n", rc);
						exit(-1);
					}
				}
			}
		}
	}
	
	pthread_exit(NULL);
}

// This function is run by every new thread created
void * connectionHandler (void *newThreadID) 
{
	long threadID;
	threadID = (long)newThreadID;

	printf("Thread with id : %ld free now\n", threadID);
	
	int newSocket;
	char *hello = "Connection Successful", buff[LEN], *fileTransferFail = "File Transfer Failed!!", *notFound = "File Not Found!";
	
	if ( listen(welcomeSocket, 5) < 0 ) {											// Waiting for connection
		perror("Listen");
		exit(EXIT_FAILURE);
	}

	int addrlen = sizeof(address);
  	// Creating newSocket for client to transfer data after communication with welcomeSocket
	if ( (newSocket = accept(welcomeSocket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0 ) {
		perror("Accept Failed");
		exit(EXIT_FAILURE);
	}
	int yes = 1;
  	// Set socket to send data without delay i.e., to fill buffer
	int result = setsockopt(welcomeSocket,IPPROTO_TCP,TCP_NODELAY,(char *) &yes, sizeof(int));
	printf("New client connected with thread ID : %ld\n", threadID);
	writeMsg(newSocket, hello, strlen(hello));

	while(1) {
		bzero(buff, sizeof(buff));
		char cmnd[LEN], arg[LEN], msg[255];
		bzero(cmnd, sizeof(cmnd));
		bzero(arg, sizeof(arg));

		int rd = read(newSocket, buff, LEN);								//Checking if the client has closed the connection to enforce Graceful termination
		if(rd == 0) {
			activeThreads--;
			threadStatus[threadID] = false;										//Marking the thread as free
			pthread_exit(NULL);
		}

		parse(buff, cmnd, arg);													//Parsing the recieved command
		printf("%s %s \n",cmnd,arg);		
		if ( strcmp(cmnd, "GET") == 0 ) {										//CASE : GET command
			
			int c = 0,res=0;
			while ( (res = get(arg, newSocket)) != 1) {						//Doing a max of 5 tries to send the file
				if (res == 2) {
					strcpy(msg, notFound);
					break;
				}
				c++;
				strcpy(msg, fileTransferFail);
			}

			if (c==5 || res==2) {
				writeMsg(newSocket, msg, strlen(msg));						//Stop trying after 5 tries
				continue;
			}

		}

		if ( strcmp(cmnd, "PUT") == 0) {										//CASE : PUT command
			int n = put(arg, newSocket);
			if (n)
				continue;
			strcpy(msg, "Not required");
			writeMsg(newSocket, msg, strlen(msg));
		}

		if ( strcmp(cmnd, "MGET") == 0) {										//CASE : MGET command
			char *temp = strtok(arg, ".");										// Removing '.' from extension
			mget(temp, newSocket);
		}

		if ( strcmp(cmnd, "exit") == 0) {										//CASE : exit
			break;
		}
	}

	activeThreads--;															//Releasing the thread if client exits
	threadStatus[threadID] = false;
	close(newSocket);															// Close socket connection

	pthread_exit(NULL);															// Free thread
}