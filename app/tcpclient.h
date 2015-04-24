#pragma once
#include <netinet/in.h>   //sockaddr_in
#include <pthread.h>

class TCPclient
{
friend void *thread_read(void*);
public:
  TCPclient(const char *host, const int port) throw(int); 
  TCPclient(int sock);
  virtual ~TCPclient();
  
  bool connect();
  bool send(const char *message, int size=0);

  bool read();
  void join();
  
  float value() const { return d_value; }
private:
  int d_sock;
  sockaddr_in d_addr;
  pthread_t d_thread_read;
  float d_value;
};
