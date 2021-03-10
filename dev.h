/***********************************************************************************
*     Класс-обёртка над классом FT, скрывающая все особеннности протокола FT1.2.   *
*            По сути, здесь реализовано только XML-программирование,               *
*                       где Dev - обобщённый прибор.                               *
*                                                         anselm.ru [2021-03-10]   *
***********************************************************************************/
#pragma once
#include "ft.h"
#include "xml.h"
#include <string>

class Dev: public FT
{
public:
  Dev(Node);
  virtual ~Dev() {}
  
  void pass();
  bool send(int);
  bool store(const string& xml);
  void force(const char* filter=NULL);
  void check_file();                   // проверка невычитанных файлов
protected:
  void read(word id, float val);
private:
  void parse();
  
  Node d_params;                       // список запрашиваемых параметров
  Node d_stores;
  //std::string DB_PATTERN;
  long d_timeout_recv;
};
