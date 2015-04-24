#include "tcpclient.h"
#include <iostream>

using namespace std;

int main(int argn, char ** argv) {

	//char b[50] = {0};
	//cout <<	fread(b, 20, 1, stdin) << b;
	
	if(argn<4) {
		cerr << "Usage: " << argv[0] << " host port register" << endl;
		cerr << "       host - hostname or ip-address of k104" << endl;
		cerr << "       port - listen port of k104" << endl;
		cerr << "       reg  - number of register" << endl;
		return 1;
	}
	
	char *host = argv[1];
	int port   = atoi(argv[2]);
	int base   = atoi(argv[3]);
	int len    = 12;                  
  int offset = 2;                 

  unsigned char buf[20] = {0};
  //buf[0]     = (xxx >> 8) & 0xff;         // Transaction Identifier [hi]
  //buf[1]     = xxxx & 0xff;       // Transaction Identifier [lo]
  buf[2]     = 0;                 // Protocol Identifier [hi]   
  buf[3]     = 0;                 // Protocol Identifier [lo]   
  buf[4]     = 0;                 // Length [hi]
  buf[5]     = 6;                 // Length [lo]                 
  buf[6]     = 1;                 // Unit Identifier
  buf[7]     = 3;                 // Function    
  buf[8]  = (base >> 8) & 0xff;
  buf[9]  =  base & 0xff;
  buf[10] = (offset >> 8) & 0xff;
  buf[11] =  offset & 0xff;
	                                 
  TCPclient tcp(host, port);
  if(!tcp.connect()) {
    cerr << "Error: not connecting";
    return 1;
  }
    
  tcp.send((char*)buf,len);
  tcp.join();    
  cout << base << " " << tcp.value();  
  return 0;
}
