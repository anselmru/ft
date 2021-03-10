/*************************************************************************
*                        Класс для работы с UDP                          *
*                                               anselm.ru [2021-03-10]   *
**************************************************************************/
#pragma once

#include <netinet/in.h> //sockaddr_in
#include <pthread.h>    //pthread_t PTHREAD_CANCEL_ENABLE
#include <sys/time.h>   //timeval
#include <list>
#include <string>
#include "xml.h"

class UDP {
friend void* thread_read(void *a); // ожидание события чтения в дочернем потоке
public:
  UDP();
  UDP(const Node&);
  virtual ~UDP();
  bool send(const char*, size_t, const char* host, int port);
  bool send(const char*, size_t, const char* srv);
  bool send(const char*, size_t = 0);
  bool listen();
  bool add_addr(const char *host, const int port);
  bool empty() const { return d_addrs.empty(); }
  static sockaddr_in mkaddr(const char* ip, int port);
  bool connect() { return true; }      // заглушка
  static const size_t CNNTIMEO = 3000; // время соединения по умолчанию
  static const size_t RCVBUF   = 255;  // размер принимающего буфера по умолчанию и вычитываемая порция
protected:
  virtual void read(const char*, size_t, const void* a_sockaddr_in = NULL); // переопределяем в потомках
  //virtual void read(const char*, size_t);
private:  
  bool send(const char*, size_t, sockaddr_in);
  bool recv(timeval *);
  int d_sock;
  pthread_t d_thread_read;
  std::list<sockaddr_in> d_addrs; // список рассылки
  unsigned long long
    d_timeout_conn, // время соединения
    d_timeout_send, // время отправки
    d_timeout_recv; // время приёма
  int d_recv_buff_size;
  std::string d_host;
  int d_port;
};
