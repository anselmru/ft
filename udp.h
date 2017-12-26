/***********************************************************************************
                         anselm.ru [2015-10-26] GNU GPL
***********************************************************************************/
#pragma once

#include <netinet/in.h> //sockaddr_in
#include <pthread.h>    //pthread_t PTHREAD_CANCEL_ENABLE
#include <sys/time.h>   //timeval
#include <list>

class UDP {
friend void* thread_read(void *a); // ожидание события чтения в дочернем потоке
public:
  UDP();
  virtual ~UDP();
  bool send(const void*, size_t, const char* host, int port);
  bool send(const void*, size_t, const char* srv);
  bool send(const void*, size_t);
  bool listen();
  bool add_addr(const char *host, const int port);
  bool empty() const { return d_addrs.empty(); }
  static sockaddr_in mkaddr(const char* ip, int port);
protected:
  virtual void read(const void*, size_t, sockaddr_in) {}
private:  
  bool send(const void*, size_t, sockaddr_in);
  bool recv(timeval *);
  int d_sock;
  pthread_t d_thread_read;
  std::list<sockaddr_in> d_addrs; // список рассылки
};
