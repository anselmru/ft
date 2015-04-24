#pragma once
#include <netinet/in.h>   //sockaddr_in
#include <sys/time.h>     //timeval
//#include <pthread.h>

class TCPclient
{
//friend void *thread_read(void*);
public:
  TCPclient(const char *host, const int port) throw(int); 
  TCPclient(int sock);
  virtual ~TCPclient();
  
  virtual bool connect();
  void disconnect();
  bool send(const char *message, int size=0);

  //void join();
  void setWait(int ms)   { d_tv.tv_sec = 0; d_tv.tv_usec = ms*1000; }

protected:	
	virtual void read(const char *message, int size) = 0;
  virtual bool read();
private:
  int d_sock;
  sockaddr_in d_addr;
  timeval d_tv;
  //pthread_t d_thread_read;
};
