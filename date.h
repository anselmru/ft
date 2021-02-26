/***********************************************************************************
                         anselm.ru [2016-10-26] GNU GPL
***********************************************************************************/
#include <string>
#include <vector>
#include <deque>
#include <string.h>
#include <sys/time.h>

#ifndef DATE_H
#define DATE_H

#ifdef WIN32
#include <time.h>
#include <windows.h>
#else
typedef unsigned long DWORD;
typedef struct  {
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME;
#endif

class Date : public timeval { // количество секунд и микросекунд с 1970 года
  static const char FORMAT_OUT[];
  static const size_t DATE_BUFF_MIN_SIZE; //2014-01-01 00:00:00.123456+0000
public:
  static const char* USEC;
  static const char* MSEC;
  static const char* SEC;  
  static const char* MINUT;
  static const char* HOUR;
  static const char* MDAY;
  static const char* MON;
  static const char* YEAR;
  static const char* YDAY;
public:
  Date();
  Date(timeval);
  Date(const char* a)        { *this=a; }
  Date(const std::string& a) { *this=a.c_str(); }
  Date(FILETIME);      //количество 100-наносекундных интервалов с 1601
  Date(double);        //количество суток с 1900 года
  Date(time_t);        //количество секунд с 1970 года
  
  static Date now();
  static Date date(int y, int m, int d, int H, int M, int S, int N = 0);
  Date date(int, int) const;
  Date date(const char*, int) const;
  
  long        to_long() const { return tv_sec; }
  std::string to_string(const char* F = FORMAT_OUT) const;
  const char* c_str    (const char* F = FORMAT_OUT);
  double      to_double() const;
  
  Date& from_double(double a);
  Date& operator=(const char* a); //распознавание строки
  
  operator std::string() const { return to_string(); }
  operator long()        const { return tv_sec;  }
  operator const char*()       { return c_str(); }
  bool  operator<(const Date&);
  bool  operator<=(const Date&);
  bool  operator>(const Date&);
  bool  operator>=(const Date&);
  bool  operator==(const Date&);
  bool  operator!=(const Date&);  
  Date& operator-=(const Date&);
  Date& operator+=(const Date&);
  
  int   operator[](int) const;
  int   operator[](const char*) const;
  
  Date  trunc(const char*) const;
  int   get  (int) const;
  int   get  (const char*) const;
  Date& set  (int, int);
  Date& set  (const char*, int);
  Date& set  (int y, int m, int d, int H, int M, int S, int N = 0);
  static bool check(int y, int m, int d, int H, int M, int S);
  static int  timezone();
  
  int  year()   const { return get(YEAR ); } //Число лет, прошедших с 0
  int  yday()   const { return get(YDAY ); } //Количество дней, прошедших с 1 января, от 1 до 366
  int  mday()   const { return get(MDAY ); } //День месяца, от 1 до 31
  int  mon()    const { return get(MON  ); } //Число месяцев, прошедших с января, от 1 до 12
  int  hour()   const { return get(HOUR ); } //Количество прошедших часов после полуночи, от 0 до 23
  int  minut()  const { return get(MINUT); } //Число минут, прошедших после часа, от 0 до 59
  int  sec()    const { return get(SEC  ); } //Число секунд, прошедших после минуты, обычно в диапазоне от 0 до 59; но для того, чтобы установить високосную секунду, используются числа до 61
  int  msec()   const { return get(MSEC ); } //Число миллисекунд
  int  usec()   const { return get(USEC ); } //Число микросекунд
  int  wday()   const; //Число дней, прошедших с воскресенья, от 1 до 7
  bool leap()   const; //Високосный год
  int  mweek()  const; //Неделя месяца, от 1 до 7
  int  lmweek() const; //Количество дней недели в месяце
  int  lmday()  const; //Дней в месяце 
  
  Date& year(int a)  { return set(YEAR, a); }
  Date& mday(int a)  { return set(MDAY, a); }
  Date& mon(int a)   { return set(MON,  a); } 
  Date& hour(int a)  { return set(HOUR, a); }
  Date& minut(int a) { return set(MINUT,a); }
  Date& sec(int a)   { return set(SEC,  a); }
  Date& msec(int a)  { return set(MSEC, a); }
  Date& usec(int a)  { return set(USEC, a); }
  
  
  bool cron_match(const char*) const; // соответствие шаблону в стиле cron
  Date cron_next(const char* tmpl) const;
  Date cron_next(const std::string& tmpl) const { return cron_next(tmpl.c_str()); }
  static Date cron_next(const char* tmpl, const Date&);
  void cron_debug();
private:
  std::string to_string(struct tm &m, const char* format) const;
  
  typedef struct VECTOR{
    VECTOR():r(0),L(false) {}
    std::vector<int> p,q; // делимое и делитель
    char r;  // разделитель [/#]
    bool L;  // есть ли символ 'L'
  } VECTOR;

  typedef struct std::vector< VECTOR > NET; 
  
  typedef struct {
    int min,max;
  } INTERVAL;

  static bool cron_net(NET &net, const char* tmpl, const Date& = now());  // Заполним матрицу сетью допустимых значений
  static bool parse(std::string tmpl, int mod, INTERVAL& I, std::vector<int> &v);
  static void parse(const std::string& tmpl, std::vector<std::string> &v);
  static bool spec_cond(const NET &net, const Date &d); //Специальная проверка
  
  char d_buff[100];
};

inline Date
operator+(const Date& a, const Date& b) {
  Date c = a;
  c+=b;
  return c;
};

inline Date
operator-(const Date& a, const Date& b) {
  Date c = a;
  c-=b;
  return c;
};

#endif //DATE_H
