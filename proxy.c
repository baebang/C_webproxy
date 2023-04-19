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


typedef struct cache {
  char uri[MAXLINE];
  char content[MAX_OBJECT_SIZE];
  int size;
  struct cache *prev;
  struct cache *next;
}cache_t; 
struct cache *cache_head = NULL;
int cache_size = 0;


void parse_uri(char *uri, char *host, char* port, char *path);
void doit(int fd) ;
void *thread(void *vargp);
void caching(void *caching);

struct cache *find_cache(char *uri);
void remove_cache();
void insert_cache(int object_size, char *from_server_uri, char *from_server_data);



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
  char uri[MAXLINE], method[MAXLINE],portname[MAXLINE], port[MAXLINE];
  char host[MAXLINE], path[MAXLINE];

  char from_server_uri[MAXLINE], from_server_data[MAX_OBJECT_SIZE];
  char empty_buf[MAXLINE];
  strcpy(empty_buf,"");
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
  parse_uri(uri, host, port, path);

  /*클라이언트로 받는 정보를 확인하여 
    캐시에 해당 요청에 대한 정보가 있는지 확인하는 함수 필요
    만약, 요청 정보가 없다면 캐시에다 저장해야함*/
  char *buffer = find_cache(uri);
  if (buffer != NULL) // 캐시에 클라이언트가 요청한 객체가 있다면
  {
    Rio_writen(fd, buffer, MAXLINE); // 해당 객체를 클라이언트에 보낸다.
  }
  else // 캐시에 클라이언트가 찾는 객체가 없다면
  {
    final_fd = Open_clientfd(host,port); // 클라이언트의 요청 정보를 서버로 보내기위한 서버 식별자
    sprintf(server_buf, "%s %s %s\r\n", method, path, version); // server_buf에 파싱했던 메소드, 패스, 버전을 넣어주기
    Rio_readinitb(&server_rio, final_fd);
    sprintf(server_buf, "GET %s HTTP/1.0\r\n\r\n", path); // 클라이언트로부터 받은 요청의 헤더를 수정하여 보냄
    Rio_writen(final_fd, server_buf, strlen(server_buf));
    size_t n;
    while((n = Rio_readlineb(&server_rio, server_buf, MAXLINE)) != 0) {
      /*읽은 정보를 캐시에 담는 함수 필요*/
      Rio_writen(fd, server_buf, n);
      cache_size += n;
      if(cache_size < MAX_OBJECT_SIZE){
        strcat(empty_buf, server_buf);
      }
    }
    Close(final_fd);
    insert_cache(strlen(empty_buf), uri, empty_buf); 

  }
  // 서버로 받은 버퍼를 파싱해주는 작업 (해당 작업으로 캐시의 KEY을 정해줄것임)
}
/*캐시 찾는 함수*/
struct cache *find_cache(char *uri)
{
  cache_t *currentitem = cache_head;
  while(currentitem != NULL)
  {
    if(strcmp(currentitem->uri, uri) == 0)
    {
      if(currentitem->prev != NULL) // 해당 객체가 최근 객체가 아니라면, 최근 리스트로 변경
      {
        currentitem->prev->next = currentitem->next;
        if(currentitem->next != NULL)
        {
          currentitem->next->prev = currentitem->prev;
        }
        currentitem->prev = NULL;
        currentitem->next = cache_head;
        cache_head->prev = currentitem;
        cache_head = currentitem;
      }
      printf("같은 객체면서 첫번째 객체 찾음: %d\n", currentitem->content);
      return currentitem->content;
    }
    currentitem = currentitem->next;
  }
  return NULL;
}
/*캐시 없애는 함수 
  가장 오래된 캐시는 맨 뒤에 있다
  즉, 맨 뒤에있는 리스트를 삭제하면 끝*/
void remove_cache(){
  cache_t *LRUitem = cache_head; // 새로 생성한 LRUitem노드는 NULL
    while(LRUitem->next != NULL)
    {
      LRUitem = LRUitem->next;
    }
  LRUitem->prev->next = NULL;
  // int size = strlen(LRUitem->data);
  free(LRUitem);
  cache_size -= LRUitem->size; // 캐시에서 빠져나간 객체의 크기만큼 빼준다.

}
/*캐시 insert하는 함수*/
void insert_cache(int object_size, char *from_server_uri, char *from_server_data)
{
  if (cache_size >= MAX_CACHE_SIZE) {
    remove_cache();
  }
  cache_t *newitem = malloc(sizeof(cache_t));
  strcpy(newitem->uri, from_server_uri);
  strcpy(newitem->content, from_server_data);
  newitem->size = object_size;

  newitem->prev = NULL;
  newitem->next = cache_head;
  if (cache_head != NULL) {
    cache_head->prev = newitem;
  }
  cache_head = newitem;
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