#pragma once

#include <netinet/in.h>   //sockaddr_in
#include <list>

class UDP
{
public:
  UDP(const char* net, int port, int datasize, int wait, const char* to);
  virtual ~UDP(void);
  bool send(const void*, size_t=0);
  bool recv(void*, size_t &, sockaddr_in &);
private:
  int d_sock;
  int d_wait;
  std::list<sockaddr_in> d_addr;
};
