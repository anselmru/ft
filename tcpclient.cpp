#include "tcpclient.h"

#include <stdio.h>
#include <errno.h>        //errno
//#include <syslog.h>       //syslog
#include <sys/socket.h>   //socket
#include <string.h>       //strdup
#include <arpa/inet.h>    //inet_addr
#include <unistd.h>       //close sleep
#include <netdb.h>        //gethostbyname

/*void *thread_read(void *a)
{
	TCPclient *tcp = (TCPclient*) a;
	tcp->read();
	pthread_exit(NULL);
}*/

TCPclient::TCPclient(const char *host, const int port) throw(int)
{
  if( (d_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))==-1 ) {
    printf("socket: error (%d) %s", errno, strerror(errno));
    throw(errno);
  }

  //отключить обработку сигнала обрыва
  int flag = 1;
 	if( setsockopt(d_sock, SOL_SOCKET, SO_NOSIGPIPE, (char *) &flag, sizeof(int))<0 ) { // предотвращение события "Broken Pipe" - аварийного завершения всей программы
		printf("nosig: error (%d) %s\n", errno, strerror(errno));
		throw errno;
	}
	
	//получим IP удаленной машины
	hostent *phe = gethostbyname(host);
	if(!phe) {
		printf("gethostbyname: error (%d) %s", errno, strerror(errno));
		throw(errno);
	}
	in_addr inaddr = {0};
	memcpy(&inaddr, phe->h_addr, phe->h_length);
	char *ip = inet_ntoa(inaddr); //адрес удаленной машины
	
  memset(&d_addr, 0, sizeof(d_addr));
  d_addr.sin_family         = AF_INET;
  d_addr.sin_port           = htons(port);
  d_addr.sin_addr.s_addr    = inet_addr(ip); //преобразует строку в реальный IP
}

TCPclient::TCPclient(int sock) : d_sock(sock)
{
}


TCPclient::~TCPclient()
{
  disconnect();
}

bool TCPclient::connect()
{ 
	//явный вызов функции bind не обязателен
	//при необходимости привязки к конкретному порту вызвать Bind
	if(::connect(d_sock, (sockaddr*)&d_addr, sizeof(d_addr))==-1) {
		printf("Error at connect: (%d) %s", errno, strerror(errno));
		return false;
	}
	
	//printf("connect %d\n", d_sock);
	//pthread_create(&d_thread_read, NULL, thread_read, (void*) this);
	
	return true;
}

void TCPclient::disconnect()
{
  if(d_sock) {
    shutdown(d_sock, SHUT_RDWR); //shutdown(d_sock, 2);
    close(d_sock);
    d_sock = 0;
  }
}

/*void TCPclient::join()
{
 	pthread_join(d_thread_read, NULL);
}*/

bool TCPclient::send(const char *message, int size)
{
  if(!size)
    size = strlen(message);
  
  int i = ::send(d_sock, message, size, 0);
  if(i<0) {
    printf("TCPclient send: error (%d) %s", errno, strerror(errno));
    return false;
  }
  return true;
}

bool TCPclient::read()
{
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(d_sock, &readfds);
  
  fd_set exceptfds;
  FD_ZERO(&exceptfds);
  FD_SET(d_sock, &exceptfds);
  
  
  int n = 50;
  char buf[n];
  memset(buf, 0, n);
  int size = 0;
  
  if( 0 < select(d_sock+1, &readfds, NULL, &exceptfds, &d_tv) ) {
    //i = ::recv(d_sock, buf+i, n, 0);
    if(FD_ISSET(d_sock, &exceptfds)) {
    	short err = errno;
    	printf("select: error (%d) %s", err, strerror(err));
    	return false;
    }
    size = ::recv(d_sock, buf, n, 0);//MSG_NOSIGNAL
    if(size<=0) {
    	short err = errno;
    	printf("recv: error (%d) %s\n", err, strerror(err));
    	return false;
    }
  }

  read(buf, size);
  
  return true;
}
