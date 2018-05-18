/*
* proxy.c - CS:APP Web proxy
*
* Fan Wentao  5140379060
*
*
* IMPORTANT: Give a high level description of your code here. You
* must also provide a header comment at the beginning of each
* function that describes what that function does.
*/

#include "csapp.h"


/*
* Function prototypes
*/
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
//void echo(int connfd);
void doit(int connfd, struct sockaddr_in *sockaddr);
void *init(void *arg);
int Open_clientfd_ts(char *hostname, int port);
int open_clientfd_ts(char *hostname, int port);
/*global variable*/
sem_t mutex;
sem_t mutex_log;
FILE *logFile;
//FILE *cache;
typedef struct
{
    int connfd;
    struct sockaddr_in sockaddr;
} Arg;
/*
* main - Main routine for the proxy program
*/
int main(int argc, char **argv)
{
    /* variable claim */
    int listenfd, port, clientlen;
    struct sockaddr_in clientaddr;
    //struct hostent *hp;
    //char *haddrp;

    /* Check arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }
    Signal(SIGPIPE, SIG_IGN);
    logFile = fopen("proxy.log","w");
    //cache = fopen("cache","w");
    port = atoi(argv[1]);
    sem_init(&mutex, 0, 1);
    sem_init(&mutex_log, 0, 1);
    listenfd = Open_listenfd(port);
    pthread_t tid;
    while (1){
        clientlen = sizeof(clientaddr);
        //connfd = malloc(sizeof(int));
        /*prepare arguments*/
        Arg *arg = malloc(sizeof(Arg));
        arg->connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        arg->sockaddr = clientaddr;
        printf("connfd in main = %d\n", arg->connfd);

        /*handle in a thread*/
        Pthread_create(&tid,NULL,init,(void *)arg);
        //doit(connfd, &clientaddr);
        printf("line 61\n");

    }

    exit(0);
}

void *init(void *arg)
{
    Pthread_detach(pthread_self());
    Arg *p = (Arg *)arg;
    int connfd = p->connfd;
    struct sockaddr_in sockaddr;
    memcpy(&sockaddr,&(p->sockaddr),sizeof(struct sockaddr_in));
    printf("connfd in init = %d\n", connfd);
    free(arg);
    doit(connfd, &sockaddr);
    //Close(connfd);
}

void doit(int connfd, struct sockaddr_in *sockaddr)
{
    printf("IN doit\n");
    printf("connfd in doit = %d\n", connfd);
    char request[MAXLINE];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE];
    char buf[MAXLINE];
    char logstring[MAXLINE];    
    int port;
    int res_size = 0;
    int size;
    rio_t client_rio, serv_rio,cache_rio;
    Rio_readinitb(&client_rio, connfd);
    Rio_readlineb(&client_rio, request, MAXLINE);

    sscanf(request, "%s %s %s", method, uri, version);
    parse_uri(uri, hostname, pathname, &port);

    printf("line 94 %s %s %d\n", hostname, pathname, port);

    int servfd = open_clientfd_ts(hostname, port);
    char path[MAXLINE];
    sprintf(path,"%s/%s","./cache/",pathname);
    int cachefd = open(path,O_RDWR|O_CREAT,DEF_MODE);
    Rio_readinitb(&cache_rio,cachefd);
    if (servfd < 0){
    /*connect server fail, find in cache*/
        while (Rio_readlineb(&cache_rio, buf, MAXLINE) > 2)
        {
        /*header*/
            printf("%s\n", buf);
            //Rio_writen(connfd, buf, strlen(buf));
        }
        printf(logFile, "Hello! We are in cache.\n");
        fflush(logFile);
        while ((size = Rio_readnb(&cache_rio, buf, MAXLINE)) != 0)
        {
            res_size += size;
            printf("%s\n", buf);
            Rio_writen(connfd, buf, size);
        }
        format_log_entry(logstring, sockaddr, uri, res_size);
        P(&mutex_log);
        fprintf(logFile, "%s\n", logstring);
        fflush(logFile);
        V(&mutex_log);
        Close(servfd);
        Close(connfd);
        Close(cachefd);
        return;
    }
    Rio_readinitb(&serv_rio, servfd);
    strcpy(request, method);
    strcat(request, " /");
    strcat(request, pathname);
    strcat(request, " ");
    strcat(request, version);
    strcat(request, "\r\n");


    printf("line 106 request = %s\n", request);
    /* send request to server*/
    Rio_writen(servfd, request, strlen(request));
    /* send request header*/
    printf("request header :\n");
    while (Rio_readlineb(&client_rio, buf, MAXLINE) > 2)
    {
        printf("%s\n", buf);
        Rio_writen(servfd, buf, strlen(buf));
    }
    Rio_writen(servfd, "\r\n", 2);
    printf("line 117\n");
    /*send response to client*/

    printf("response header :\n");
    while (Rio_readlineb(&serv_rio, buf, MAXLINE) > 2)
    {
        
        printf("%s\n", buf);
        Rio_writen(cachefd, buf,strlen(buf));
        Rio_writen(connfd, buf, strlen(buf));
    }
    printf("line 135 \n");
    //send a empty line
    Rio_writen(connfd, "\r\n", 2);
    Rio_writen(cachefd, "\r\n", 2);

    printf("line 147\n");
    //send back response data

    while ((size = Rio_readnb(&serv_rio, buf, MAXLINE)) != 0)
    {
        res_size += size;
        printf("%s\n", buf);
        Rio_writen(cachefd, buf, size);
        Rio_writen(connfd, buf, size);
    }
    printf("line 157\n");

    format_log_entry(logstring, sockaddr, uri, res_size);
    P(&mutex_log);
    int temp = fprintf(logFile, "%s\n", logstring);
    fflush(logFile);
    V(&mutex_log);
    printf("line 162  %d logstring = %s\n", temp, logstring);
    //fclose(logFile);
    Close(servfd);
    Close(connfd);
    Close(cachefd);
}


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
}

/*
* format_log_entry - Create a formatted log entry in logstring.
*
* The inputs are the socket address of the requesting client
* (sockaddr), the URI from the request (uri), and the size in bytes
* of the response from the server (size).
*/
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
    char *uri, int size)
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
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}


int Open_clientfd_ts(char *hostname, int port)
{
    int rc;

    if ((rc = open_clientfd_ts(hostname, port)) < 0) {
        if (rc == -1)
            unix_error("Open_clientfd Unix error");
        else
            dns_error("Open_clientfd DNS error");
    }
    return rc;
}
int open_clientfd_ts(char *hostname, int port)
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; /* check errno for cause of error */

    /* Fill in the server's IP address and port */
    P(&mutex);
    if ((hp = gethostbyname(hostname)) == NULL){
    	V(&mutex);
        return -2; /* check h_errno for cause of error */
    }
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],
        (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    V(&mutex);
    serveraddr.sin_port = htons(port);

    /* Establish a connection with the server */
    if (connect(clientfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return clientfd;
}