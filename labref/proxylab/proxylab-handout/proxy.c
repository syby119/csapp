#define _GNU_SOURCE // strcasestr
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "csapp.h"

#define VERBOSE

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_OBJECT_COUNT (MAX_CACHE_SIZE / MAX_OBJECT_SIZE)

#define MAX_HEADER_COUNT 20

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = 
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

enum StatusCode {
    OK = 200,
    Created = 201,
    Accepted = 202,
    NoContent = 204,
    MovedPermanently = 301,
    MovedTemporarily = 302,
    NotModified = 304,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404, 
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
};

typedef struct timeval timeval_t;

typedef struct RequestLine {
    char method[MAXLINE];
    char version[MAXLINE];
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
} RequestLine;

typedef struct RequestHeader {
    char content[MAXLINE];
} RequestHeader;

typedef struct Request {
    RequestLine line;
    RequestHeader headers[MAX_HEADER_COUNT];
    int nheaders;
} Request;

typedef struct CacheObject {
    timeval_t timestamp;          /* for LRU replacement */
    char* key;                    /* key for cached object */
    char* data;                   /* cached data */
    int size;                     /* size of the cache object */
    int nreaders;                 /* number of readers */
    sem_t mutex;                  /* protect nreaders */
    sem_t rwlock;                 /* reader-writer's lock */
} CacheObject;

typedef struct Cache {
    CacheObject objects[MAX_OBJECT_COUNT];
} Cache;

Cache cache;

void init_cache();
int read_cache(int clientfd, const char* key);
void write_cache(char* buf, int size, const char* key);
void deinit_cache();

void status_message(enum StatusCode code, char* buf, int maxlen);
void proxy_error(int connfd, enum StatusCode code, const char* details);

void parse_uri(const char* uri, RequestLine* request_line);
void print_request_line(const RequestLine* request_line);
int generate_key(RequestLine* line, char* key, int maxlen);
int is_least_recently(const timeval_t* t1, const timeval_t* t2);

int forward_request(int clientfd, Request* request);
int backward_response_from_cache(int clientfd, const char* key);
int backward_response_from_server(int clientfd, int serverfd, const char* key);
void* proxy_routine(void* argp);

int main(int argc, char* argv[]) {
    int rc;
    int *clientfdp;
    pthread_t tid;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char hostname[MAXLINE], port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>", argv[0]);
        exit(1);
    }

    /* init cache */
    init_cache(cache);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "overwrite signal handler for SIGPIPE failure");
        exit(1);
    }

    /* listen */
    int listenfd = Open_listenfd(argv[1]);

    /* main loop */
    while (1) {
        clientfdp = (int*)malloc(sizeof(int));
        if (clientfdp == NULL) {
            fprintf(stderr, "malloc error");
            continue;
        }

        /* accept */
        clientlen = sizeof(clientaddr);
        *clientfdp = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (*clientfdp < 0) {
            fprintf(stderr, "accept error");
            free(clientfdp);
            continue;
        }

        /* get info */
        if ((rc = getnameinfo((struct sockaddr*)&clientaddr,
                              clientlen, hostname, MAXLINE, port, MAXLINE, 0))) {
            fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(rc));
            proxy_error(*clientfdp, Unauthorized, "getnameinfo error");
            close(*clientfdp);
            free(clientfdp);
            continue;
        }

#ifdef VERBOSE
        printf("Accepted connection from (%s, %s)\n", hostname, port);
#endif

        /* create thread to server the request */
        if (pthread_create(&tid, NULL, proxy_routine, clientfdp) != 0) {
            proxy_error(*clientfdp, InternalServerError, "pthread_create error");
            close(*clientfdp);
            free(clientfdp);
            continue;
        }

#ifdef VERBOSE
        printf("Create thread %ld to handle proxy routine\n", tid);
#endif
    }

    /* destroy cache, never reached */
    deinit_cache();

    return 0;
}

void init_cache() {
    for (int i = 0; i < MAX_OBJECT_COUNT; ++i) {
        cache.objects[i].timestamp.tv_sec = 0;
        cache.objects[i].timestamp.tv_usec = 0;
        cache.objects[i].key = Malloc(MAXLINE);
        cache.objects[i].data = Malloc(MAX_OBJECT_SIZE);
        cache.objects[i].size = 0;
        cache.objects[i].nreaders = 0;
        Sem_init(&cache.objects[i].mutex, 0, 1);
        Sem_init(&cache.objects[i].rwlock, 0, 1);
    }
}

int read_cache(int clientfd, const char* key) {
    int found = 0;
    for (int i = 0; i < MAX_OBJECT_COUNT; ++i) {
        /* lock mutex for nreaders */
        P(&cache.objects[i].mutex);
        cache.objects[i].nreaders++;

        /* if first reader, lock the rwlock to prevent data race with writter */
        if (cache.objects[i].nreaders == 1) {
            P(&cache.objects[i].rwlock);
        }

        /* unlock mutex for other readers */
        V(&cache.objects[i].mutex);

        /* begin rwlock critical section */
        if (strcmp(cache.objects[i].key, key) == 0) {
            found = 1;
            /* LRU replacement */
            P(&cache.objects[i].mutex);
            gettimeofday(&cache.objects[i].timestamp, NULL);
            V(&cache.objects[i].mutex);

            /* send data to the client */
            int len = cache.objects[i].size;
            if (len != rio_writen(clientfd, cache.objects[i].data, len)) {
                fprintf(stderr, "Proxy write data to client failure\n");
            }
        }
        /* end rwlock critical section */

        /* lock mutex for nreaders */
        P(&cache.objects[i].mutex);
        cache.objects[i].nreaders--;
        
        /* if no other readers, unlock the rwlock for other readers/writters */
        if (cache.objects[i].nreaders == 0) {
            V(&cache.objects[i].rwlock);
        }


        /* unlock mutex for other readers/writters */
        V(&cache.objects[i].mutex);

        if (found) {
            break;
        }
    }

#ifdef VERBOSE
    printf("read_cache: %d\n", found);
#endif

    return found;
}

void write_cache(char* buf, int size, const char* key) {
    /* find the victim using the approximate LRU, 
       despite the timestamp may get changed during comparision */
    int index = 0;
    timeval_t min_timestamp = { LONG_MAX, LONG_MAX };
    for (int i = 0; i < MAX_OBJECT_COUNT; ++i) {
        P(&cache.objects[i].mutex);
        if (is_least_recently(&cache.objects[i].timestamp, &min_timestamp)) {
            index = i;
            min_timestamp = cache.objects[i].timestamp;
        }        
        V(&cache.objects[i].mutex);
    }

    /* begin rwlock critical section */
    P(&cache.objects[index].rwlock);

    /* update timestamp */
    gettimeofday(&cache.objects[index].timestamp, NULL);

    /* update key */
    strcpy(cache.objects[index].key, key);

    /* update data and its size */
    memcpy(cache.objects[index].data, buf, size);
    cache.objects[index].size = size;

    V(&cache.objects[index].rwlock);
    /* end rwlock critical section  */

#ifdef VERBOSE
    printf("write to cache object %d\n", index);
#endif
}

void deinit_cache() {
    for (int i = 0; i < MAX_OBJECT_COUNT; ++i) {
        free(cache.objects[i].key);
        free(cache.objects[i].data);
        sem_destroy(&cache.objects[i].mutex);
        sem_destroy(&cache.objects[i].rwlock);
    }
}

void parse_uri(const char* uri, RequestLine* request_line) {
    // According to HTTP/1.0 Protocal
    // https://datatracker.ietf.org/doc/html/rfc1945#section-5.1,
    // if method is GET, and proxy used, then the request line will be
    //     GET SP Request-URI SP HTTP/1.0 CLRF
    //     the Request-URI should be absoluteURI:
    //         http://www.xxx.com[:port][/path]
    // According to HTTP/1.1 Protocol 
    // https://datatracker.ietf.org/doc/html/rfc2616#page-36,
    // if method is GET, then the request line will be
    //     GET SP /abs/path SP HTTP/1.1 CLRF, followed by
    //     Host: SP http://www.xxxx.com:port CLRF
    // According to HTTP/1.1 Protocol 
    // https://datatracker.ietf.org/doc/html/rfc2616#page-36,
    // if method is CONNECT, the request is made to a proxy,
    // then the request line will be
    //     CONNECT SP Request-URI SP HTTP/1.1 CLRF
    //     the Request-URI will be absoluteURI:
    //         http://www.xxx.com[:port][/path]

    // We'll assume the proxy to be absoluteURI as request is sent to the proxy
    // if (strcasecmp(request_line->method, "CONNECT") == 0) {
    //     strcpy(request_line->method, "GET");
    // }

    strcpy(request_line->port, "80");

    // const char* https_prefix = "https://";
    // if (strncmp(uri, https_prefix, strlen(https_prefix)) == 0) {
    //     strcpy(request_line->port, "443");
    // }

    const char* p = strstr(uri, "://");
    p = (p ? p + 3 : uri);
    int host_pos = 0, port_pos = 0, path_pos = 0;
    int port_detected = 0, path_detected = 0;

    for (char c = *p; c != '\0'; c = *++p) {
        if (path_detected) {
            request_line->path[path_pos++] = c;
        } else if (port_detected) {
            if (c == '/') {
                path_detected = 1;
                request_line->path[path_pos++] = c;
            } else {
                request_line->port[port_pos++] = c;
            }
        } else {
            if (c == '/') {
                path_detected = 1;
                request_line->path[path_pos++] = c;
            } else if (c == ':') {
                port_detected = 1;
            } else {
                request_line->host[host_pos++] = c;
            }
        }
    }

    request_line->host[host_pos] = '\0';

    if (port_detected) {
        request_line->port[port_pos] = '\0';
    }

    if (path_pos == 0) {
        request_line->path[path_pos++] = '/';
    }
    request_line->path[path_pos] = '\0';
}

void status_message(enum StatusCode code, char* buf, int maxlen) {
    char message[32];
    switch (code) {
        case OK:                  strcpy(message, "OK");                    break;
        case Created:             strcpy(message, "Created");               break;
        case Accepted:            strcpy(message, "Accepted");              break;
        case NoContent:           strcpy(message, "No Content");            break;
        case MovedPermanently:    strcpy(message, "Moved Permanently");     break;
        case MovedTemporarily:    strcpy(message, "Moved Temporarily");     break;
        case NotModified:         strcpy(message, "Not Modified");          break;
        case BadRequest:          strcpy(message, "Bad Request");           break;
        case Unauthorized:        strcpy(message, "Unauthorized");          break;
        case Forbidden:           strcpy(message, "Forbidden");             break;
        case NotFound:            strcpy(message, "Not Found");             break;
        case InternalServerError: strcpy(message, "Internal Server Error"); break;
        case NotImplemented:      strcpy(message, "Not Implemented");       break;
        case BadGateway:          strcpy(message, "Bad Gateway");           break;
        case ServiceUnavailable:  strcpy(message, "Service Unavailable");   break;
        default:                  strcpy(message, "Unknown");               break;
    }

    int message_len = strlen(message);
    if (message_len + 1 > maxlen) {
        message_len = maxlen - 1;
    }

    memcpy(buf, message, message_len);
    buf[message_len] = '\0';
}

void proxy_error(int connfd, enum StatusCode code, const char* details) {
    char buf[MAXLINE];

    const char* err_template = 
        "<html>\r\n"
        "<head>\r\n"
        "  <title>Proxy Error</title>\r\n"
        "</head>\r\n"
        "<body>\r\n"
        "  <h1>HTTP Status %d - %s</h1>\r\n"
        "  <p>%s</p>\r\n"
        "</body>\r\n"
        "</html>\r\n";

    char message[32];
    status_message(code, message, sizeof(message));

    /* HTTP response header */
    sprintf(buf, "HTTP/1.0 %d %s\r\n", code, message);
    rio_writen(connfd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    rio_writen(connfd, buf, strlen(buf));

    /* HTTP response body */
    sprintf(buf, err_template, code, message, details);
    rio_writen(connfd, buf, strlen(buf));
}

int parse_request(int clientfd, Request* request) {
    rio_t rio;
    char line[MAXLINE];
    char err_message[MAXLINE];

    Rio_readinitb(&rio, clientfd);
    /* 1. receive request from client */
    /* 1.1 read the request line: Method URI HTTP/version */
    int rc = rio_readlineb(&rio, line, MAXLINE);
    if (rc == -1) { /* error */
        sprintf(err_message, "rio_readlineb: ");
        int len = strlen(err_message);
        strerror_r(errno, err_message + len, MAXLINE - len);
        fprintf(stderr, "%s", err_message);
        proxy_error(clientfd, InternalServerError, err_message);
        return 0;
    } else if (rc == 0) {  /* no data */
        proxy_error(clientfd, NoContent, "No actual data received");
        return 0;
    } else {
        if (rc == MAXLINE - 1) {
            fprintf(stderr, "app error: line to long\n");
            proxy_error(clientfd, BadRequest, "Request too long to handle");
            return 0;
        }
    }

    /* 1.2 parse the request line */
    char uri[MAXLINE];
    if (sscanf(line, "%s %s %s", request->line.method, uri, request->line.version) != 3) {
        sprintf(err_message, "invalid http request line: %s", line);
        fprintf(stderr, "%s", err_message);
        proxy_error(clientfd, BadRequest, err_message);
        return 0;
    }

    if (strcasecmp(request->line.method, "GET")) {
        fprintf(stderr, "Proxy doesn't support %s\n", request->line.method);
        proxy_error(clientfd, NotImplemented, "Proxy only support GET method");
        return 0;
    }

    if (strcasecmp(request->line.version, "HTTP/1.1") == 0) {
        strcpy(request->line.version, "HTTP/1.0");
    }

    parse_uri(uri, &request->line);

    /* 1.3 parse request headers */
    request->nheaders = 0;
    while(Rio_readlineb(&rio, line, MAXLINE) > 0) {
        /* end of request header */
        if (strcmp(line, "\r\n") == 0) {
            break;
        }

        strcpy(request->headers[request->nheaders++].content, line);
        if (request->nheaders == MAX_HEADER_COUNT) {
            fprintf(stderr, "app error: request headers to long, need code optimization\n");
            proxy_error(clientfd, InternalServerError, "Proxy run out of memory for request headers");
            return 0;
        }
    }

    return 1;
}

int generate_key(RequestLine* request_line, char* key, int maxlen) {
    /* host */ 
    char* p = request_line->host;
    for (char ch = *p; ch != '\0'; ch = *++p) {
        if (--maxlen <= 0) {
            return 0;
        }

        *(key++) = ch;
    }

    /* path */
    p = request_line->path;
    for (char ch = *p; ch != '\0'; ch = *++p) {
        if (--maxlen <= 0) {
            return 0;
        }

        *(key++) = ch;
    }

    *key = '\0';

    return 1;
}

int is_least_recently(const timeval_t* t1, const timeval_t* t2) {
    if ((t1->tv_sec < t2->tv_sec) || 
        ((t1->tv_sec == t2->tv_sec) && (t1->tv_usec < t2->tv_usec))) {
        return 1;
    } else {
        return 0;
    }
}

int forward_request(int clientfd, Request* request) {
    char new_request[MAXBUF];
    sprintf(new_request, "%s %s %s\r\n", 
            request->line.method, request->line.path, request->line.version);
    strcat(new_request, "Host: ");
    strcat(new_request, request->line.host);
    strcat(new_request, ":");
    strcat(new_request, request->line.port);
    strcat(new_request, "\r\n");
    strcat(new_request, "Connection: close\r\n");
    strcat(new_request, "Proxy-Connection: close\r\n");

    int find_user_agent_hdr = 0;
    for (int i = 0; i < request->nheaders; ++i) {
        char* content = request->headers[i].content;
        /* filter some fields */
        if (strcasestr(content, "Host:") ||
            strcasestr(content, "Connection:") || 
            strcasestr(content, "Proxy-Connection:")) {
            continue;
        }

        if (strcasestr(content, "User-Agent:")) {
            find_user_agent_hdr = 1;
        }

        /* fill in the fields */
        strcat(new_request, content);
    }

    if (!find_user_agent_hdr) {
        strcat(new_request, user_agent_hdr);
    }

    strcat(new_request, "\r\n");

#ifdef VERBOSE
    printf("\nNew request:\n%s", new_request);
#endif

    /* 2 forward request to server */
    /* 2.1 establish connection to server */
    int serverfd = open_clientfd(request->line.host, request->line.port);
    if (serverfd < 0) {
        proxy_error(clientfd, BadGateway, "Proxy cannot connect to server");
        return -1;
    }

#ifdef VERBOSE
    printf("Establish connection to (%s, %s)\n", request->line.host, request->line.port);
#endif

    /* 2.2 forward request header */
    int len = strlen(new_request);
    int rc = rio_writen(serverfd, new_request, strlen(new_request));
    if (rc != len) {
        proxy_error(clientfd, BadGateway, "Proxy forward header failure");
        close(serverfd);
        return -1;
    }

    return serverfd;
}

int backward_response_from_cache(int clientfd, const char* key) {
    return read_cache(clientfd, key);
}

int backward_response_from_server(int clientfd, int serverfd, const char* key) {
    rio_t rio;
    rio_readinitb(&rio, serverfd);

    char buf[MAXLINE];
    char object_buf[MAX_OBJECT_SIZE];
    int n, total = 0;
    int success = 1;

    while ((n = rio_readnb(&rio, buf, MAXLINE))) {
#ifdef VERBOSE
        printf("Receive %d btyes from server\n", n);
#endif

        if (n == -1) {
            fprintf(stderr, "Proxy read data from server failure\n");
            success = 0;
            break;
        }

        if (n != rio_writen(clientfd, buf, n)){
            fprintf(stderr, "Proxy write data to client failure\n");
            success = 0;
            break;
        }

        if (total <= MAX_OBJECT_SIZE) {
            memcpy(object_buf + total, buf, n);
        }

        total += n;
    }

    if (total <= MAX_OBJECT_SIZE) {
        write_cache(object_buf, total, key);
    }

    return success;
}

void* proxy_routine(void* argp) {
    int clientfd = *(int*)argp;
    free(argp);

    /* detach the thread */
    if (pthread_detach(pthread_self()) != 0) {
        fprintf(stderr, "pthread_detach error");
        proxy_error(clientfd, InternalServerError, "pthread_detach error");
        goto CLEAN_UP;
    }

    /* parse request line */
    Request request;
    if (!parse_request(clientfd, &request)) {
        goto CLEAN_UP;
    }

    /* generate key */
    char key[MAXLINE];
    if (!generate_key(&request.line, key, sizeof(key))) {
        fprintf(stderr, "generate_key error");
        proxy_error(clientfd, InternalServerError, "generate_key error");
        goto CLEAN_UP;
    }

    /* search cache for response */
    if (backward_response_from_cache(clientfd, key)) {
        goto CLEAN_UP;
    }

    /* forward request to the remote client */
    int serverfd = forward_request(clientfd, &request);
    if (serverfd >= 0) {
        backward_response_from_server(clientfd, serverfd, key);
        close(serverfd);
    }

    printf("%s: %d\n", __func__, __LINE__);

CLEAN_UP:
    close(clientfd);
    return NULL;
}