/*
 * proxy.c - CS:APP Web pr- CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *      Peter Johnston, pemjohns@gmail.com
 *		Scott Young, syoung160@gmail.com     
 * 
 * Description: proxy.c acts as an intermediary between a client and server.
 * Using threads it is able to operate concurently and serve multiple clients.
 * Threads are passed a struct containing all the arguments needed.
 * The log file will start anew every time the proxy is executed.
 * PLEASE NOTE: This works well on a browser assuming the port and host are set
 * AND the entire address is typed in, AND ending in a /
 * i.e. http://www.aol.com/ in the browser or 
 * GET http://www.aol.com/ HTTP/1.0
 * It is very finicky and has issues without exact commands
 */ 
 
 #include "csapp.h"
 
 /* Function Prototypes  */
void server_connect(int serv_arg, char **serv_argv);
void *thread(void *vargp);  // Thread routine
void read_http(int connfd, int port, struct sockaddr_in *sockaddr);

/* Functions given to us */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
int parse_uri(char *uri, char *hostname, char *path, int  *port); 

/* Wrapper Function Prototypes */
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readn_w(int fd, void *usrbuf, size_t n);
void Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t n);

// Log FILE
FILE *proxy_log;

// Global struct to pass to threads
struct thread_info{
	int fd;
	int id;
	struct hostent *hp;
	struct sockaddr_in clientaddr;
	char *haddrp;
	int port;
};

  /* 
  * main - Main routine for the proxy program 
  */
int main(int argc, char **argv)
{
	// Passed to a new function because it helped
	//with vizualization of steps
	server_connect(argc, argv);	exit(0);
}  // End of main()

/* server_connect handles the opening and closing of the log, as well
 * as calling to malloc and creating eatch thread by passing it the 
 * thread_info structure */
void server_connect(int serv_argc, char **serv_argv)
{
	int listenfd, port;
	socklen_t clientlen;	
	struct sockaddr_in clientaddr;
	pthread_t tid;
	int id= 0;
	/* Check arguments*/
	if (serv_argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", serv_argv[0]);
		exit(0);
	}
	// Ignore SIGPIPE
	Signal(SIGPIPE, SIG_IGN); 
	// Open log file and begin new
	proxy_log = Fopen("proxy.log", "w");
	proxy_log = Fopen("proxy.log", "a");
	// Connect to port given
	port = atoi(serv_argv[1] );
	// Listen for connection
	listenfd = Open_listenfd(port);
	while (1) 
	{
		// Create struct for each request and fill it with
		// arguments to be passed to thread
		struct thread_info *td;
		id++;
		clientlen =  sizeof(clientaddr);
		td= Malloc(sizeof(struct thread_info));
		td->fd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		td->clientaddr = clientaddr;
		td->port = port;
		td->id = id;
		printf("Accepted client!\n");
		// Create the thread
		pthread_create(&tid, NULL, thread, td);
	}
	// Close descriptor
	Close(listenfd);
	// Close log file
	Fclose(proxy_log);	
	exit(0);
} // End of server_connect()

/* The thread method gives each thread created connection information
 * then calls read_http */
void *thread(void *vargp)
{
	struct thread_info *td = ((struct thread_info *)vargp);
	struct hostent *hp;
	struct sockaddr_in clientaddr;
	char *haddrp;
	int port;
	int connfd;
	int id;
	
	// Pass in variables from the struct argument received
	connfd = td->fd;
	clientaddr = td->clientaddr;
	id = td->id;
	port = td->port;
	hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
		sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	haddrp = inet_ntoa(clientaddr.sin_addr);
	// Detach the thread
	pthread_detach(pthread_self() );
	// Free the structure Malloc
	Free(vargp);
	// Call read_http to parse request
	read_http(connfd, port, &clientaddr); 
	// Close the descriptor
	Close(connfd);
	return NULL;
} // End of thread()

/*
* parse_uri - URI parser
*  
* Given a URI from an HTTP proxy GET request (i.e., a URL), extract
* the host name, path name, and port.  The memory for hostname and
* pathname must already be allocated and should be at least MAXLINE
* bytes. Return -1 if there are any problems.
*/
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
	char *hostbegin;
	char *hostend;
	char *pathbegin;
	int len;
	
	if (strncasecmp(uri, "http://", 7) != 0) {
		hostname[0] = '\0';
		return -1;
	}
	
	/* Extract the host name */
	hostbegin = uri + 7;
	hostend = strpbrk(hostbegin, " :/\r\n\0");
	len = hostend - hostbegin;
	strncpy(hostname, hostbegin, len);
	hostname[len] = '\0';
	
	/* Extract the port number */
	*port = 80; /* default */
	if (*hostend == ':')   
		*port = atoi(hostend + 1);
	
	/* Extract the path */
	pathbegin = strchr(hostbegin, '/');
	if (pathbegin == NULL) {
		pathname[0] = '\0';
	}
	
	else {
		pathbegin++;	
		strcpy(pathname, pathbegin);
	}
	return 0;
	
} // End of parse_uri()

/*
* format_log_entry - Create a formatted log entry in logstring. 
* 
* The inputs are the socket address of the requesting client
* (sockaddr), the URI from the request (uri), and the size in bytes
* of the response from the server (size).
*/
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size)
{
	time_t now;
	char time_str[MAXLINE];
	unsigned long host;
	unsigned char a, b, c, d;
	/* Get a formatted time string */
	now = time(NULL);
	strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));
	
	/* 
	* Convert the IP address in network byte order to dotted decimal
	* form. Note that we could have used inet_ntoa, but chose not to
	* because inet_ntoa is a Class 3 thread unsafe function that
	* returns a pointer to a static variable (Ch 13, CS:APP).
	*/
	
	host = ntohl(sockaddr->sin_addr.s_addr);
	a = host >> 24;
	b = (host >> 16) & 0xff;
	c = (host >> 8) & 0xff;
	d = host & 0xff;
	
	/* Return the formatted log entry string */
	sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
	
} // End of format_log_entry()

/* read_http parses the request and forwards it to the server
*  It then returns the response from the server to the client
*/
void read_http(int connfd, int port, struct sockaddr_in *sockaddr)
{
	int n;
	int size = 0;
	int parse;
	int ported;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char hostname[MAXLINE], pathname[MAXLINE];
	char log_entry[MAXLINE];
	rio_t rio;
	Rio_readinitb(&rio, connfd);
	struct sockaddr_in *addr = sockaddr;
	int running = 1;
	int hostfd =0;
	rio_t rio_host;
	int done_client = 0;
	while(running)
	{
		// Read the request
		if((n = Rio_readlineb_w(&rio, buf, MAXLINE)) != 0)
		{
			sscanf(buf, "%s %s %s", method, uri, version);
			printf("buf:%s\n", buf);
			if (strcasecmp(method, "GET"))
			{
				printf("This proxy can only accept GET requests.\n");
				return;
			}
			// Parse the uri to retrieve host name and path name
			parse = parse_uri(uri, hostname, pathname, &ported);
			printf("uri: %s\nhostname: %s\npathname: %s\n port: %d\n", uri, hostname, pathname, ported);
			printf("%d\n", parse);

			// Connect to host on hostfd
			if((hostfd = Open_clientfd(hostname, ported)) < 0)
			{
				running = 0;
				break;
			}
			// Send request to host
			Rio_readinitb(&rio_host, hostfd);
			Rio_writen_w(hostfd, buf, strlen(buf));

			done_client = 0;
			while(!done_client && ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0))
			{
				Rio_writen_w(hostfd, buf, n);
				done_client = (buf[0] == '\r');
			}
			
			// Read all response lines from remote host
			while((n = Rio_readnb_w(&rio_host, buf, MAXLINE) ) != 0)
			{
				Rio_writen_w(connfd, buf, n);
				size += n; // Keeps track of size in bytes of the response for logging
			}

			// Send received information to log file
			format_log_entry(log_entry, addr, uri, size);
			fprintf(proxy_log, "%s %d\n", log_entry, size);
			fflush(proxy_log);

			Close(hostfd);
			running = 0;
		}
		else
		{
			running = 0;
		}
	}
}// End of read_http()

// Below are Wrappers for the rio_readnb, rio_readn, rio_writen, and rio_readlinb methods
// These return a warning message on an error rather than terminating the process
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
	size_t nbytes;
	if((nbytes = rio_readnb(rp, usrbuf, n)) < 0)
	{
		printf("Warning: Rio_readnb failed.\n");
		return 0;
	}
	return nbytes;
}
ssize_t Rio_readn_w(int fd, void *usrbuf, size_t n)
{
	size_t nbytes;
	if ((nbytes = rio_readn(fd, usrbuf, n)) < 0)
	{
		printf("Warning: Rio_readn failed.\n");
		return 0;
	}
	return nbytes;
}
void Rio_writen_w(int fd, void *usrbuf, size_t n)
{
	if(rio_writen(fd, usrbuf, n) != n)
	{
		printf("Warning: Rio_writen failed.\n");
		return;
	}
}
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t n)
{
	ssize_t rc;
	if((rc = rio_readlineb(rp, usrbuf, n)) < 0)
	{
		printf("Warning: Rio_readlineb experienced an error.\n");
		return 0;
	}
	return rc;
}
