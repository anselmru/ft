#include "modbustcp.h"
#include "log.h"
#include <md5.h>

#include <syslog.h>
#include <stdio.h>
#include <string.h>  //strlen strdup strrchr
#include <sys/time.h>
#include <stdlib.h>

enum VARENUM {
  VT_EMPTY=0,
  VT_I4=3,
  VT_R4=4,
  VT_R8=5
} VARIANT;

#define PASSWORD_LEN 16
#define CHECKSUM_LEN 32
#define OPC_ITEM_LEN sizeof(OPC_ITEM)

ModbusTCP::ModbusTCP(const Node& cfg)
  : TCPclient(cfg["data"]["socket"]["host"].c_str(), cfg["data"]["socket"]["port"].to_int())
  , UDP_PASSWORD(NULL)
{
  const int   wait  = cfg["data"]["socket"]["rcvtimeo"].to_int(1000);
  const int   rate  = cfg["data"]["rate"].to_int(10);
  UDP_PASSWORD      = cfg["store"]["password"].strdup("*****");
  
  Node::CPair ret   = cfg["data"].equal_range("param");
  setWait(wait);
  
  Node::const_iterator I = ret.first;
  for(; I!=ret.second; ++I) {
    const Node& param = I->second;
    //printf("param id='%d' command='%s'\n", param["id"].to_int(), param["command"].c_str());
    ModbusTCP::Pair cmd = std::make_pair(param["id"].to_string(), param["command"].to_string());
    *this << cmd;  // insert commands
  }
  
  ret = cfg.equal_range("store");
  I = ret.first;
  for(; I!=ret.second; ++I) {
    const Node& data = I->second;
    
    Node::CPair ret2 = data.equal_range("socket");
    Node::const_iterator J=ret2.first;
    for(; J!=ret2.second; ++J) {
      Node sock = J->second;
      Node::const_iterator K = data.begin(); // перебираем все свойства ёмкости data
      for(; K!=data.end(); ++K) {
        const string key = K->first;
        if(key.compare("comment")!=0 && sock[key].to_string().empty() && !K->second.to_string().empty()) {
          sock[key] = K->second; // наследуем свойство родителя
        }
      }
      //string iid = sock["host"].to_string()+":"+sock["port"].to_string();
      d_udp.add_addr(sock["host"].c_str(), sock["port"].to_int());
      //d_stores[ iid ] = sock;
    }
  }
  
  if(d_udp.empty()) {
    warning("empty section store in conf-file");
    //d_error = true;
    return;
  }

}

ModbusTCP::~ModbusTCP()
{
  //if(udp) delete udp;
  if(UDP_PASSWORD) delete [] UDP_PASSWORD;
}

void ModbusTCP::read(const char *buf, const int size) // заполняет d_item
{ 
	if(size<9) {
    if(size==0)
      printf("read: unexpected close of connection at remote end\n");
    else
      printf("response was too short - %d chars\n", size);
      
    return;  
  }
  
  if(buf[7] & 0x80) { // buf[7]>=128
    printf("MODBUS exception response - type %d\n", buf[8]);
    return;
  }
  
  
  timeval tv;
	gettimeofday(&tv, NULL);
  //tv.tv_sec += d_hour*3600;
  
  
  int id  = (buf[0]<<8) + (buf[1]&0xff);
  Query q = d_query[id];
  if(id!=q.id) return; // если не нашли параметр
  
  if(buf[7]==3 && q.size>0) {  // во избежание зацикливания
  	int byte_count = 2*q.size; // 1 регистр = 2 байта
  	for(int i=0; i<buf[8]; i+=byte_count) {
  		unsigned int dw = ((buf[9+i]&0xff)<<8)+((buf[10+i]&0xff))+((buf[11+i]&0xff)<<24)+((buf[12+i]&0xff)<<16);
  		float f = 0.0;
  		memcpy(&f, &dw, byte_count);
  		OPC_ITEM item = {0};
			item.id     = id+i/byte_count;
			item.tv     = tv;
			item.type   = VT_R4;  // float
			item.data.f = f;
			//debug(item);
			d_items.push_back( item );
  	}
  }
  else if(buf[7]==1) {
  	unsigned short w = 0; //(buf[9]&0xff) + ((buf[10]&0xff)<<8) + ...;
  	for(int i=0; i<buf[8]; i++)
  		w += ((buf[9+i]&0xff)<<(8*i));
  		
  	for(int i=0; i<q.count; i++) {
  		OPC_ITEM item = {0};
			item.id     = id+i;
			item.tv     = tv;
			item.type   = VT_I4;  // int
			item.data.i = (w>>(i+q.reg-1)) &0x1; // вообще-то скобки можно было опустить, т.к. последовательность 
				// операций совпадает с приоритетом операций в Си. И ещё: q.reg>0, т.к. иначе будет ошибка 2.
			//debug(item);
			d_items.push_back( item );
  	}
  }
  
}

bool ModbusTCP::connect()
{
  bool con = TCPclient::connect();
  if(!con) return false;
    
  d_items.clear(); // перед отправкой очищаем все данные
  
  std::map<int, Query>::iterator i = d_query.begin();
  for(; i!=d_query.end(); i++) {
  	
  	Query q = i->second;
    //if( !(send((char*)i->second.data, i->second.size) && read()) ) {      // read() вызывает read(...), которая заполняет d_items
    if( !(send(q.data, q.length) && read()) ) {      // read() вызывает read(...), которая заполняет d_items
      break;
    }
  
  }
  
  send_datagram(); // отправка всех данных в одном пакете
  d_items.clear(); // после отправки очищаем все данные
  
  return true;
}

ModbusTCP& ModbusTCP::operator << (const Pair &a) 
{
	Query q  = {0};
  q.length = 12;
  
  sscanf(a.first.c_str(), "%d", &q.id);
  sscanf(a.second.c_str(), "%d %d %d %d %d", &q.device, &q.function, &q.reg, &q.count, &q.size);
  
  int reg_total = q.count * q.size;
  
  q.data[0]  = (q.id >> 8) & 0xff;        // Transaction Identifier [hi]
  q.data[1]  = q.id & 0xff;               // Transaction Identifier [lo]
  q.data[2]  = 0;                         // Protocol Identifier [hi]   
  q.data[3]  = 0;                         // Protocol Identifier [lo]   
  q.data[4]  = 0;                         // Length [hi]
  q.data[5]  = 6;                         // Length [lo]                 
  q.data[6]  = q.device;                  // Unit Identifier
  q.data[7]  = q.function;                // Function    
  q.data[8]  = (q.reg >> 8) & 0xff;       // Register [hi]
  q.data[9]  =  q.reg & 0xff;             // Register [lo]
  q.data[10] = (reg_total >> 8) & 0xff;   // Number of Register [hi]
  q.data[11] =  reg_total & 0xff;         // Number of Register [lo]

  
  d_query[q.id] = q;  
  //debug(q);
  
  return *this;
}


bool ModbusTCP::send_datagram()
{
  bool cool = true;
  
  const size_t count = d_items.size();
  if(count==0) //нет смысла выполнять дальнейшие действия, если отправлять нечего
    return cool;
    
  const size_t size = PASSWORD_LEN + count * OPC_ITEM_LEN + CHECKSUM_LEN; //пароль + параметры + контрольная_сумма
  char *buf   = (char*)malloc(size), *const buf0 = buf; //buf0 - указатель на начало
  
  memset(buf, 0, size); //очистим буфер перед новым использованием
  strcpy(buf, UDP_PASSWORD);//запишем пароль
  buf += PASSWORD_LEN;

  Items::iterator i = d_items.begin();   
  for(; i!=d_items.end(); i++) {
    memcpy(buf, &(*i), OPC_ITEM_LEN);
    buf += OPC_ITEM_LEN;
  }//for

  size_t len = buf-buf0;
  if(len>PASSWORD_LEN) {//если есть хоть один параметр, то есть смысл отправить пакет
    //Запечатываем буфер котрольной суммой
    char hash[33];
    MD5Data(buf0, len, hash);
    strncpy(buf, hash, CHECKSUM_LEN); //memcpy(buf, hash, CHECKSUM_LEN);
    //cool = udp->send(buf0, len+CHECKSUM_LEN);
    cool = d_udp.send(buf0, len+CHECKSUM_LEN);
  }

  if(buf0) free(buf0);
  
  return cool;
}

/*void ModbusTCP::debug(Query q)
{
	printf("Query: id=%d device=%d function=%d reg=%d count=%d size=%d length=%d", q.id, q.device, q.function, q.reg, q.count, q.size, q.length);
	printf(" data=[%d %d %d %d %d %d %d %d %d %d %d %d]\n", q.data[0], q.data[1], q.data[2], q.data[3], q.data[4], q.data[5], q.data[6], q.data[7], q.data[8], q.data[9], q.data[10], q.data[11]);
}

void ModbusTCP::debug(OPC_ITEM a)
{
	if(a.type==VT_R4)
		printf("OPC_ITEM: id=%d value=%f\n", a.id, a.data.f);
	else
		printf("OPC_ITEM: id=%d value=%d\n", a.id, a.data.i);
}*/


