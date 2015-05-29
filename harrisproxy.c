/* Bitch I'm fabulous */
#include "csapp.h"

#define PROXY_LOG "proxy.log"
FILE * logfile;

void process_request(int connfd, struct sockaddr_in* clientaddr);
int parse_uri(char* uri, char* hostname, char* pathname, int* port);
void format_log_entry(char* log_entry, struct sockaddr_in* sockaddr, char* uri, int size);

int main(int argc, char ** argv) {
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
	
	printf("Accepting connections at port %d...\n", port);

	while(1)
	{
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
		process_request(connfd, &clientaddr);
	}

	close(logfile);
	exit(0);
} 

void process_request(int connfd, struct sockaddr_in* clientaddr) {	
	printf("connection made\n");
	close(connfd);
}

int parse_uri(char* uri, char* hostname, char* pathname, int* port) {
	char *hostbegin;
	char *hostend;
	char *pathbegin;
	int len;

	//URI should be in format: "http://[hostname]:[port]/[pathname]"

	//Check to make sure http protocol is specified. Otherwise we won't handle!
	if(strncasecmp(uri, "http://", 7) !=0) {
		hostname[0] = '\0';
		return -1;
	}

	//extract the host name out of the uri
	hostbegin = uri + 7; //move past the protocol
	hostend = strbrk(hostbegin, " :/\r\r\0"); //Take the rest of the string until one of these chars
	len = hostend - hostbegin;
	strncpy(hostname, hostbegin, len); //copy the string into host name
	hostname[len] = '\0'; //Make sure to null terminate

	//get port
	*port = 80; //set a default port in case user does not specify one.
	if (*hostend == ':') {
		*port = atoi(hostend + 1); //parse the port if there is one specified
	}

	//extract path
	pathbegin = strchr(hostbegin, '/'); //take location right after the '/' char (where path should be)
	if(pathbegin == NULL) { //no path
		pathname[0] = '\0';
	} else { //take the rest of the uri string
		pathbegin++;
		strcpy(pathname, pathbegin)
	}
	return 0;
}

void format_log_entry(char* log_entry, struct sockaddr_in* sockaddr, char* uri, int size) {
	//format = Date: browserIP URL size
	time_t now;
	char time_str[MAXLINE];
	unsigned long host;
	unsigned char a, b, c, d;

	now = time(NULL); //get current time
	//format time as a string: [week day] [month day] [month] [year] [hour]:[minute]:[second] [daylight savings]
	strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

	//convert IP address to host byte order
	host = ntohl(sockaddr->sin_addr.s_addr);
	//convert to hex format
	a = host >> 24; //7,8 hex digits
	b = (host >> 16) & 0xff; //5,6 hex digits
	c = (host >> 8) & 0xff; //3,4 hex digits
	d = host & 0xff; //1,2 hex digits
	//hex address is a:b:c:d
	
	//put fully formatted string into log entry
	sprintf(log_entry, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}
