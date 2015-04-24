#include "tcpclient.h"


#include <stdio.h>
#include <errno.h>        //errno
//#include <syslog.h>       //syslog
#include <sys/socket.h>   //socket
#include <string.h>       //strdup
#include <arpa/inet.h>    //inet_addr
#include <unistd.h>       //close sleep
#include <netdb.h>        //gethostbyname
//#include <netinet/tcp.h>  //TCP_NODELAY


void *thread_read(void *a)
{
	TCPclient *tcp = (TCPclient*) a;
	tcp->read();
	pthread_exit(NULL);
}

TCPclient::TCPclient(const char *host, const int port) throw(int)
	:d_value(0)
{
  if( (d_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))==-1 ) {
    printf("socket: error (%d) %s", errno, strerror(errno));
    throw(errno);
  }

  //int flag = 1; 
	//setsockopt(d_sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	
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
  if(d_sock) {
    shutdown(d_sock, SHUT_RDWR); //shutdown(d_sock, 2);
    close(d_sock);
  }
}

bool TCPclient::connect()
{ 
	//явный вызов функции bind не обязателен
	//при необходимости привязки к конкретному порту вызвать Bind
	if(::connect(d_sock, (sockaddr*)&d_addr, sizeof(d_addr))==-1) {
		printf("Error at connect: (%d) %s", errno, strerror(errno));
		return false;
	}
	
	pthread_create(&d_thread_read, NULL, thread_read, (void*) this);
	
	return true;
}

void TCPclient::join()
{
 	pthread_join(d_thread_read, NULL);
}

bool TCPclient::send(const char *message, int size)
{
  if(!size)
    size = strlen(message);
  
  int i = ::send(d_sock, message, size, 0);
  if(i<0) {
    printf("TCPclient send: error (%d) %s", errno, strerror(errno));
    //close(d_sock);
    //d_sock = 0;
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
  
  timeval d_tv;
  d_tv.tv_sec  = 10;
  d_tv.tv_usec = 0;
  
  //unsigned char a[100] = {0};
  

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
  
  if(size) {
  	//printf("answer:%s\n", buf);
  	if(size<9) {
      if(size==0)
        printf("read: unexpected close of connection at remote end\n");
      else
        printf("response was too short - %d chars\n", size);
        
      return false;  
    }
    else if(buf[7] & 0x80) { // buf[7]>=128
      printf("MODBUS exception response - type %d\n", buf[8]);
      return false;
    }
    else {
			//unsigned short n = (buf[0]<<8) + (buf[1]&0xff); // I code register in this
			for(unsigned short i=9; i<size; i+=4) {
				unsigned long dw = ((buf[i]&0xff)<<8)+((buf[i+1]&0xff))+((buf[i+2]&0xff)<<24)+((buf[i+3]&0xff)<<16);
				d_value = 0.0;
				memcpy(&d_value, &dw, 4);
				//n+=2;
			}
			
    }//else
	    
  }
  
  return true;
}
