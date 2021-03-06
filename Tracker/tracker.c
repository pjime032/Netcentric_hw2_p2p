#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <pthread.h>
#define BUFFSIZE 256

void syserr(char *msg) { perror(msg); exit(-1); }
void readandsend(int tempfd, int newsockfd, char* buffer);
void * trccomm(void * s);
void recvandwrite(int tempfd, int newsockfd, int size, char* buffer);
//Shared List
typedef struct l {

  char * filename;
  uint32_t clientIP;
  int portnum;
  struct l * fl_next;
} fileList;

fileList * head = NULL;
fileList * curr = NULL;
fileList * tail = NULL;
int listLen = 0;

pthread_mutex_t llock; // List Lock
pthread_t pthread; //peer thread

//Thread struct
typedef struct s {
   int nsock;
   struct sockaddr_in* clientInfo;
} sockStruct;
   

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno;
  struct sockaddr_in serv_addr, clt_addr;
  socklen_t addrlen;  

  if(argc != 2) 
  { 
    portno = 5000;
    printf("Default port is: %d\n", portno);
  }
  else
  { 
  	portno = atoi(argv[1]);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0); 
  if(sockfd < 0) syserr("can't open socket"); 
  	printf("create socket...\n");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
    syserr("can't bind");
  printf("bind socket to port %d...\n", portno);

  listen(sockfd, 5); 

for(;;) {
  printf("wait on port %d...\n", portno);
  addrlen = sizeof(clt_addr); 
  newsockfd = accept(sockfd, (struct sockaddr*)&clt_addr, &addrlen);
  if(newsockfd < 0) syserr("can't accept");
  
  sockStruct * s;
  s = (sockStruct *)malloc(sizeof(sockStruct));
  s -> nsock = newsockfd;
  s -> clientInfo = &clt_addr;
  
  pthread_mutex_init(&llock, NULL); 
  if(pthread_create(&pthread, NULL, trccomm, (void *) s))
  	syserr("Thread was not created.\n");
  //close(sockfd);
}
  close(sockfd); 
  return 0;
}

void * trccomm(void * s)
{
	int n, size, newsockfd, clientPort;
	uint32_t clientIP;
	char * fname;
	char command[20];
    char buffer[256];
	
	//Thread struct operations
	sockStruct* sCInfo = (sockStruct *) s;
	newsockfd = sCInfo -> nsock;
	clientIP = sCInfo -> clientInfo -> sin_addr.s_addr;
	
	//Populate list with client files, IP and port
	pthread_mutex_lock(&llock);
	
	n = recv(newsockfd, &clientPort, sizeof(int), 0);
	if(n < 0) syserr("can't receive files from peer");
	int cliPort = ntohs(clientPort);
	printf("Client Port is: %d\n", cliPort);
	
	// Get filenames from peer
	for(;;)
	{	
    	memset(buffer, 0, BUFFSIZE);
		curr = (fileList *)malloc(sizeof(fileList));
		fname = malloc(sizeof(char)*BUFFSIZE);
		curr -> portnum = clientPort;
		curr -> clientIP = clientIP;
		
		n = recv(newsockfd, buffer, BUFFSIZE, 0);
		if(n < 0) syserr("can't receive files from peer");
		if(strcmp(buffer, "EndOfList") == 0) break;
		
		sscanf(buffer, "%s", fname);
		printf("Filename received is: %s\n", fname);
		curr -> filename = fname;
		curr -> fl_next = NULL;
		
		if(tail == NULL)
		{
		  tail = curr;
		  head = curr;
		  listLen++;	
		}
		else
		{
		  tail -> fl_next = curr;
		  tail = curr;
		  listLen++;
		}			
	}
	pthread_mutex_unlock(&llock);
	
	for(;;)
	{
	    memset(buffer, 0, BUFFSIZE); 
		n = recv(newsockfd, buffer, BUFFSIZE, 0);
		if(n < 0) syserr("can't receive command from client");
		sscanf(buffer, "%s", command);
		printf("message from client is: %s\n", buffer);
		
		if(strcmp(command, "list") == 0)
		{
			pthread_mutex_lock(&llock);
            size = htonl(listLen);      
			n = send(newsockfd, &size, sizeof(int), 0);
		    if(n < 0) syserr("couldn't send listLen to client");
		    curr = head;
			while(curr)
			{
				memset(buffer, 0, BUFFSIZE);
				strcpy(buffer, curr -> filename);
  				printf("The filename sent to peer is: %s\n", buffer);
				n = send(newsockfd, &buffer, BUFFSIZE, 0);
		    	if(n < 0) syserr("couldn't send filename to client");
				int cIP = htonl(curr -> clientIP); 
				n = send(newsockfd, &cIP, sizeof(uint32_t), 0);
		    	if(n < 0) syserr("couldn't send clientIP to client");
				int cP = htonl(curr -> portnum); 
				n = send(newsockfd, &cP, sizeof(int), 0);
		    	if(n < 0) syserr("couldn't send clientPort to client");
		    	
		    	curr = curr -> fl_next;
			}
			pthread_mutex_unlock(&llock);
		}
		
		if(strcmp(command, "exit") == 0)
		{
			int cport;
			n = recv(newsockfd, &cport, sizeof(int), 0);
			if(n < 0) syserr("can't receive files from peer");
			
        	char peerAddr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientIP, peerAddr, INET_ADDRSTRLEN);
			printf("Connection to client %s shutting down\n", peerAddr);
			fileList * dltptr = NULL;
			
			pthread_mutex_lock(&llock);
			curr = head;
			if(curr -> clientIP == clientIP && curr -> portnum == cport)
			{
				do
				{
					head = curr -> fl_next;
					free(curr);
					curr = head;
					listLen--;
					if(head == NULL)
					{
						tail = NULL;
						break; // case: list emptied out
					}
				} while(curr -> clientIP == clientIP && curr -> portnum == cport); 
			}
			else
			{
				while(!(curr -> fl_next -> clientIP == clientIP && curr -> fl_next -> portnum == cport)) 
				{
					curr = curr -> fl_next;
				}
				do
				{
					dltptr = curr -> fl_next;
					curr -> fl_next = curr -> fl_next -> fl_next;
					listLen--;
					free(dltptr);
					dltptr = NULL;
					if(curr -> fl_next == NULL)
					{
						tail = curr;
						break;					
					}
				} while(curr -> fl_next -> clientIP == clientIP && curr -> fl_next -> portnum == cport);
			}
			pthread_mutex_unlock(&llock);
						
			int i = 1;
			i = htonl(i);
			n = send(newsockfd, &i, sizeof(int), 0);
		    if(n < 0) syserr("didn't send exit signal to client");
				break; 
		}
	}
	return 0;
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
		
		int total = 0;
		int n;
		int bytesleft = bytes_read;
		while(total < bytes_read)
		{
			n = send(newsockfd, buffer+total, bytesleft, 0);
			if (n == -1) 
			{ 
			   syserr("error sending file"); 
			   break;
			}
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
