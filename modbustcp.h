#pragma once
#include "tcpclient.h"
#include "udp.h"
#include "xml.h"

#include <list>
#include <map>
#include <string>

#pragma pack(push, 1) // то же что директива MSVC-компилятора /Zp1 - вырвнивание по одному байту
struct OPC_ITEM {
	unsigned short
		id,			//ид. параметра, определяется в conf и в СУБД
		type,
		quality;
	timeval tv;
	union {
		double d;
		float  f;
		int    i;
	} data;
};
#pragma pack(pop)

typedef struct Query {
	int id,         // отличительный номер запроса (присвоим id параметра в системе Q-SCADA)
		device,       // номер устройства
		function,     // номер функции
		reg,          // начальный регистр
		count,        // количество запрашиваемых параметров
		size,         // размер параметра (либо в регистрах - для функций 3 и 4, либо в битах - для функций 1 и 2)
		length;       // размер в байтах всего запроса MODBUS-TCP
	char data[20];  // данные запроса MODBUS-TCP
} Query;


class ModbusTCP : public TCPclient
{
public:
  ModbusTCP(const Node&);
  virtual ~ModbusTCP();
  bool connect();
   
  typedef std::pair<std::string, std::string> Pair;
  ModbusTCP& operator << (const Pair &);      // собираем команды
protected:
  void read(const char *message, const int size);
  bool read() { return TCPclient::read(); }
  
private:
  typedef unsigned short WORD;
  typedef unsigned char BYTE;

  //int d_timeout, d_rate, d_hour;
  
  std::map<int, Query> d_query;
  
  //UDP *udp;
  //const char *password;
  typedef std::list<OPC_ITEM> Items;
  Items d_items;
  
  //void debug(Query);
  //void debug(OPC_ITEM);
  
  
  bool send_datagram();
  const char* UDP_PASSWORD;       // пароль для доступа к службам хранения 
  UDP d_udp;                      // список udp-серверов хранения
};
