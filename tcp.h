/*************************************************************************
*                        Класс для работы с TCP                          *
*                                               anselm.ru [2021-03-10]   *
**************************************************************************/
#pragma once

#include <netinet/in.h> //sockaddr_in
#include <sys/time.h>   //timeval
#include <list>
#include "xml.h"

class TCP {
friend void *thread_event(void *);
public:
  TCP();
  TCP(const Node&);
  virtual ~TCP();
  
  bool accept(int maxcon, const char* iface, int port=0);
  bool connect(const char* host, int port=0);
  bool connect(const sockaddr_in&);
  bool connect();
  bool connected() const { return d_is_connect; }
  void disconnect();
  
  bool listen();
  bool send(const char*, size_t = 0) const;
  void send_all(const char*, size_t) const;
  static sockaddr_in mkaddr(const char* host, int port=0);
  static const char* mkip(sockaddr_in);
  void cnntimeo(long msec);
  void sndtimeo(long msec);
  void rcvtimeo(long msec);
  void rcvbuf(int size);
  void reuse() const;
  static const size_t CNNTIMEO = 3000; // время соединения по умолчанию
  static const size_t RCVBUF   = 255;  // размер принимающего буфера по умолчанию и вычитываемая порция
protected:
  bool send(const char*, size_t, int sock) const;
  virtual void read(const char*, size_t, const sockaddr_in* = NULL); // переопределяем в потомках
  void set_attr(int sock) const;
  void join();
  unsigned long long
    d_timeout_conn, // время соединения
    d_timeout_send, // время отправки
    d_timeout_recv; // время приёма
private:
  int create_socket();
  void close_all_clients();
  void close_client(int sock);
  void close_main_socket();
  int recv(int sock);
  int get_error(); // получить и "сбросить" ожидающую обработки ошибку сокета
  
  pthread_t d_thread;
  int d_sock;
  std::list<int> d_socks; // список клиентов
  int d_recv_buff_size;
  bool d_is_connect;
  std::string d_host;
  int d_port;
};
