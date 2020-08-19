
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <dirent.h>
#include <netinet/tcp.h>

#define MAX_LINE 512
ssize_t total = 0;
// gcc client.c -o client
// ./client <SERVER_IP> <SERVER_PORT>

const int LEN = 100000;

int parse(char *buff, char *cmnd, char *arg)
{

	int i = 0, j=0;
	while (i < LEN && (buff[i] != ' ' && buff[i]!='\0')) {
		cmnd[i] = buff[i];
		i++;
	}

	cmnd[i] = '\0';
	i++;

	if (buff[i]=='\0' && strcmp(cmnd, "exit")==0)
		return 1;
	if(buff[i]=='\0')
		return 0;

	while(i<LEN && buff[i] !='\0' && buff[i]!=' ') {
		arg[j] = buff[i];
		i++;
		j++;
	}
	arg[j] = '\0';
	if(buff[i]!='\0')
		return 0;
	return 1;
}


void init(int sockfd){	// first communication after connection
	char buff[LEN];
	int n;
	bzero(buff,sizeof(buff));		// bzero fills up the array with zeros
	read(sockfd,buff,sizeof(buff));	// read takes data from socket inserts into buffer
	printf("%s\n",buff);
}

void writefile(int sockfd, char * filename,long int sz)
{

	int fileDesc = open(filename,  O_WRONLY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR);	// opening file via file descriptor

	char buffer[MAX_LINE];

	long int total = sz;	// file size in bytes
	ssize_t read_return;	// ssize_t is data type for storing size in bytes
	 while (total > 0) {

        if (fileDesc == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        do {
            read_return = read(sockfd, buffer, MAX_LINE);// break the file in chunks of size MAX_LINE
            if (read_return == -1) {		// and send each chunk separately
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (write(fileDesc, buffer, read_return) == -1) {// writes data from buffer into file specified by file descriptor
                perror("write");
                exit(EXIT_FAILURE);
            }
            total -= read_return;	//	 subtract from total size the amount of bytes sent
        } while(read_return > 0 && total > 0);
        close(fileDesc);
    }
}



int get(char * filename,int sockfd){
	if (access(filename,F_OK) == 0) {
		char c,nextline;
		printf("File already present locally\nWant to overwrite?\nPress Y for Yes and N for No :\n");
		scanf("%c",&nextline);
		scanf("%c",&c);
		if(c=='N') return 0;
	}

	char request_msg[LEN],reply_msg[LEN];
	char * data ;
	bzero(request_msg,sizeof(request_msg));
	bzero(reply_msg,sizeof(reply_msg));

	strcpy(request_msg,"GET ");
	strcat(request_msg,filename);

	write(sockfd,request_msg,strlen(request_msg));

	bzero(reply_msg,sizeof(reply_msg));
	read(sockfd,reply_msg,LEN);

	printf("%s\n",reply_msg );
	reply_msg[2] = '\0';	//converting to string
	if(strcmp(reply_msg,"OK") == 0){
		char file_size[256];
		recv(sockfd,file_size,256,0);	// first taking from server the size of the file
		printf("File of size %s will be received\n",file_size );
		write(sockfd,"SEND",4);		//sending acknowledgement for synchronization
		printf("Receiving file...\n");
		writefile(sockfd,filename,atoi(file_size));
	}
	else {
		printf("File not present at server\n");
			return 0;
	}
	return 1;
}

void doPUT(char * filename,int sockfd){
		struct stat obj;		// file struct
		int fileDesc, fileSize;
		stat(filename, &obj);
		fileDesc = open(filename, O_RDONLY);	//open file in read only mode
		fileSize = obj.st_size;

		send(sockfd, &fileSize, sizeof(int), 0);
		printf("Sending file...\n");
		if (sendfile(sockfd, fileDesc, NULL, fileSize) <0) {
			printf("Couldn't send file\n");
			return;
		}
		char msg[LEN];
		read(sockfd,msg,LEN);
		printf("%s by server\n",msg);
}


int put(char * filename,int sockfd){
	if (access(filename,F_OK ) < 0 ) {
		printf("File does not exist\n");
		return 0;
	}
	char request_msg[LEN],reply_msg[LEN];
	char * data ;

	bzero(request_msg,sizeof(request_msg));
	bzero(reply_msg,sizeof(reply_msg));

	strcpy(request_msg,"PUT ");
	strcat(request_msg,filename);

	write(sockfd,request_msg,strlen(request_msg));

	bzero(reply_msg,sizeof(reply_msg));
	read(sockfd,reply_msg,LEN);

	if(strcmp(reply_msg,"OK") == 0){
		doPUT(filename,sockfd);
	}
	else {
		printf("Server already has a file named this\nWant to overwrite?Press Y or N:\n");
		char response[LEN], ch;
		int i = 0;
		do{
            scanf("%c", &ch);
            if(ch!='\n' && ch!=EOF){
                response[i] = ch;
                i++;
            }
		}while(ch!=EOF && ch!='\n');// asking user whether to overwrite
		write(sockfd,response,strlen(response));
		bzero(reply_msg,sizeof(reply_msg));
		read(sockfd,reply_msg,LEN);
		printf("%s\n",reply_msg);
		if(strcmp(reply_msg,"OK") == 0 ) doPUT(filename,sockfd);
	}

}

int mget(int sockfd){
	while(1){
		char filename[LEN],reply_msg[LEN];

		bzero(filename,sizeof(filename));
		bzero(reply_msg,sizeof(reply_msg));

		read(sockfd,filename,LEN);
		printf("%s\n",filename);
		if(strcmp(filename,"Finished")==0) {
			return 0;
		}

		if (access(filename,F_OK) == 0) {
			char c,nextline;
			printf("File already present locally\nWant to overwrite?\nPress Y for Yes and N for No :\n");
			scanf("%c",&nextline);
			scanf("%c",&c);
			if(c == 'N') {
				write(sockfd,"N",1);
				continue;
			}
		}

		write(sockfd,"Y",1);

		read(sockfd,reply_msg,sizeof(reply_msg));//OK expected

		printf("%s\n", reply_msg);

		if(strcmp(reply_msg,"OK") == 0){
			char file_size[256];
			recv(sockfd,file_size,256,0);

			printf("Reading %d bytes\n",atoi(file_size) );

			char request_msg[LEN];

			strcpy(request_msg,"SEND");
			write(sockfd,request_msg,strlen(request_msg));
			printf("Receiving file...\n");
			writefile(sockfd, filename, atoi(file_size));
			printf("File received\n");

			char temp[] = "READY";	// acknowledgementfor synchronization
			temp[5] = '\0';
			write(sockfd, temp, strlen(temp));
		}
	}
}

void clear (FILE *in)
{
  //while ( getchar() != '\n' );
  char ch;

  do
    ch = fgetc ( in );
  while ( ch != EOF && ch != '\n' );

  clearerr ( in );
}

int32_t main(int argc , char **argv){

	assert(argc == 3);	// 2nd argument is SERVER_IP and 3rd is PORT
	char SERVER_IP[100];
	strcpy(SERVER_IP,argv[1]);
	int SERVER_PORT = atoi(argv[2]);


	int sockfd = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in serv_addr ;

	if(sockfd == -1){
		printf("Could not create socket\n");
		exit(0);
	}
	else {
		printf("Successfully created socket\n");
	}

	bzero(&serv_addr , sizeof(serv_addr)); // fills serv_addr with zeros

	serv_addr.sin_family = AF_INET;		// ipv4
	serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serv_addr.sin_port = htons(SERVER_PORT);  // host to network short

	if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr) ) != 0){
		printf("Could not connect to server:(\n");
		exit(0);
	}
	else printf("Connecting to the server\n");
	init(sockfd);

	int yes = 1;
	int result = setsockopt(sockfd,
                        IPPROTO_TCP,
                        TCP_NODELAY,	// disables the nagle bufferring algorithm
                        (char *) &yes,
                        sizeof(int));

	while(1){
		char *filename,reply_msg[LEN];
		char cmnd[LEN], arg[LEN];
		char buf[LEN];

		bzero(reply_msg, sizeof(reply_msg));
		bzero(cmnd, sizeof(cmnd));
		bzero(arg, sizeof(arg));
		bzero(buf, sizeof(buf));

		printf("Enter one of the following commands:\n");
		printf("GET <FILENAME>\nPUT <FILENAME>\nMGET <EXTENSION>\nMPUT <EXTENSION>\nexit\n");
		char ch;
		//scanf("%[^\n]",buf);
		int i = 0;
		do{
            scanf("%c", &ch);
            if(ch!='\n' && ch!=EOF){
                buf[i] = ch;
                i++;
            }
		}while(ch!=EOF && ch!='\n');
		//(stdin);
		// scanf("%c", &ch);
        int res = parse(buf, cmnd, arg);

		printf("%s %s\n", cmnd, arg);
		if(!res){
			printf("Please enter a valid command:\n");
			continue;
		}

        if(strcmp(cmnd,"exit")==0) {
			write(sockfd,cmnd,sizeof(cmnd));
			break;
		}
		else if(strcmp(cmnd,"GET")==0){
			filename = arg;
			int rt = get(filename,sockfd);
			if(!rt) continue;
			printf("Successfully received\n");
		}
		else if(strcmp(cmnd,"PUT")==0){
			filename = arg;
			int rt = put(filename,sockfd);
			//clear(stdin);
		}
		else if(strcmp(cmnd,"MGET")==0){
			char buf[LEN],request_msg[LEN],*ext;
			bzero(buf,sizeof(buf));
			bzero(request_msg,sizeof(request_msg));

			ext = arg;
			strcpy(request_msg,"MGET ");
			strcat(request_msg,ext);	// appending the desired extension to mget

			write(sockfd,request_msg,strlen(request_msg));
			mget(sockfd);  // passing the command to mget function
		}
		else if(strcmp(cmnd,"MPUT")==0){
			char *ext;
			ext = arg;
			char * ext_name = strtok(ext,".");
			DIR *dir;		// directory pointer
			struct dirent *file;
			dir = opendir(".");	//  current directory
			if (dir)
			{
				while ((file = readdir(dir)) != NULL)
				{
					char temp[LEN];
					strcpy(temp, file->d_name);
					char *fname = strtok(temp, ".");
					char *fext = strtok(NULL, ".");
					if(fext != NULL && strcmp(fext, ext_name) == 0) {
						printf("%s\n",file->d_name );
						put(file->d_name,sockfd);
					}
				}
			}
			// scanf("%c", &ch);
			//clear(stdin);
		}
		else {
			printf("Please enter a valid command:\n");
		}
	}


}
