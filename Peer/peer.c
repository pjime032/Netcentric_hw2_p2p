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
#include <dirent.h>
#define BUFFSIZE 256

void syserr(char *msg) { perror(msg); exit(-1); }
void serverRoutine(int newsockfd, char* buffer);
void ClientCode(char *trackIP, int portTrac, int portClient);
void peer2peer(uint32_t cIP, int cP, char * filen);
void readandsend(int tempfd, int newsockfd, char* buffer);
void recvandwrite(int tempfd, int newsockfd, int size, char* buffer);
//List of files
typedef struct l {

  char * filename; 
  uint32_t clientIP;
  int portnum;
  struct l * fl_next;
} fileList;

fileList * head = NULL;
fileList * curr = NULL;
fileList * tail = NULL;

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portTrac, portClient, pid;
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
  	printf("create socket as peer server...\n");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portClient);

  if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
    syserr("can't bind");
  printf("bind socket to port %d...\n", portClient);

  listen(sockfd, 5); 
  //Fork Client & Servers
   	pid = fork();
   	if (pid < 0) syserr("Error on fork");
   	if (pid == 0)
   	{
		for(;;) {
		  printf("wait on port %d...\n", portClient);
		  addrlen = sizeof(clt_addr); 
		  newsockfd = accept(sockfd, (struct sockaddr*)&clt_addr, &addrlen);
		  if(newsockfd < 0) syserr("can't accept"); 
		  
		  pid = fork();
		   if (pid < 0)
			 syserr("Error on fork");
		   if (pid == 0)
		   {
			 close(sockfd);
			 serverRoutine(newsockfd, buffer);
			 exit(0);
		   }
		   else
			 close(newsockfd);
		}
   	}
    else
    {
		ClientCode(argv[1], portTrac, portClient);
		close(sockfd);
    }
  close(sockfd); 
  return 0;
}

void serverRoutine(int newsockfd, char* buffer)
{
	int n, size, tempfd;
    struct stat filestats;
	char * filename;
	filename = malloc(sizeof(char)*BUFFSIZE);
	
	//Receive file name
    memset(buffer, 0, BUFFSIZE);
	n = recv(newsockfd, buffer, BUFFSIZE, 0);
	//printf("amount of data recieved: %d\n", n);
	if(n < 0) syserr("can't receive filename from peer");
	sscanf(buffer, "%s", filename);
	printf("filename peer wants to download is: %s\n", filename);
	//printf("size of filename is: %lu\n", sizeof(filename));
	
	//Send file size and file to peer
	stat(filename, &filestats);
	size = filestats.st_size;
	printf("Size of file to send: %d\n", size);
    size = htonl(size);      
	n = send(newsockfd, &size, sizeof(int), 0);
    if(n < 0) syserr("couldn't send size to peer");
	//printf("The amount of bytes sent for filesize is: %d\n", n);
	tempfd = open(filename, O_RDONLY);
	if(tempfd < 0) syserr("failed to open file");
	readandsend(tempfd, newsockfd, buffer);
	close(tempfd);
	
	//Close the connection to peer	
	printf("Connection to peer shutting down\n");
	int i = 1;
	i = htonl(i);
	n = send(newsockfd, &i, sizeof(int), 0);
    if(n < 0) syserr("didn't send exit signal to client");
}

void ClientCode(char *trackHost, int portTrac, int portClient)
{
  //Socksfd: file descriptor; portno: port number; n: return values for read and write
  int socksfd, portno, n, size, listLen;
  char input[70];
  char *filename;
  char *command;
  struct hostent* server; // server info
  struct sockaddr_in serv_addr; //server address info
  char buffer[256];
  command = malloc(sizeof(char)*sizeof(buffer));
  
  server = gethostbyname(trackHost);
  if(!server) {
    fprintf(stderr, "ERROR: no such host: %s\n", trackHost);
    //return 2;
    exit(0);
  }
  portno = portTrac;

  socksfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(socksfd < 0) syserr("can't open socket");
  printf("create socket as peer client...\n");

 // set all to zero, then update the sturct with info
  memset(&serv_addr, 0, sizeof(serv_addr)); 
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
  serv_addr.sin_port = htons(portno); 	

 // connect with file descriptor, server address and size of addr
  if(connect(socksfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    syserr("can't connect to tracker");
  printf("connect...\n");
  printf("Connection established. Waiting for commands...\n");
  
  //Send portClient
  int cliP = htons(portClient);
  n = send(socksfd, &cliP, sizeof(int), 0);
  if(n < 0) syserr("can't send portClient to tracker");
  
  //Send Files
  DIR * directory;
  struct dirent * filesInfo;
  directory = opendir("./");
  if(directory != NULL)
  {
  	while((filesInfo = readdir(directory)) != NULL)
  	{
  		if(filesInfo -> d_type != DT_DIR)
  		{
	  		memset(buffer, 0, sizeof(buffer));
	  		strcpy(buffer, filesInfo -> d_name);
			n = send(socksfd, buffer, BUFFSIZE, 0);
			if(n < 0) syserr("can't send filename to tracker");
		}  		
  	}
	memset(buffer, 0, sizeof(buffer));
	strcpy(buffer, "EndOfList");
	n = send(socksfd, buffer, BUFFSIZE, 0);
	if(n < 0) syserr("can't send EndOfList to tracker");
  }
  
  //Input Commands
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
  		//Send command, get size of list
  		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, "list");
		n = send(socksfd, buffer, BUFFSIZE, 0);
		if(n < 0) syserr("can't send command to tracker");
		n = recv(socksfd, &size, sizeof(int), 0); 
        if(n < 0) syserr("can't receive size of list from tracker");
        listLen = ntohl(size);
        
        //Make the List POSSIBLE MEM LEAK
        head = curr = tail = NULL;
        int i;
        for(i=1; i<=listLen; i++)
        {
        	curr = (fileList *)malloc(sizeof(fileList));
  			filename = malloc(sizeof(char)*sizeof(buffer));
        	n = recv(socksfd, &buffer, BUFFSIZE, 0); 
    		if(n < 0) syserr("can't receive filename from tracker");
    		sscanf(buffer, "%s", filename);
    		curr -> filename = filename;
  			
    		uint32_t cIP;
    		n = recv(socksfd, &cIP, sizeof(uint32_t), 0); 
    		if(n < 0) syserr("can't receive IP from tracker");
    		curr -> clientIP = ntohl(cIP);
    		
    		int cP;
    		n = recv(socksfd, &cP, sizeof(int), 0); 
    		if(n < 0) syserr("can't receive port from tracker");
    		curr -> portnum = ntohl(cP);
    		
    		curr -> fl_next = NULL;
        	if(tail == NULL)
        	{
        		tail = curr;
        		head = curr;
        	}
        	else
        	{
				tail -> fl_next = curr;
				tail = curr;
        	}
        } 
        
        //Print out list to console
        curr = head;
        for(i=1; i<=listLen; i++)
        {
        	char peerAddr[INET_ADDRSTRLEN];
        	uint32_t cIP = curr -> clientIP;
			inet_ntop(AF_INET, &cIP, peerAddr, INET_ADDRSTRLEN);
        	printf("[%d] %s %s:%d\n", i, curr -> filename, peerAddr, ntohs(curr -> portnum));
        	curr = curr -> fl_next;
        }
  	}
  	else if(strcmp(command, "exit") == 0)
  	{
  		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, "exit");
		n = send(socksfd, buffer, BUFFSIZE, 0);
		if(n < 0) syserr("can't send command to tracker");
  		n = send(socksfd, &cliP, sizeof(int), 0);
		if(n < 0) syserr("can't send portnum to tracker");
		n = recv(socksfd, &size, sizeof(int), 0);
        size = ntohl(size);  
        if(n < 0) syserr("can't receive exit signal from server");
        
		if(size)
		{
			printf("Connection to server terminated\n");
			break;
		}
		else
		{
			printf("Server didn't exit");
		}
		
  	} 	
  	else if(strcmp(command, "download") == 0)
  	{
  		//Get input, search for file  		
  		filename = malloc(sizeof(char)*sizeof(buffer));
  		strcpy(filename, strtok(NULL, " "));
  		int index = atoi(filename);
  		free(filename);
  		if(listLen <= index) syserr("Index too large for list");
  		curr = head;
  		int j;
  		for(j=1; j<index; j++)
  		{
  			curr = curr -> fl_next;
  		}
  		peer2peer(curr -> clientIP, curr -> portnum, curr -> filename);
  	} 
  	else
  	{
  		printf("Correct commmands are: 'ls-local', 'download <file index>', 				'list', 'exit'\n");
  	}  	
  }
  close(socksfd);
}

void peer2peer(uint32_t cIP, int cP, char * filen)
{
	int sockfd, portno, n, size, tempfd;
	char *filename;
	struct hostent* server; // server info
	struct sockaddr_in serv_addr; //server address info
	char buffer[256];
	char peerAddr[INET_ADDRSTRLEN];
	
	//Convert clientIP to standard dot notation
	inet_ntop(AF_INET, &cIP, peerAddr, INET_ADDRSTRLEN);
	
	server = gethostbyname(peerAddr);
	if(!server) {
		fprintf(stderr, "ERROR: no such host: %s\n", peerAddr);
		return;
	}
	portno = cP;
	filename = filen;

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // check man page
	if(sockfd < 0) syserr("can't open socket");
	printf("create socket...\n");

	// set all to zero, then update the sturct with info
	memset(&serv_addr, 0, sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
	serv_addr.sin_port = portno; 	

	// connect with filde descriptor, server address and size of addr
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	syserr("can't connect to server");
	printf("connect...\n");
	printf("Connection established. Getting files...\n");
	
	//Get the file
	memset(buffer, 0, sizeof(buffer));
	strcpy(buffer, filename);
	n = send(sockfd, buffer, BUFFSIZE, 0);
	if(n < 0) syserr("can't send filename to peer");
	n = recv(sockfd, &size, sizeof(int), 0); 
    if(n < 0) syserr("can't receive size of file from peer");
    size = ntohl(size);        
	if(size ==0) // check if file exists
	{
		printf("File not found at peer\n");
		return;
	}
	printf("The size of the file to recieve is: %d\n", size);
	tempfd = open(filename, O_CREAT | O_WRONLY, 0666);
	if(tempfd < 0) syserr("failed to get file");
	recvandwrite(tempfd, sockfd, size, buffer);
	printf("Download of '%s' was successful\n", filename);  
	close(tempfd);
	
	//Close connection
	n = recv(sockfd, &size, sizeof(int), 0);
    size = ntohl(size);  
    if(n < 0) syserr("can't receive exit signal from server");
    
	//printf("size was %d from server\n", size);
	if(size)
	{
		printf("Connection to server terminated\n");
	}
	else
	{
		printf("Server didn't exit");
	}	
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
