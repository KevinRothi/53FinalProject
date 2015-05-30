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
	//we'll ignore any signals resulting from writing to a closed connection
	signal(SIGPIPE,SIG_IGN);
	//grab the port number
	port = atoi(argv[1]);

	//Open the connection
	listenfd = Open_listenfd(port);

	//open up the log file, and set it to append (so we don't overwrite anything).
	logfile = Fopen(PROXY_LOG,"a");
	
	//print out a status so we know the proxy is running
	printf("Accepting connections at port %d...\n", port);

	while(1)
	{
		
		clientlen = sizeof(clientaddr);
		//Accept a request from a client, then process it.
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
		process_request(connfd, &clientaddr);
	}

	Fclose(logfile);
	exit(0);
} 

void process_request(int connfd, struct sockaddr_in* clientaddr) {	
	int serverfd; //we'll open a connection to the server to read from.
	char *request; //the http request we pull from the client
	char *request_uri; //pointer to the start of the URI from the request
	char *request_uri_end; //end of the URI
	char *rest_of_request; //after URI
	int request_len; //size of the request
	int response_len; //size of the response (from the server)
	int i,n; //some counting/iteration vars
	int realloc_factor; //We'll use this to increase the size of the request/response buf

	char hostname[MAXLINE]; //we'll extract the hostname from the URI
	char pathname[MAXLINE]; //extract pathname from URI
	int port; //port number from URI -- defaults to 80

	char log_entry[MAXLINE]; //log entry

	rio_t rio; //robust input/output

	char buf[MAXLINE];

	//We'll read the request one line at a time.
	request = (char*)Malloc(MAXLINE);
	request[0] = '\0';
	realloc_factor = 2;
	request_len = 0;
	Rio_readinitb(&rio, connfd);
	while(1) {
		if((n = Rio_readlineb_w(&rio, buf, MAXLINE)) <= 0) {
			printf("process_request: client issued a bad request (1).\n");
			close(connfd);
			free(request);
			return NULL;
		}
		//make sure request buffer is large enough;
		if(request_len + n + 1 > MAXLINE) {
			Realloc(request, MAXLINE*realloc_factor++);
		}
		//copy the newly read line into the request
		strcat(request, buf);
		request_len += n;

		//terminate the HTTP request with a blank line
		if(strcmp(buf, "\r\n") == 0) {
			break;
		}
	}

	//TODO: does it only need to handle GET requests?
	if(strncmp(request, "GET ", strlen("get "))) {
		printf("process_request: Received non-GET request\n");
		close(connfd);
		free(request);
		return NULL;
	}
	request_uri = request + 4; //the URI occurs right after the GET

	//find URI
	request_uri_end = NULL; //initially we don't know where the end of the URI is.
	for(i = 0; i < request_len; ++i) {
		//URI ends at a space
		if(request_uri[i] == ' ') {
			request_uri[i] = '\0'; //change that space to a null terminator for parsing
			request_uri_end = &request_uri[i]; //set the end of the uri
			break;
		}
	}

	//if we found the end before a blank, then the request is not properly formatted
	if(i == request_len) {
		printf("process_request: Couldn't find end of URI\n");
		close(connfd);
		free(request);
		return NULL;
	}

	//make sure HTTP version follows URI
	if(strncmp(request_uri_end + 1, "HTTP/1.0\r\n", strlen("HTTP/1.0\r\n")) &&
	strncmp(request_uri_end + 1, "HTTP/1.1\r\n", strlen("HTTP/1.1\r\n"))) {
		printf("process_request: client issued a bad request (4).\n");
		close(connfd);
		free(request);
		return NULL;
	}

	//The rest of the request follow the HTTP directive
	rest_of_request = request_uri_end + strlen("HTTP/1.0\r\n") + 1;

	//parse URI. [hostname]:[port]/[pathname] -> hostname, pathname, port
	if(parse_uri(request_uri, hostname, pathname, &port) < 0) {
		printf("process_request: cannot parse URI\n");
		close(connfd);
		free(request);
		return NULL;
	}

	//forward request to the end server
	if((serverfd = open_clientfd(hostname, port)) < 0) {
		printf("process_request: Unable to connect to end server.\n");
		//close(connfd); //?
		free(request);
		return NULL;
	}
	
	//write the full request to the server.
	Rio_writen_w(serverfd, "GET /", strlen("GET /"));
	Rio_writen_w(serverfd, pathname, strlen(pathname));
	Rio_writen_w(serverfd, " HTTP/1.0\r\n", strlen(" HTTP/1.0\r\n"));
	Rio_writen_w(serverfd, rest_of_request, strlen(rest_of_request));
	
	//initialize reader to read from server
	Rio_readinitb(&rio, serverfd);
	response_len = 0;
	//continually read the response, line by line
	while((n = Rio_readn_w(serverfd, buf, MAXLINE)) > 0) {
		response_len += n; //keep track of how long the response is
		Rio_writen_w(connfd, buf, n); //write the line from the server to the client
		bzero(buf, MAXLINE); //zero out the buffer
	}

	//log the request
	format_log_entry(log_entry, &clientaddr, request_uri, response_len);
	fprintf(log_file, "%s %d\n", log_entry, response_len); //put to file
	fflush(log_file);
	printf("%s %d\n", log_entry, response_len); //also print to console.

	//do some cleanup
	close(connfd);
	close(serverfd);
	free(request);
	return NULL;
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
	hostend = strpbrk(hostbegin, " :/\r\r\0"); //Take the rest of the string until one of these chars
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
		strcpy(pathname, pathbegin);
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
