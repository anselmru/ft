#include "udp.h"
#include <sys/socket.h>   //socket
#include <arpa/inet.h>    //inet_addr
#include <errno.h>        //errno
#include <syslog.h>       //syslog
#include <netdb.h>        //gethostbyname

/** Максимальный размер одной датаграммы IP равен 65535 байтам.
Из них не менее 20 байт занимает заголовок IP.
Заголовок UDP имеет размер 8 байт. Таким образом, максимальный
размер одной дейтаграммы UDP составляет 65507 байт.*/

UDP::UDP(const char* net, int port, int datasize, int wait, const char* to)
  :d_sock(0)
  ,d_wait(wait)
{
  d_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(d_sock == -1) {
    syslog(LOG_ERR, "error: socket (%d) %s", errno, strerror(errno));
    return;
  } 

  //Разделяем порт
  const int on = 1;
  if(setsockopt(d_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) == -1) {
    syslog(LOG_ERR, "error: setsockopt SO_REUSEADDR (%d) %s", errno, strerror(errno));
    return;
  }
  //Устанавливаем размер буфера (вроде необязательно)
  if(setsockopt(d_sock, SOL_SOCKET, SO_RCVBUF, (char*) &datasize, sizeof(datasize)) == -1) {    
    syslog(LOG_ERR, "error: setsockopt SO_RCVBUF (%d) %s", errno, strerror(errno));
    return;
  }
  
  sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(net);
  addr.sin_port = htons(port);
  
  //Сохраняем адреса для рассылки в списке d_addr
  char *s = strdup(to);
  char *token = strtok(s, " ");
  while(token) {
    char host[31]={0}, *ip;
    sscanf(token, "%s", host);
    token = strtok(NULL, " ");
    
    //find ip by hostname
    hostent *phe = gethostbyname(host);
    if(phe) {
      in_addr inaddr = {0};
      memcpy(&inaddr, phe->h_addr, phe->h_length);
      ip = inet_ntoa(inaddr);
    }
    
    addr.sin_addr.s_addr = inet_addr(ip);
    //printf("ip=%s\n", ip);
    d_addr.push_back(addr);
  }
  delete s;
}


UDP::~UDP()
{
  if(d_sock) {
    shutdown(d_sock, 2);
    close(d_sock);
    d_sock = 0;
  }
}

bool UDP::send(const void *msg, size_t size)
{
  bool cool;
  fd_set writefds;
  FD_ZERO(&writefds);
  FD_SET(d_sock, &writefds);
  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = d_wait*1000;

  if(size<=0) {
    size = strlen((char*)msg);
  }

  std::list<sockaddr_in>::const_iterator i = d_addr.begin();
  for(; i!= d_addr.end(); i++) {
    int iRes = select(0, NULL, &writefds, NULL, &tv);//ожидаем, когда буфер освободится для записи
    if(iRes == -1) {
      cool = false;
      syslog(LOG_ERR, "error: select before sendto (%d) %s", errno, strerror(errno));
    }
    else {
      int res = sendto(d_sock, (char*)msg, size, 0, (sockaddr*)& *i, sizeof(sockaddr_in));
      cool = res != -1;
      if(!cool) {
        syslog(LOG_ERR, "error: sendto (%d) %s", errno, strerror(errno));
      }
    }
  }//for

  return cool;
}

bool UDP::recv(void* msg, size_t &size, sockaddr_in &from)
{
  bool cool;
  //Определяем размер буфера
  socklen_t len = sizeof(int); 
  if( getsockopt(d_sock, SOL_SOCKET, SO_RCVBUF, (char*) &size, &len) == -1 ) {
    syslog(LOG_ERR, "error: getsockopt SO_RCVBUF (%d) %s", errno, strerror(errno));
    //return false;
  }
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(d_sock, &readfds);
  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = d_wait*1000;
  int iRes = select(0, &readfds, NULL, NULL, &tv);//ожидаем, когда в буфере появятся данные для чтения(не актуально, т.к. я точно знаю, когда они появятся)
  if(iRes == -1) {
    cool = false;
    syslog(LOG_ERR, "error: select before recvfrom (%d) %s", errno, strerror(errno));
  }
  else {
    memset(msg, 0, size);
    socklen_t fromlen = sizeof(sockaddr_in);
    memset(&from, 0, fromlen);
    int res = recvfrom(d_sock, (char*)msg, size, 0, (sockaddr*) &from, &fromlen);//чтение буфера с очисткой
    //int res = recvfrom(d_sock, (char*)msg, size, MSG_PEEK, (sockaddr*) &from, &fromlen);//просмотр буфера без очистки
    cool = res != -1;
    if(!cool) {
      syslog(LOG_ERR, "error: recvfrom (%d) %s", errno, strerror(errno));
    }
  }

  return cool;
}
