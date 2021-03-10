/***********************************************************************************
*               Класс для работы по протоколу FT1.2 поверх UDP                     *
*                         anselm.ru [2021-03-09]                                   *
***********************************************************************************/
#pragma once

#include "xml.h"
#include "date.h"

typedef unsigned char  byte;
typedef unsigned short word;

template <typename T>
class FT: public T {
public:
  FT(const Node& node);
  virtual ~FT() {}
  bool send11(word reg, byte a1, byte a2);
  bool send19(word reg, byte a1, byte a2, word ii, byte q);
  bool send22(word id, word reg, byte a1, byte a2);
  bool send23(word id, word reg, byte a1, byte a2, word ii, byte q);
  
  bool recv11();
  bool recv19();
  bool recv22(const byte*);
  bool recv23(const byte*);
  
  bool get(Node&);  
  
  static byte CS(const byte* a, int i1, int i2);
  static word index_hour(Date& d, int D=32);
  static word index_day(Date& d);
protected:
  virtual void read(const char*, size_t, const void* = NULL);
  virtual void read(word id, float val);
  int d_wait_sec;
private:
  std::string d_ans;
  bool wait(int) const;
  bool d_ret_bool;
  float d_ret_float;
};
