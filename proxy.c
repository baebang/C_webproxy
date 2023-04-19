#include <stdio.h>
#include "csapp.h"
//http://52.79.166.31:3001/
/* Recommended max cache and object sizes */

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void parse_uri(char *uri, char *host, char* port, char *path);
void doit(int fd) ;
void *thread(void *vargp);

int main(int argc, char **argv) {
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(struct sockaddr_storage);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    // Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
    //             0);
    Pthread_create(&tid, NULL, thread, connfd);

  }
}

void *thread(void *vargp){
  int connfd = *((int *) vargp);
  Pthread_detach(Pthread_self());
  Free(vargp);
  doit(connfd);   // line:netp:tiny:doit
  Close(connfd);  // line:netp:tiny:close
  return NULL;

}

void doit(int fd) {
  int final_fd; //최종 서버에게 보내는 식별자
  char client_buf[MAXLINE], server_buf[MAXLINE];

  char filename[MAXLINE],  version[MAXLINE];
  char uri[MAXLINE], method[MAXLINE],port[MAXLINE],portname[MAXLINE];
  char host[MAXLINE], path[MAXLINE];
  rio_t client_rio, server_rio;
  
  /* Read request line and headers */
  Rio_readinitb(&client_rio, fd);
  Rio_readlineb(&client_rio, client_buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", client_buf);
  sscanf(client_buf, "%s %s %s", method, uri, version); // 클라이언트 요청 정보 메소드,URI,버전 추출
  if (strcasecmp(method, "GET")&&strcasecmp(method, "HEAD")) { // 이외의 메소드는 안받는다.
    printf(fd, method, "501", "Not implemented","Tiny does not implement this method");
    
    return; 
  }

  /* Parse URI from GET request */
  parse_uri(uri, host, port, path );

  final_fd = Open_clientfd(host,port); // 클라이언트의 요청 정보를 서버로 보내기위한 서버 식별자
  sprintf(server_buf, "%s %s %s\r\n", method, path, version); // server_buf에 파싱했던 메소드, 패스, 버전을 넣어주기
  Rio_readinitb(&server_rio, final_fd);
  sprintf(server_buf, "GET %s HTTP/1.0\r\n\r\n", path); // 클라이언트로부터 받은 요청의 헤더를 수정하여 보냄
  Rio_writen(final_fd, server_buf, strlen(server_buf)); // 담았던 모든 정보를 서버 식별자로부터 넘겨주기


  size_t n;
  while((n = Rio_readlineb(&server_rio, server_buf, MAXLINE)) != 0) {
    printf("Proxy received %d bytes from server and sent to client\n", n);
    Rio_writen(fd, server_buf, n);
  }
  Close(final_fd);

  

}


void parse_uri(char *uri, char *hostname, char *port, char *pathname) {
    // 1. "http://" 접두어를 파싱합니다.
    char *start = strstr(uri, "http://");
    char *port_end;
    if (start == NULL) {
        return -1;
    }
    start += strlen("http://");

    // 2. 호스트 이름을 파싱합니다.
    char *end = strstr(start, ":");
    if (end == NULL) {
        end = strstr(start, "/");
        if (end == NULL) {
            return -1;
        }
    }
    strncpy(hostname, start, end - start);

    // 3. 포트 번호를 파싱합니다.
    if (*end == ':') {
        end++;
        char *port_end = strstr(end, "/");
        if (port_end == NULL) {
            return -1;
        }
        strncpy(port, end, port_end - end);
        strcpy(pathname, port_end);

    } else {

        strcpy(port, "80");
        strcpy(pathname,end);

    }


    return 0;
}