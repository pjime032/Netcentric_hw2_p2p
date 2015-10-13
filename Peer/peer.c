#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#define BUFFSIZE 256

void syserr(char *msg) { perror(msg); exit(-1); }
void ftpcomm(int newsockfd, char* buffer);
void ClientCode(char *trackIP, int portTrac);
void readandsend(int tempfd, int newsockfd, char* buffer);
void recvandwrite(int tempfd, int newsockfd, int size, char* buffer);

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portTrac, portClient, pid, hasClientRun;
  struct sockaddr_in serv_addr, clt_addr;
  socklen_t addrlen;
  char buffer[256];

  if(argc != 4) 
  { 
    portTrac = 5000;
    printf("Default Tracker port is: %d\n", portTrac);
    portClient = 6000;
    printf("Default Client port is: %d\n", portClient);
  }
  else
  { 
  	portTrac = atoi(argv[2]); 
  	portClient = atoi(argv[3]);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0); 
  if(sockfd < 0) syserr("can't open socket"); 
  	printf("create socket...\n");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portTrac);

  if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
    syserr("can't bind");
  printf("bind socket to port %d...\n", portTrac);

  listen(sockfd, 5);
  hasClientRun = 0; 

for(;;) {
  
  pid = fork();
   if (pid < 0)
     syserr("Error on fork");
   if (pid == 0)
   {
   	if(hasClientRun == 0)
   	{
   		hasClientRun = 1;
   		ClientCode(argv[1], portTrac);	   	
   	}
   	else
   	{
	  	printf("wait on port %d...\n", portTrac);
	  	addrlen = sizeof(clt_addr); 
	  	newsockfd = accept(sockfd, (struct sockaddr*)&clt_addr, &addrlen);
	  	if(newsockfd < 0) syserr("can't accept"); 
		close(sockfd);
		ftpcomm(newsockfd, buffer);
		close(newsockfd);
		exit(0);
     }
   }
}
  close(sockfd); 
  return 0;
}

void ftpcomm(int newsockfd, char* buffer)
{
	int n, size, tempfd;
    struct stat filestats;
	char * filename;
	char command[20];
	filename = malloc(sizeof(char)*BUFFSIZE);
	for(;;)
	{
	    memset(buffer, 0, BUFFSIZE);
		n = recv(newsockfd, buffer, BUFFSIZE, 0);
		//printf("amount of data recieved: %d\n", n);
		if(n < 0) syserr("can't receive from client");
		sscanf(buffer, "%s", command);
		//printf("message from client is: %s\n", buffer);
		
		if(strcmp(command, "ls") == 0)
		{
			system("ls >remotelist.txt");
			stat("remotelist.txt", &filestats);
			size = filestats.st_size;
			//printf("Size of file to send: %d\n", size);
            size = htonl(size);      
			n = send(newsockfd, &size, sizeof(int), 0);
		    if(n < 0) syserr("couldn't send size to client");
			//printf("The amount of bytes sent for filesize is: %d\n", n);
			tempfd = open("remotelist.txt", O_RDONLY);
			if(tempfd < 0) syserr("failed to get file, server side");
			readandsend(tempfd, newsockfd, buffer);
			close(tempfd);			
		}
		
		if(strcmp(command, "exit") == 0)
		{
			printf("Connection to client shutting down\n");
			int i = 1;
			i = htonl(i);
			n = send(newsockfd, &i, sizeof(int), 0);
		    if(n < 0) syserr("didn't send exit signal to client");
			break;  // check to make sure it doesn't need to be exit
		}
		
		if(strcmp(command, "get") == 0)
		{
			sscanf(buffer, "%*s%s", filename);
			//printf("filename from client is: %s\n", filename);
			//printf("size of filename is: %lu\n", sizeof(filename));
			stat(filename, &filestats);
			size = filestats.st_size;
			//printf("Size of file to send: %d\n", size);
            size = htonl(size);      
			n = send(newsockfd, &size, sizeof(int), 0);
		    if(n < 0) syserr("couldn't send size to client");
			//printf("The amount of bytes sent for filesize is: %d\n", n);
			tempfd = open(filename, O_RDONLY);
			if(tempfd < 0) syserr("failed to open file");
			readandsend(tempfd, newsockfd, buffer);
			close(tempfd);			
		}
		if(strcmp(command, "put") == 0)
		{
			//Parse filename
			sscanf(buffer, "%*s%s", filename);
			//printf("filename from client is: %s\n", filename);
			//printf("size of filename is: %lu\n", sizeof(filename));
			//Receieve size of file
			n = recv(newsockfd, &size, sizeof(int), 0); 
        	if(n < 0) syserr("can't receive from server");
        	size = ntohl(size);        
			if(size ==0) // check if file exists
			{
				printf("File not found\n");
				break;
			}
			//printf("The size of the file to recieve is: %d\n", size);
			//Receieve the file
			tempfd = open(filename, O_CREAT | O_WRONLY, 0666);
			if(tempfd < 0) syserr("failed to open the file");
			recvandwrite(tempfd, newsockfd, size, buffer);
			close(tempfd);
		}
	}
}

void ClientCode(char *trackHost, int portTrac)
{
  //Socksfd: file descriptor; portno: port number; n: return values for read and write
  int socksfd, portno, n, size, tempfd;
  char input[70];
  char *filename;
  char *command;
  struct hostent* server; // server info
  struct sockaddr_in serv_addr; //server address info
  struct stat filestats;
  char buffer[256];
  command = malloc(sizeof(char)*sizeof(buffer));
  filename = malloc(sizeof(char)*sizeof(buffer));
  
  server = gethostbyname(trackHost);
  if(!server) {
    fprintf(stderr, "ERROR: no such host: %s\n", trackHost);
    //return 2;
    exit(0);
  }
  portno = portTrac;

  socksfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(socksfd < 0) syserr("can't open socket");
  printf("create socket...\n");

 // set all to zero, then update the sturct with info
  memset(&serv_addr, 0, sizeof(serv_addr)); 
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
  serv_addr.sin_port = htons(portno); 	

 // connect with file descriptor, server address and size of addr
  if(connect(socksfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    syserr("can't connect to server");
  printf("connect...\n");
  printf("Connection established. Waiting for commands...\n");
  
  for(;;){
  	printf("%s:%d> ", trackHost, portno);
  	fgets(buffer, sizeof(input), stdin);
  	int m = strlen(buffer);
  	if (m>0 && buffer[m-1] == '\n')
  		buffer[m-1] = '\0';
  	
  	strcpy(command, strtok(buffer, " "));
  	//printf("The size of command is: %lu\n", sizeof(command));
  	//printf("The command is: %s\n", command);
  	if(strcmp(command, "ls-local") == 0)
  	{	
  		printf("Files at the Client\n");
		system("ls -a | cat");
		printf("\n");
  	}
  	else if(strcmp(command, "list") == 0)
  	{
  		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, "list");
		//printf("sent in buffer: %s\n", buffer);
		n = send(socksfd, buffer, BUFFSIZE, 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to tracker");
		n = recv(socksfd, &size, sizeof(int), 0); 
        if(n < 0) syserr("can't receive from tracker");
        size = ntohl(size);        
		if(size ==0) // check if file exists
		{
			printf("File not found\n");
			break;
		}
		//printf("The size of the file to recieve is: %d\n", size);
		tempfd = open("trackerlist.txt", O_CREAT | O_WRONLY, 0666);
		recvandwrite(tempfd, socksfd, size, buffer);
		printf("%s\n", buffer);
		remove("trackerlist.txt");
		close(tempfd);	
  	}
  	else if(strcmp(command, "ls-remote") == 0)
  	{
  		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, "ls");
		//printf("sent in buffer: %s\n", buffer);
		n = send(socksfd, buffer, BUFFSIZE, 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to server");
		n = recv(socksfd, &size, sizeof(int), 0); 
        if(n < 0) syserr("can't receive from server");
        size = ntohl(size);        
		if(size ==0) // check if file exists
		{
			printf("File not found\n");
			break;
		}
		//printf("The size of the file to recieve is: %d\n", size);
		tempfd = open("remotelist.txt", O_CREAT | O_WRONLY, 0666);
		recvandwrite(tempfd, socksfd, size, buffer);
		printf("%s\n", buffer);
		remove("remotelist.txt");
		close(tempfd);	
  	}
  	else if(strcmp(command, "exit") == 0)
  	{
  		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, "exit");
		//printf("sending command to server: %s\n", buffer);
		n = send(socksfd, buffer, strlen(buffer), 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to server");
		n = recv(socksfd, &size, sizeof(int), 0);
        size = ntohl(size);  
        if(n < 0) syserr("can't receive from server");
        
		//printf("size was %d from server\n", size);
		if(size)
		{
			printf("Connection to server terminated\n");
			exit(0);
		}
		else
		{
			printf("Server didn't exit");
		}
		
  	} 	
  	else if(strcmp(command, "get") == 0)
  	{
  		strcpy(filename, strtok(NULL, " "));
  		//printf("The filename is: %s\n", filename);
  		memset(buffer, 0, sizeof(buffer));
  		strcpy(buffer, "get ");
  		strcat(buffer, filename);
		//printf("sent in buffer: %s\n", buffer);
		n = send(socksfd, buffer, BUFFSIZE, 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to server");
		n = recv(socksfd, &size, sizeof(int), 0); 
        if(n < 0) syserr("can't receive from server");
        size = ntohl(size);        
		if(size ==0) // check if file exists
		{
			printf("File not found\n");
			break;
		}
		//printf("The size of the file to recieve is: %d\n", size);
		tempfd = open(filename, O_CREAT | O_WRONLY, 0666);
		if(tempfd < 0) syserr("failed to get file");
		recvandwrite(tempfd, socksfd, size, buffer);
		printf("Download of '%s' was successful\n", filename);  
		close(tempfd);
		
  	}
  	else if(strcmp(command, "put") == 0)
  	{
  		//Send command & filename
  		strcpy(filename, strtok(NULL, " "));
  		memset(buffer, 0, BUFFSIZE);
  		strcpy(buffer, "put ");
  		strcat(buffer, filename);
  		//printf("The filename to send to server is: %s\n", filename);
		n = send(socksfd, buffer, BUFFSIZE, 0);
		//printf("amount of data sent: %d\n", n);
		if(n < 0) syserr("can't send to server");
		//printf("sent in buffer: %s\n", buffer);
		//Send file size
		stat(filename, &filestats);
		size = filestats.st_size;
		//printf("Size of file to send: %d\n", size);
        size = htonl(size);      
		n = send(socksfd, &size, sizeof(int), 0);
		if(n < 0) syserr("couldn't send size to server");
		//Send file
		tempfd = open(filename, O_RDONLY);
		if(tempfd < 0) syserr("failed to get file, server side");
		readandsend(tempfd, socksfd, buffer);
		printf("Upload of '%s' was successful\n", filename); 
		close(tempfd);	
  	}
  	else
  	{
  		printf("Correct commmands are: 'ls-local', 'ls-remote', 'get <filename>' and 				'put <filename>, 'exit''\n");
  	}  	
  }
  close(socksfd);
}
void readandsend(int tempfd, int newsockfd, char* buffer)
{
	while (1)
	{
		memset(buffer, 0, BUFFSIZE);
		int bytes_read = read(tempfd, buffer, BUFFSIZE); //is buffer cleared here?
		buffer[bytes_read] = '\0';
		if (bytes_read == 0) // We're done reading from the file
			break;

		if (bytes_read < 0) syserr("error reading file");
		//printf("The amount of bytes read is: %d\n", bytes_read); 
		
		int total = 0;
		int n;
		int bytesleft = bytes_read;
		//printf("The buffer is: \n%s", buffer);
		while(total < bytes_read)
		{
			n = send(newsockfd, buffer+total, bytesleft, 0);
			if (n == -1) 
			{ 
			   syserr("error sending file"); 
			   break;
			}
			//printf("The amount of bytes sent is: %d\n", n);
			total += n;
			bytesleft -= n;
		}
	}
}

void recvandwrite(int tempfd, int newsockfd, int size, char* buffer)
{
	int totalWritten = 0;
	int useSize = 0;
	while(1)
	{
		if(size - totalWritten < BUFFSIZE) 
		{
			useSize = size - totalWritten;
		}
		else
		{
			useSize = BUFFSIZE;
		}
			memset(buffer, 0, BUFFSIZE);
			int total = 0;
			int bytesleft = useSize; //bytes left to recieve
			int n;
			while(total < useSize)
			{
				n = recv(newsockfd, buffer+total, bytesleft, 0);
				if (n == -1) 
				{ 
					syserr("error receiving file"); 
					break;
				}
				total += n;
				bytesleft -= n;
			}
			//printf("The buffer is: \n%s", buffer);
			//printf("Amount of bytes received is for one send: %d\n", total);
		
			int bytes_written = write(tempfd, buffer, useSize);
			//printf("Amount of bytes written to file is: %d\n", bytes_written);
			totalWritten += bytes_written;
			//printf("Total amount of bytes written is: %d\n", totalWritten);
			if (bytes_written == 0 || totalWritten == size) //Done writing into the file
				break;

			if (bytes_written < 0) syserr("error writing file");
		
    }	
}
