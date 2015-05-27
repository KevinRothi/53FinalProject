/* Bitch I'm fabulous */
#include "csapp.h"

#define PROXY_LOG "proxy.log"
FILE * logfile;

void process_request(int connfd, struct sockaddr_in* clientaddr);
int parse_uri(char* uri, char* hostname, char* pathname, int* port);
void format_log_entry(char* log_entry, struct sockaddr_in* sockaddr, char* uri, int size);

int main(int argc, char ** argv)
{
	int listenfd;
	int port;
	int clientlen;
	int connfd;
	struct sockaddr_in clientaddr;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <port number>\n",argv[0]);
		exit(0);
	}
	signal(SIGPIPE,SIG_IGN);
	port = atoi(argv[1]);
	listenfd = Open_listenfd(port);
	logfile = Fopen(PROXY_LOG,"a");
	while(1)
	{
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
		process_request(connfd, &clientaddr);
	}	
	exit(0);
} 

void process_request(int connfd, struct sockaddr_in* clientaddr)
{	
	printf("connection made\n");
	close(connfd);
}

int parse_uri(char* uri, char* hostname, char* pathname, int* port)
{
	return 0;
}

void format_log_entry(char* log_entry, struct sockaddr_in* sockaddr, char* uri, int size)
{
	
}
