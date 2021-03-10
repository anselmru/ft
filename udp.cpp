/*************************************************************************
*                        Класс для работы с UDP                          *
*                                               anselm.ru [2021-03-10]   *
**************************************************************************/
#include "udp.h"
#include <sys/socket.h>   //socket
#include <arpa/inet.h>    //inet_addr
#include <netdb.h>        //gethostbyname
#include <unistd.h>       //close
#include <string.h>       //strlen
#include <stdlib.h>       //malloc
#include <stdio.h>       //printf
#include "log.h"
#include "event.h"

void*
thread_read(void *a) {// ожидание события в дочернем потоке
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);  // отложенный выход - установлен по умолчанию
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL); // отложенный выход - в ближайшей точке выхода
  
  void ** arr = (void**)  a;
  UDP   * udp = (UDP*)    arr[0];
  Event * evt = (Event*)  arr[1];

  evt->wake(); // сообщаем о готовности потока
  debug3("thread_read - start");
  
  while(1) {
    pthread_testcancel();
    if(!udp->recv(NULL)) break; // когда сокет уничтожается на него сыплется тысяча событий, поэтому можно погасить поток
    
    // но если предварительно поток гасится, то сокет естественно не будет ничего слышать
    //udp->wake(); // сообщаем о том, что все данные получены и обработаны
  }
  
  warning("pthread_exit\n");
  //udp->wake(); // сообщаем о завершении потока
  pthread_exit(NULL);
}

UDP::UDP()
: d_sock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
, d_thread_read(0)
, d_timeout_conn(CNNTIMEO)
, d_timeout_send(0)
, d_timeout_recv(0)
, d_recv_buff_size(RCVBUF) 
{
  if(d_sock == -1) error("socket");
}


UDP::UDP(const Node& a)
: d_sock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
, d_thread_read(0)
, d_timeout_conn(CNNTIMEO)
, d_timeout_send(0)
, d_timeout_recv(0)
, d_recv_buff_size(RCVBUF)
{
  if(d_sock == -1) error("socket");
  
  if(!a["host"].blank())     d_host = a["host"].c_str();
  if(!a["port"].blank())     d_port = a["port"].to_int();
  if(!a["sndtimeo"].blank()) d_timeout_send   = a["sndtimeo"].to_int();
  if(!a["rcvtimeo"].blank()) d_timeout_recv   = a["rcvtimeo"].to_int();
  if(!a["cnntimeo"].blank()) d_timeout_conn   = a["cnntimeo"].to_int();
  if(!a["rcvbuf"].blank())   d_recv_buff_size = a["rcvbuf"].to_int();
  debug3("%s:%d", d_host.c_str(), d_port);
  debug3("d_timeout_send   = %ld", d_timeout_send);
  debug3("d_timeout_recv   = %ld", d_timeout_recv);
  debug3("d_timeout_conn   = %ld", d_timeout_conn);
  debug3("d_recv_buff_size = %ld", d_recv_buff_size);  
  
  add_addr(d_host.c_str(), d_port);
}


UDP::~UDP() {
  if(d_thread_read) {
    int ret = pthread_cancel(d_thread_read);
    void *rval_ptr;
    ret = pthread_join(d_thread_read, &rval_ptr);
    d_thread_read = 0;
  }
  
  if(d_sock) {
    shutdown(d_sock, 2);
    close(d_sock);
    d_sock = 0;
  }
}

bool
UDP::listen() {
  if(d_thread_read) {
    error("listening thread already ran");
    return false; // слушающий поток уже запущен
  }
  
  Event evt; 
  void * arr[2] = {(void*)this, (void*)&evt};
  
  if(pthread_create(&d_thread_read, NULL, thread_read, arr)!=0) {
    error("pthread_create(thread_read)");
    d_thread_read = 0;
  }

  evt.wait(); // надо дать время запуститься потоку (и заполнить ид потока d_thread_read)
  
  debug3("UDP::listen start thread %d", d_thread_read);
  
  return d_thread_read!=0;
}

bool
UDP::send(const char *msg, size_t size, sockaddr_in to) {
  print_bin3("UDP::send ", msg, size);
  
  if(size<=0) {
    size = strlen((char*)msg);
  }
  
  int res = sendto(d_sock, (char*)msg, size, 0, (sockaddr*)& to, sizeof(sockaddr_in));
  bool cool = res != -1;
  if(!cool) {
    error("sendto");
  }

  return cool;
}

bool
UDP::recv(timeval *tv) {
  bool res = true;
  
  debug3("UDP::recv d_sock=%d", d_sock);
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(d_sock, &readfds);
  int i = select(d_sock+1, &readfds, NULL, NULL, tv);//ожидаем, когда в буфере появятся данные для чтения(не актуально, т.к. я точно знаю, когда они появятся)
  if(i == -1) {
    error("select before recvfrom");// когда уничтожается сокет
    res = false;
  }
  else {
    debug3("UDP::recv succes");
    const size_t MIN_SIZE = 10;
    size_t buf_size  = MIN_SIZE; // начальный размер буфера
    size_t data_size = 0;        // надо найти полный размер датаграммы
    
    socklen_t fromlen = sizeof(sockaddr_in);
    sockaddr_in from  = {0};
    
    char *buf = (char*)malloc(buf_size);
    bzero(buf, buf_size);
    
    int i;
    while((i=recvfrom(d_sock, (char*)buf, buf_size, MSG_PEEK, (sockaddr*) &from, &fromlen))>0) { //MSG_PEEK - просмотр буфера без очистки
      if(i<buf_size) {
        data_size = i;
        break;
      }
      buf = (char*)realloc(buf, buf_size+=MIN_SIZE); //увеличиваем размер буфера
      bzero(buf, buf_size);
    }
    
    if(data_size>0) {
      i=recvfrom(d_sock, (char*)buf, buf_size, 0, (sockaddr*) &from, &fromlen);  //чтение буфера с очисткой
      read(buf, data_size, &from);
      //read(buf, data_size);
    }
    else {
      error("recvfrom");
      res = false;
    }
        
    free(buf);
  }

  return res;
}

void
UDP::read(const char* msg, size_t, const void*) {
  warning3("UDP::read  <-- override this");
}

sockaddr_in
UDP::mkaddr(const char* host, int port) {
  sockaddr_in to = {0};         
  
  hostent *phe = gethostbyname(host);
  if(!phe) {
    //error("gethostbyname host=%s", host);
    return to;
  }
  in_addr inaddr = {0};
  memcpy(&inaddr, phe->h_addr, phe->h_length);
  char *ip = inet_ntoa(inaddr); //адрес удаленной машины
  
  to.sin_family = AF_INET;
  to.sin_addr.s_addr = inet_addr(ip);
  to.sin_port = htons(port);
  return to;
}

bool
UDP::send(const char* msg, size_t len, const char* host, int port) {
  return send(msg, len, mkaddr(host, port));
}

bool
UDP::send(const char* msg, size_t len, const char* srv) {
  char ip[20] = {0};
  int port = 0;
  sscanf(srv, "%[^:]:%d", ip, &port);
  return port>0 && send(msg, len, mkaddr(ip, port));
}

bool
UDP::add_addr(const char *host, const int port) { // для массовой рассылки
  sockaddr_in addr = mkaddr(host, port);
  if(addr.sin_family==0) {
    error("bad address %s:%d", host,port);
    return false;
  }
  d_addrs.push_back(addr);
  return true;
}

bool
UDP::send(const char *msg, size_t size) { // для массовой рассылки
  if(size==0) size = strlen(msg);
  bool ret = true;
  std::list<sockaddr_in>::const_iterator I = d_addrs.begin();
  for(; I!=d_addrs.end(); I++) {
    ret &= send(msg, size, *I); // все отсылки д.б. удачными
  }
  return ret;
}
