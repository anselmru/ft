/***********************************************************************************
*               Класс для работы по протоколу FT1.2 поверх UDP|TCP                 *
*                            anselm.ru [2021-03-10]                                *
***********************************************************************************/

#include "ft.h"
#include "log.h"
#include "udp.h"
#include "tcp.h"
#include <stdarg.h>
#include <errno.h>

//https://stackoverflow.com/questions/8752837/undefined-reference-to-template-class-constructor
template class FT<UDP>; // создадим экземпляр UDP
template class FT<TCP>; // создадим экземпляр TCP

template <typename T>
byte
FT<T>::CS(const byte* a, int i1, int i2) {
  byte b = 0;
  for(int i=i1; i<i2; i++) {
    b = (b + a[i]) % 256;// & 0xff;
  }
  return b;//&0xff;
}

template <typename T>
FT<T>::FT(const Node& node)
:T(node)
,d_wait_sec(10) {
  T::connect();
  T::listen();
}

template <typename T>
void
FT<T>::read(const char* msg, size_t size, const void*) { // обработка пришедшего пакета
  print_bin2("FT::read ", msg, size);
  
  d_ans.append(msg, size);
  
  if(d_ans.size()<4) { // слишком короткий ответ
    warning("error: size of answer too small %d", d_ans.size());
    return;
  }

  const byte* a = (const byte*) msg;
  
  size_t size_u = a[1]&0xff; // размер полезных данных - с 4 до контрольной суммы
  if(size_u!=(a[2]&0xff) || size_u!=size-6) {    // размеры должны совпадать
    error("incorrect size");
    return;
  }
  
  if(a[size-2]!=CS(a, 4, size-2)) {    // контрольная сумма и предпоследний байт должны совпадать
    error("incorrect checksum");
    return;
  }
  
  const byte P = a[4]&0xff;
  switch (P) {
    case 1: d_ret_bool = recv11();  break;
    case 2: d_ret_bool = recv19();  break;
    case 3: d_ret_bool = recv22(a); break;
    case 4: d_ret_bool = recv23(a); break;
    default: error("unexpected command");
  }
}

/*******************************************************************************
          Команда 11h «Чтение параметра из внешнего модуля»
ЭВМ в линию FT1.2: 10 4Р Адрес1 11 Адрес2 NN ТТ КС 16
Адаптер в CAN: чтение параметра TTNN из модуля с адресом «Адрес2»
Адаптер в FT1.2: 68 LL+2 LL+2 68 0Р Адрес ХХо..ХХn КС 16
                                             \/
                                           LL байт
Здесь     Адрес1 -        адрес адаптера в линии FT1.2;
          Адрес2 -        адрес требуемого модуля в магистрали CAN;
          Р      -        номер пакета (см. 1.10);
          NN     -        номер параметра (т.е. младший байт полного номера);
          ТТ     -        тип параметра (т.е. старший байт полного номера);
          LL     -        длина параметра в байтах (от 1 до 4);
          КС     -        контрольная сумма
          ХХо .. ХХn      значение параметра
*******************************************************************************/
template <typename T>
bool
FT<T>::send11(word reg, byte a1, byte a2) {
  const byte P = 0x01; // номер пакета (от 1 до 7) буду использовать для идентификации команды
  byte a[] = {0x10, 0x40+P, a1, 0x11, a2, reg & 0xff, reg>>8 & 0xff, 0x00, 0x16};
  a[7] = CS(a, 1, 7);
  
  print_bin2("FT::send11 ", (const char*)a, sizeof(a));
  
  return T::send((const char*)a, sizeof(a));
}

/*******************************************************************************
          Команда 19h «Чтение индексного параметра внешнего модуля»
Команда с кодом 19h применяется для чтения индексированных параметров,
в основном числовых архивов, из модулей системы «ТЭКОН-20»
через адаптер FT1.2/CAN. Процедура ее выполнения имеет следующий вид:

ЭВМ в линию FT1.2: 
        68 09 09 68 4Р Адрес1 19 Адрес2 NN TT IIмл IIст QQ KC 16
Адаптер в CAN: чтение параметра TTNN(II) из модуля «Адрес2»
        (запрос \ ответ повторяется QQ раз с индексами II от IIнач до  IIнач+QQ-1)
Модуль: 10 0Р Адрес1 ХХ0 ХХ1 ХХ2 ХХ3 КС 16           при QQ=1 вариант 1
        68 06 06 68 0Р Адрес1 ХХ0 ХХ1 ХХ2 ХХ3 КС 16  при QQ=1 вариант 2
        68 LL LL 68  0Р Адрес1 ХХо..ХХn  КС  16      при QQ>1
Здесь   Адрес1 -        адрес адаптера в линии FT1.2;
        Адрес2 -        адрес требуемого модуля в магистрали CAN;
        Р      -        номер пакета;
        NN     -        номер параметра (т.е. младший байт полного номера);
        ТТ     -        тип параметра (т.е. старший байт полного номера);
        IIмл   -        младший байт начального значения индекса;
        IIст   -        старший байт начального значения индекса;
        QQ     -        количество требуемых элементов (от 1 до 60);
        КС     -        контрольная сумма;
        ХХо .. ХХn      значения параметра для всех требуемых индексов;
         
        LL=QQ*Lпар + 2  длина ответного сообщения при QQ>1;
        Lпар  -         длина параметра, байт (1, 2 или 4).
*******************************************************************************/
template <typename T>
bool
FT<T>::send19(word reg, byte a1, byte a2, word ii, byte q) {
//FTsend23(word id, 
         //word reg, byte a1, byte a2, word ii, byte q) { // получение индексированного значения параметра
  const byte P = 0x02; // номер пакета (от 1 до 7) буду использовать для идентификации команды
  //                                      4Р  А1    19  А2         NN            TT       IIмл          IIст  QQ    KC    16
  byte a[] = {0x68, 0x09, 0x09, 0x68, 0x40+P, a1, 0x19, a2, reg & 0xff, reg>>8 & 0xff, ii & 0xff, ii>>8 & 0xff,  q, 0x00, 0x16};
  a[13]    = CS(a, 4, 13);
  
  print_bin2("FT::send19 ", (const char*)a, sizeof(a));
  
  return T::send((const char*)a, sizeof(a));
}

template <typename T>
bool
FT<T>::send22(word id, word reg, byte a1, byte a2) { // получение мгновенное значение параметра
  const byte P = 0x03; // номер пакета (от 1 до 7) буду использовать для идентификации команды
  //                                      4Р    А1    19  А2          NN            TT        IDмл          IDст    KC    16
  byte a[] =   {0x68, 0x08, 0x08, 0x68, 0x40+P, a1, 0x22, a2, reg & 0xff, reg>>8 & 0xff, id & 0xff, id>>8 & 0xff, 0x00, 0x16};
  //               0     1     2     3       4   5     6   7          8             9         10             11   12    13
  size_t size = sizeof(a);      // 14 байт
  a[1] = a[2] = size - 6;       // размер без первых четырёх и последних двух
  a[size-2] = CS(a, 4, size-2); // контрольная сумма на предпоследнем месте
  
  print_bin2("FT::send22 ", (const char*)a, sizeof(a));
  
  return T::send((const char*)a, sizeof(a));
}

template <typename T>
bool
FT<T>::recv22(const byte* a) {   // мгновенное значение параметра
  size_t size = a[1]&0xff;    // размер полезных данных - с 4 до контрольной суммы
  
  word id=0;
  memcpy(&id, &a[size+2], 2); // идентификатор параметра на четвёртом с конца месте (там вообще-то может стоять любой идентификатор)
  
  //warning("iid=%d value=[%0x %0x %0x %0x]", id, a[6]&0xf, a[7]&0xf, a[8]&0xf, a[9]&0xf );
  
  float f=0.0;
  memcpy(&f, &a[6], 4);
  
  read(id, f);
  
  return true;
}

template <typename T>
bool
FT<T>::send23(word id, word reg, byte a1, byte a2, word ii, byte q) { // получение индексированного значения параметра
  const byte P = 0x04; // номер пакета (от 1 до 7) буду использовать для идентификации команды
  //                                      4Р    А1    19  А2         NN            TT         IIмл          IIст QQ       IDмл          IDст    KC    16
  byte a[] =   {0x68, 0x0B, 0x0B, 0x68, 0x40+P, a1, 0x23, a2, reg & 0xff, reg>>8 & 0xff, ii & 0xff, ii>>8 & 0xff, q, id & 0xff, id>>8 & 0xff, 0x00, 0x16};
  //               0     1     2     3       4   5     6   7          8             9         10            11  12        13            14    15    16
  size_t size = sizeof(a);      // 18 байт
  a[1] = a[2] = size - 6;       // размер без первых четырёх и последних двух байт
  a[size-2] = CS(a, 4, size-2); // контрольная сумма на предпоследнем месте
  
  print_bin2("FT::send23 ", (const char*)a, sizeof(a));
                                                                                          
  return T::send((const char*)a, sizeof(a));
}

template <typename T>
bool
FT<T>::recv23(const byte* a) { // мгновенное значение параметра
  size_t size = a[1]&0xff;    // размер полезных данных - с 4 до контрольной суммы
  
  word id=0;
  memcpy(&id, &a[size+2], 2); // идентификатор параметра на четвёртом с конца месте (там вообще-то может стоять любой идентификатор)
  
  for(size_t i=0; i<size-4; i+=4) { //size-4 - это размер без номера прибора(4-ый байт), номера пакета (5-ый байт), без идентификатора (два байта в конце)
    float f=0.0;
    memcpy(&f, &a[6+i], 4);
    read(id, f);
  }
  
  return true;
}

template <typename T>
word
FT<T>::index_hour(Date& d, int D) { //D - количество дней - глубина часового архива  
  //int G = dt.year()-2000; // количество лет с 2000 года 
  //int N = 365*G + int(G/4) + dt.yday()-1; // количество дней с 2000 года
  //int D = 64;// количество дней - глубина часового архива 1536(0-1535) часов                
  Date d2000("2000-01-01");  
  d = d.trunc(Date::HOUR);// это всегда прошедший час 
  long N = (d-d2000)/3600/24;
  word ii = (N%D)*24+d.hour()-1; // количество часов с 2000 года
  warning2("hour: %s ~ %d", d.c_str(), ii);
  return ii;
}

template <typename T>
word
FT<T>::index_day(Date& d) {
  Date today = Date::now().trunc(Date::MDAY);
  d = d.trunc(Date::MDAY);
  if(today==d) { // если дата за сегодня,
    d-=3600*24;  // ... то подразумеваем за вчера
    warning2("esterday=%s\n", d.c_str());
  }
  
  word ii = d.yday()-1; //т.к. сутки в ТЭКОН принято считать с нуля
  warning2("day: %s ~ %d", d.c_str(), ii);
  return ii;
}

template <typename T>
void
FT<T>::read(word id, float f) { // перегружаемая
  warning2("FT::read %02x=%f\n", id, f);
}

template <typename T>
bool
FT<T>::recv11() {  
  const int L = d_ans[1]+6; // ожидаемая длина ответа
    
  if(d_ans.size()<L) { // неправильная длина ответа
    warning("error size of answer (%d) less than need %d", d_ans.size(), L);
    return false;
  }
  
  if(d_ans.size()>L) {
    warning("error size of answer (%d) more than need %d", d_ans.size(), L);
    d_ans.clear();
    return false;
  }
  
  if(d_ans[0]!=0x68 || d_ans[0]!=d_ans[3] || d_ans[1]!=d_ans[2]) { // неправильная сигнатура ответа
    warning("error wrong answer signature %02X %02X %02X %02X", d_ans[0]&0xff, d_ans[1]&0xff, d_ans[2]&0xff, d_ans[3]&0xff);
    return false;
  }
  
  if(d_ans[L-1]!=0x16) { // неправильное окончание ответа
    warning("error wrong last symbol %02X", d_ans[L-1]&0xff);
    return false;
  }
  
  byte cs;
  if( (d_ans[L-2]&0xff) != (cs=CS((byte*)d_ans.data(), 4, L-2)) ) {
    warning("error check summa %02Xh, need %02Xh", d_ans[L-2]&0xff, cs);
    return false;
  }
  
  switch(d_ans[1]-2) { // размер пришедшего числового значения в байтах
    case 4: {
      float f = 0.0;
      memcpy(&f, d_ans.data()+6, 4);
      d_ret_float = f;
      //read(i, d_ret_float);
    }
    break;
    case 2: {
      word u = 0;
      memcpy(&u, d_ans.data()+6, 2);
      d_ret_float = u;
      //read(i, d_ret_float);
    }
    break;
    case 1: {
      d_ret_float = d_ans.data()[6]; //d_ans[6]&0xff
      //read(i, d_ret_float);
    }
    break;
    default: {
      warning("error size of data = %d", d_ans[1]-2);
      return false;
    }
  }//switch
  
  d_ans.clear();
  return true;
}

template <typename T>
bool
FT<T>::recv19() {
  const int L = d_ans[1]+6; // ожидаемая длина ответа
    
  if(d_ans.size()<L ) { // неправильная длина ответа
    warning("error size of answer %d, need %d", d_ans.size(), L);
    return false;
  }

  if(d_ans.size()>L) {
    warning("error size of answer (%d) more than need %d", d_ans.size(), L);
    d_ans.clear();
    return false;
  }

  std::string b = d_ans;
  
  
  if(b[0]!=0x68 && b[0]!=0x10) { // возможны два варианта ответа
    warning("error first byte %02Xh, need 68h or 10h", b[0]&0xff);
    return false;
  }
  
  if(b[0]==0x68) { // вариант 2
    
    if(b[0]!=b[3] || b[1]!=b[2] || b[L-1]!=0x16) { // неправильная сигнатура ответа
      warning("error wrong signature %02X %02X %02X %02X ... %02X", b[0]&0xff, b[1]&0xff, b[2]&0xff, b[3]&0xff, b[L-1]&0xff);
      return false;
    }
    
    byte cs;
    if( (b[L-2]&0xff) != (cs=CS((byte*)b.data(), 4, L-2)) ) {
      warning("error check summa %02Xh, need %02Xh", b[L-2]&0xff, cs);
      return false;
    }

    const byte q = 1; // сколько параметров; q у меня будет всегда равным 1 TODO а как вычислить в общем случае?
    switch((b[1]-2)/q) {
      case 4: {
        for(int i=0; i<q; i++) {
          float f = 0.0;
          memcpy(&f, b.data()+6+i*4, 4);
          d_ret_float = f;
          //read(i, d_ret_float);
        }
      }
      break;
      case 2: {
        for(int i=0; i<q; i++) {
          word u = 0;
          memcpy(&u, b.data()+6+i*2, 2);
          d_ret_float = u;
          //read(i, d_ret_float);
        }
      }
      break;
      case 1: {
        for(int i=0; i<q; i++) {
          d_ret_float = b.data()[6+i];
          //read(i, d_ret_float);
        }
      }
      break;
      default: {
        warning("error size of data = %d", (b[1]-2)/q);
        return false;
      }
    }//switch
  }  

  d_ans.clear();
  return true;  
}

template <typename T>
bool
FT<T>::wait(int sec) const { // ожидаем, пока функции recv не вернут истину или не выйдет таймаут
  time_t t = time(NULL)+sec;
  while(time(NULL)<t) {
    if(d_ret_bool) return true;
    usleep(200000);
  }
    
  return false;
}

template <typename T>
bool
FT<T>::get(Node& p) {
  p["success"] = d_ret_bool  = false;  // сбрасывем первоначальное состояние возвращённое функциями recv
  p["value"  ] = d_ret_float = 0.0;    // сбрасывем значение
  
  const int cmd = p["fun"].to_int();
  const word id = p["reg"].to_int();  
  const byte a1 = p["a1" ].to_int();
  const byte a2 = p["a2" ].to_int();  
  //warning2("cmd=%d tag=0x%02X", cmd, id);
  
  bool ret = false; // здесь будем хранить итог выполнения функций send
  switch(cmd) {
    case 11: {
      ret = send11(id, a1, a2);
      break;
    }
    case 19: {
      word ind = p["ind"].to_int();
      ret = send19(id, ind, 1, a1, a2);
      break;
    }
    //case 22:;
    //case 23:;
    default: warning("error: unknown command %d", cmd);
  }
  
  if(!ret) return false; // не продолжаем, если отправка не удалась
  ret = wait(d_wait_sec); // ожидаем ответа
  
  d_ans.clear();
  
  if(ret) {
    p["success"] = true;
    p["value"]   = d_ret_float;
  }
  
  //usleep(d_timeout_recv * 1000); // пауза между запросами
  sleep(1);
  return ret;
}
