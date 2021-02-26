/***********************************************************************************
                         anselm.ru [2016-10-26] GNU GPL
***********************************************************************************/
#include "date.h"
#include <stdio.h>   //sprintf
#include <math.h>    //pow
#include <iterator>
#include <sstream>
#include <algorithm> //sort
#include <stdlib.h>  //strtol

#ifdef WIN32
#define DST_NONE 0
//#pragma warning (disable: 4996)//UNSAFE
#ifndef localtime_r
#define localtime_r(_Time, _Tm) ({ struct tm *___tmp_tm =   \
            localtime((_Time)); \
            if (___tmp_tm) {  \
              *(_Tm) = *___tmp_tm;  \
              ___tmp_tm = (_Tm);  \
            }     \
            ___tmp_tm;  })
#endif
#ifndef gmtime_r
#define gmtime_r(_Time,_Tm) ({ struct tm *___tmp_tm =   \
            gmtime((_Time));  \
            if (___tmp_tm) {  \
              *(_Tm) = *___tmp_tm;  \
              ___tmp_tm = (_Tm);  \
            }     \
            ___tmp_tm;  })
#endif

void
bzero(void* s, size_t n) {
  memset(s, 0, n);
}

time_t
timegm(struct tm *tm) {
  return mktime(tm) - Date::timezone() * 60;
}
#endif //WIN32

const char* Date::USEC = "usec";
const char* Date::MSEC = "msec";
const char* Date::SEC  = "sec";
const char* Date::MINUT= "minut";
const char* Date::HOUR = "hour";
const char* Date::MDAY = "mday";
const char* Date::MON  = "mon";
const char* Date::YEAR = "year";
const char* Date::YDAY = "yday";
const size_t Date::DATE_BUFF_MIN_SIZE = strlen("2014-01-01 00:00:00.123456+0000")+1;

using namespace std;

const char Date::FORMAT_OUT[] = "%Y-%m-%d %H:%M:%S.%N%z"; // %N - микросекунды
const char FORMAT_IN[]        = "%u-%u-%u%*[^0-9]%u:%u:%u";

/****************************** Constructors ******************************/

Date::Date() {
  tv_sec  = 0;
  tv_usec = 0;
}

Date::Date(timeval tv) {
  tv_sec  = tv.tv_sec;
  tv_usec = tv.tv_usec;
}

/*******************************************************************************
Распознание будет осуществляться по такому правилу:
если имеется указание на временную зону, значит речь идёт об времени UTC;
если временная зона не указана, значит предполагается время местное.
******************************************************************************/
Date&
Date::operator=(const char* A) {
//Date& Date::operator=(const char* A) {
  tv_sec  = 0;
  tv_usec = 0;
  if(strlen(A)<8) return *this;
  
  char *a = strdup(A);
  // распознаем микросекунды
  char *s;
  if( (s=strchr(a, '.')) && s++ ) {
    int len = strspn(s, "0123456789");
    tv_usec = strtol(s, NULL, 10) * pow(10, 6-len);
  }
  else {
    tv_usec = 0;
  }

  // распознаем временнУю зону и удалим её из строки, т.к. она может смутить следующую ф-ю sscanf
  int z=0;
  bool tz_def = false;
  if( strlen(a)>10 && (s=strpbrk(a+10, "-+"))) {
    int hour=0, minut=0;
    sscanf(s, "%3d%d", &hour, &minut);
    z = abs(hour)*60 + minut;
    if(hour>0) z*=-1;
    //printf("z=%d\n", z);
    *s = 0; // отсекаем временную зону
    tz_def = true;
  }
  if(a[strlen(a)-1]=='Z') { // значит, время в UTC
    tz_def = true;
    z=0;
  }
  
  // распознаем дату и время в формате ISO 8601
  int y=0,m=0,d=0,H=0,M=0,S=0;
  sscanf(a, FORMAT_IN, &y, &m, &d, &H, &M, &S);
  
  //printf("%u-%u-%u %u:%u:%u.%u %d\n", y, m, d, H, M, S, tv_usec, z);
  
  struct tm t = {0};
  t.tm_year  = y-1900;
  t.tm_mon   = m-1;
  t.tm_mday  = d;
  t.tm_hour  = H;
  t.tm_min   = M;
  t.tm_sec   = S;
  t.tm_isdst = -1;//очень важно!
  //t.tm_gmtoff = z*3600;
  
  //printf("timegm=%d\n", timegm(&t));
  if(tz_def) // если временная зона указана, то UTC
    tv_sec = timegm(&t)+z*60;
  else       // если временная зона НЕ указана, то местное
    tv_sec = mktime(&t);
    
  delete [] a;
  
  return *this;
}

Date::Date(FILETIME a) {
  typedef long long int64;
  int64 ull = (((int64)a.dwHighDateTime)<<32)+a.dwLowDateTime; 
  ull -= 116444736000000000LL;
  tv_sec = ull / 10000000;
  tv_usec = (ull % 10000000) / 10;
}

Date::Date(double a) {
  double b = (a-25569) * 24*60*60;
  tv_sec = b;
  tv_usec = (b - tv_sec) * 1000000;
  tv_sec += Date::timezone()*60; // double представлено всегда в локальном времени, поэтому переводим в UTC
}

Date::Date(time_t a) {
  tv_sec = a;
  tv_usec = 0;
}

/****************************** Operators ******************************/
Date&
Date::operator+=(const Date& a) {
  tv_sec += a.tv_sec;
  tv_usec += a.tv_usec;
  if(tv_usec >= 1000000) {
    tv_sec += tv_usec / 1000000;
    tv_usec %= 1000000;
  }
  return *this;
}

Date&
Date::operator-=(const Date& a) {
  time_t sec  = a.tv_sec;
  time_t usec = a.tv_usec;

  if (tv_usec < usec) {
    time_t nsec = (usec-tv_usec)/1000000+1;
    usec -= 1000000 * nsec;
    sec += nsec;
  }
  if (tv_usec - usec > 1000000) {
    time_t nsec = (usec-tv_usec)/1000000;
    usec += 1000000 * nsec;
    sec -= nsec;
  }
  tv_sec -= sec;
  tv_usec -= usec;

  return *this;
}

bool
Date::operator<(const Date& a) {
  if(tv_sec < a.tv_sec) return true;
  if(tv_sec ==a.tv_sec && tv_usec < a.tv_usec) return true;
  return false;
}

bool
Date::operator<=(const Date& a) {
  return *this<a || *this==a;
}

bool
Date::operator>(const Date& a) {
  if(tv_sec > a.tv_sec) return true;
  if(tv_sec ==a.tv_sec && tv_usec > a.tv_usec) return true;
  return false;
}

bool
Date::operator>=(const Date& a) {
  return *this>a || *this==a;
}

bool
Date::operator==(const Date& a) {
  return tv_sec==a.tv_sec && tv_usec==a.tv_usec;
}

bool
Date::operator!=(const Date& a) {
  return tv_sec!=a.tv_sec || tv_usec!=a.tv_usec;
}

int
Date::operator[](const char* a) const {
  return get(a);
}

int
Date::operator[](int a) const {
  return get(a);
}

/****************************** Methods ******************************/

Date
Date::now() {
#ifdef WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  Date d = ft;
  return ft;
#else
  timeval t;
  gettimeofday(&t, DST_NONE);
  return t;
#endif
}

string
Date::to_string(struct tm &t, const char* FORMAT) const {
  const int n = strlen(FORMAT) + DATE_BUFF_MIN_SIZE;
  char *format = new char[n];
  strcpy(format, FORMAT);
  
  char* p = NULL;
  
#ifdef WIN32  
  p = (p=strstr(format, "%z")) ? p : strstr(format, "%Z");
  if(p) {
    char *r = new char[n]; //правая половина
    char *l = new char[n]; //левая половина
    strcpy(r, p+2);
    *p = 0;
    int z = timezone();
    int hour = abs(z)/60, minut = abs(z)%60;
    sprintf(l, "%c%02d%02d", z>0 ? '-':'+', hour, minut); // POSIX
    strcat(format, l);
    strcat(format, r);
    delete [] l;
    delete [] r;
  }
#endif

  p = strstr(format, "%N");
  if(p) { // микросекунды
    char *r = new char[n]; //правая половина
    char *l = new char[n]; //левая половина
    strcpy(r, p+2);
    *p = 0;    
    sprintf(l, "%06ld", tv_usec);
    strcat(format, l);
    strcat(format, r);
    delete [] l;
    delete [] r;
  }
  
  char *buff = new char[n];
  strftime(buff, n-1, format, &t);// -1 на всякий случай (не повредит)
  string s(buff);
  
  delete [] format;
  delete [] buff;
  
  return s;
}

string
Date::to_string(const char* format) const {
  struct tm t = {0};
  if(format[strlen(format)-1]=='Z' && format[strlen(format)-2]!='%') // вывести время в UTC
    gmtime_r( &tv_sec, &t );
  else
    localtime_r( &tv_sec, &t );
    
  return to_string(t, format);
}

const char*
Date::c_str(const char* format) {
  std::string buff = to_string(format);
  bzero(d_buff, sizeof(d_buff));
  buff.copy(d_buff, buff.size());
  //return to_string(format).c_str();
  return d_buff;
}

double
Date::to_double() const {
  return (double)(tv_sec - Date::timezone()*60 + (double)tv_usec/1000000 )/(24*60*60) + 25569;
}

int
Date::timezone() { // часовая зона в минутах
  int ret = 0;
#ifdef WIN32
  TIME_ZONE_INFORMATION tzi;
  GetTimeZoneInformation(&tzi);
  ret = tzi.Bias;
#else
  time_t ts = 0;
  struct tm t;
  char buf[16];
  localtime_r(&ts, &t);
  strftime(buf, sizeof(buf), "%z", &t); // POSIX +0500  
  int hour=0, minut=0;
  sscanf(buf, "%3d%d", &hour, &minut);
  ret = abs(hour)*60 + minut;
  if(hour>0) ret*=-1;
  //strftime(buf, sizeof(buf), "%Z", &t);
  //sscanf(buf, "%*[^0-9]%d", &ret);
#endif
  return ret;
}

int
Date::wday() const {
  struct tm t = {0};
  localtime_r(&tv_sec, &t);
  return t.tm_wday ? t.tm_wday : 7;
}

bool
Date::leap() const {
  struct tm t = {0};
  localtime_r(&tv_sec, &t);
  int y = t.tm_year;
  return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

int
Date::lmday() const { // дней в месяце
  int d[12] = {31, leap()?29:28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return d[ mon()-1 ];
}

int
Date::mweek() const { // номер дня недели в месяце
  int mon0 = mon();
  int r; // искомый результат
  for(r=1; r<5; r++) { // # за пять циклов(недель) обязательно найдём
    Date d = tv_sec - r*7*24*60*60;
    //printf("%s\n", d.c_str());
    int mon = d.mon();
    if(mon0!=mon) return r;
  } 
  return 0;
}

int
Date::lmweek() const { // последний понедельник месяца, последний вторник, и т.д.
  int mon0 = mon();
  int r; // искомый результат
  for(r=1; r<5; r++) { // # за пять циклов(недель) обязательно найдём
    Date d = tv_sec + r*7*24*60*60;
    //printf("%s\n", d.c_str());
    int mon = d.mon();
    if(mon0!=mon) break;
  }
  
  return r-1+mweek();
}

Date
Date::trunc(const char *a) const {
  struct tm t = {0};
  localtime_r(&tv_sec, &t);
  
  if( strcmp(SEC, a)==0 ) {
  }
  else if( strcmp(MINUT, a)==0 ) {
    t.tm_sec = 0;
  }
  else if( strcmp(HOUR, a)==0 ) {
    t.tm_sec = 0;
    t.tm_min = 0;
  }
  else if( strcmp(MDAY, a)==0 ) {
    t.tm_sec = 0;
    t.tm_min = 0;
    t.tm_hour = 0;
  }
  else if( strcmp(MON, a)==0 ) {
    t.tm_sec = 0;
    t.tm_min = 0;
    t.tm_hour = 0;
    t.tm_mday = 1;
  }
  else {
    return *this;
  }
  
  Date b;
  b.tv_usec = 0;
  b.tv_sec  = mktime(&t);
  return b;
}

int
Date::get(const char *a) const {
  struct tm t = {0};
  localtime_r(&tv_sec, &t);
  
  if( strcmp(USEC,       a)==0 )
    return tv_usec;
  else if( strcmp(MSEC,  a)==0 )
    return tv_usec / 1000;
  else if( strcmp(SEC,   a)==0 )
    return t.tm_sec;
  else if( strcmp(MINUT, a)==0 )
    return t.tm_min;
  else if( strcmp(HOUR,  a)==0 )
    return t.tm_hour;
  else if( strcmp(MDAY,  a)==0 )
    return t.tm_mday;
  else if( strcmp(MON,   a)==0 )
    return t.tm_mon+1;
  else if( strcmp(YEAR,  a)==0 )
    return t.tm_year + 1900;
  else if( strcmp(YDAY,  a)==0 )
    return t.tm_yday+1;
  else
    return -1;
}

int
Date::get(int a) const {
  struct tm t = {0};
  localtime_r(&tv_sec, &t);
  
  switch (a) {
    case 6: return tv_usec; break;
    case 5: return t.tm_sec; break;
    case 4: return t.tm_min; break;
    case 3: return t.tm_hour; break;
    case 2: return t.tm_mday; break;
    case 1: return t.tm_mon+1; break;
    case 0: return t.tm_year + 1900; break;
  }
  
  return -1;
}

Date&
Date::set(int y, int m, int d, int H, int M, int S, int N) {
  struct tm t = {0};
  localtime_r(&tv_sec, &t);
  t.tm_year = y-1900;
  t.tm_mon  = m-1;
  t.tm_mday = d;
  t.tm_hour = H;
  t.tm_min  = M;
  t.tm_sec  = S;
  tv_usec   = N;
  tv_sec = mktime(&t);
  return *this;
}

Date
Date::date(const char* a, int i) const {
  Date d = *this;
  
  struct tm t = {0};
  localtime_r(&tv_sec, &t);
  
  if( strcmp(USEC,       a)==0 )
    d.tv_usec = i;
  else if( strcmp(MSEC,  a)==0 )
    d.tv_usec = i * 1000;
  else if( strcmp(SEC,   a)==0 )
    t.tm_sec = i;
  else if( strcmp(MINUT, a)==0 )
    t.tm_min = i;
  else if( strcmp(HOUR,  a)==0 )
    t.tm_hour = i;
  else if( strcmp(MDAY,  a)==0 )
    t.tm_mday = i;
  else if( strcmp(MON,   a)==0 )
    t.tm_mon = i-1;
  else if( strcmp(YEAR,  a)==0 )
    t.tm_year = i-1900;
  else
    return d;

  d.tv_sec = mktime(&t);
      
  return d;
}

Date&
Date::set(const char* a, int i) {
  *this = date(a, i);
  return *this;
}

Date
Date::date(int a, int i) const {
  Date d = *this;
  
  struct tm t = {0};
  localtime_r(&tv_sec, &t);
  
  switch (a) {
    case 6: d.tv_usec = i; break;
    case 5: t.tm_sec  = i; break;
    case 4: t.tm_min  = i; break;
    case 3: t.tm_hour = i; break;
    case 2: t.tm_mday = i; break;
    case 1: t.tm_mon  = i-1; break;
    case 0: t.tm_year = i-1900; break;
    default: return d;
  }
  
  d.tv_sec = mktime(&t);
      
  return d;
}

Date&
Date::set(int a, int i) {
  *this = date(a, i);
  return *this;
}

Date
Date::date(int y, int m, int d, int H, int M, int S, int N) {
  struct tm t = {0};
  t.tm_year = y-1900;
  t.tm_mon  = m-1;
  t.tm_mday = d;
  t.tm_hour = H;
  t.tm_min  = M;
  t.tm_sec  = S;
  
  timeval tv;
  tv.tv_usec = N;
  tv.tv_sec  = mktime(&t);
  return tv;
}

bool
Date::check(int y, int m, int d, int H, int M, int S) {
  if(m<1 || m>12) return false;
  if(H<0 || H>23) return false;
  if(M<0 || M>59) return false;
  if(S<0 || S>59) return false;
  Date dd;
  int n = dd.year(y).mon(m).lmday();
  if(d<1 || d>n) return false;
  
  return true;
}

/****************************************************************************************

                                         CRON

****************************************************************************************/

string
replace(string& a, const string& b, const string& c) {
  size_t index = 0;
  while (true) {
    index = a.find(b, index);
    if (index == string::npos) break;
    a.replace(index, b.size(), c);
    index += c.size();
  }
  return a;
}

bool
in(int a, const vector<int>& v) {
  return find(v.begin(), v.end(), a)!=v.end();
}

void
normal(vector<int> &v) { // Нормализуем последовательность  
  sort(v.begin(), v.end(), less<int>());  
  int max = *(--v.end());//!v.empty()
  unique(v.begin(), v.end());
  vector<int>::iterator last = find(v.begin(), v.end(), max); // удалим последние повторяющиеся
  if(last!=v.end()) v.erase(++last, v.end());
}

bool
Date::spec_cond(const NET &net, const Date& d) {//Специальная проверка

  if( net[3].L && d.mday()!=d.lmday() ) { // если день месяца не последний
    //printf("LAST_MDAY\n");
    return false;
  }
  
  if( !in(d.wday(), net[6].p) ) { // ищем только если множество дней недели не пусто - во избежание бесконечного цикла
    //printf("NEXT_WDAY\n");
    return false;
  }
  
  if( (  net[6].L && d.mweek()!=d.lmweek() )    // если неделя месяца не последняя, либо...
     ^( !net[6].L && !net[6].q.empty() && !in(d.mweek(), net[6].q) ) )   // если неделя месяца не соответствует
  {
    //printf("LAST_MWEEK\n");
    return false;
  }
  
  return true;
}


bool
Date::parse(string tmpl, int mod, INTERVAL& I, vector<int> &v) {
  replace(tmpl, ",", " ");
  replace(tmpl, "L", "");  // можно здесь избавиться, т.к. признак уже взят в VECTOR::L
  int i = 0;
  if(tmpl[0]=='*') {
    for(int i=I.min; i<=I.max; i+=mod) v.push_back(i);
  }
  else {
    stringstream ss(tmpl);
    copy(istream_iterator<int>(ss), istream_iterator<int>(), back_inserter(v));//inserter(v, v.end())
  }
  for(int i=v.size()-1; i>=0; i--) {
    if(v[i]<0) {
      v[i]*=-1;
      int b = v[i];
      int a = v[--i];
      for(int j=a+1; j<b; j++) v.push_back(j);
    }
  }
  if(v.empty()) return false;
  
  normal(v); // Нормализуем последовательность
  
  // Удалим всё, что выше диапазона
  vector<int>::iterator J = find_if(v.begin(), v.end(), bind2nd( greater<int>(), I.max ) );
  if(J!=v.end()) v.erase(J, v.end());
  
  // Удалим всё, что ниже диапазона
  J = find_if(v.begin(), v.end(), bind2nd( less<int>(), I.min ) );
  if(J!=v.end()) v.erase(v.begin(), J+1);
  
  return !v.empty();
}

void
Date::parse(const string& tmpl, vector<string> &v) {
  stringstream ss(tmpl);
  copy(istream_iterator<string>(ss), istream_iterator<string>(), back_inserter(v));
}

// Заполним матрицу сетью допустимых значений
bool
Date::cron_net(NET &net, const char* tmpl, const Date& dt) {
  const int Y = dt.year();
  
  INTERVAL e;
  vector<INTERVAL> I;
  e.min = 0; e.max = 59; // секунды
  I.push_back(e);
  e.min = 0; e.max = 59; // минуты
  I.push_back(e);
  e.min = 0; e.max = 23; // часы
  I.push_back(e);
  e.min = 1; e.max = 31; // дни 
  I.push_back(e);
  e.min = 1; e.max = 12; // месяцы
  I.push_back(e);
  e.min = Y; e.max = Y+1; // годы (ищем в пределах только двух лет)
  I.push_back(e);
  e.min = 1; e.max = 7; // недели
  I.push_back(e);
  
  // Заполним вектор шаблонов
  vector<string> v;
  string s;
  if(tmpl) s = tmpl;
  s = "* " + s + " * * * * * *"; // секунды будут в шаблоне !!любые [2016-10-21]!!
  parse(s, v);  
  //warning("1) templ=%s, I.size=%d, v.size=%d", s.c_str(), I.size(), v.size());
  v.resize(I.size());
  //warning("2) I.size=%d, v.size=%d", I.size(), v.size());
  for(int i=0; i<v.size(); i++) {
    
    VECTOR a;

    a.L   = v[i].find_first_of("L") != -1; // есть ли в выражении символ 'L'
    int k = v[i].find_first_of("/#");
    
    if(k>0) {
      a.r = v[i][k];
      if(a.r=='/') { // модуль кратности
        int mod = 0;
        sscanf(v[i].c_str()+k+1, "%d", &mod);
        parse(v[i].c_str(), mod, I[i], a.p);
      }
      else if(a.r=='#') { // перечисление через запятую
        parse(v[i].c_str(),     1, I[i], a.p);
        parse(v[i].c_str()+k+1, 1, I[i], a.q);
      }
    }
    else {
      parse(v[i].c_str(), 1, I[i], a.p);
    }

    net.push_back(a);
  }
  
  // Сузим диапазон поиска суток (быстрее будет искать)
  if(net[3].L) {
    int a[] = {28,29,30,31};
    net[3].p.insert(net[3].p.end(), a, a+4);
    normal(net[3].p);
  }
  
  for(int i=0; i<7; i++)
    if(net[i].p.empty()) return false; // такое недопустимо - сеть не должна быть дырявой
  
/*  for(int i=0; i<7; i++) {
    for(int j=0; j<net[i].p.size(); j++)
      printf("%d ", net[i].p[j]);
    printf("%c", net[i].r);
    for(int j=0; j<net[i].q.size(); j++)
      printf("%d ", net[i].q[j]);
    printf("\n");
  }*/
  
  return true;
}

bool
Date::cron_match(const char* tmpl) const {
  // Получим текущее местное время
  struct tm m = {0};
  localtime_r(&tv_sec, &m);
  // Заполним вектор временной точки
  vector<int> v;
  v.push_back(m.tm_sec);
  v.push_back(m.tm_min);
  v.push_back(m.tm_hour);
  v.push_back(m.tm_mday);
  v.push_back(m.tm_mon+1);
  v.push_back(m.tm_year+1900);
  v.push_back(m.tm_wday ? m.tm_wday : 7);
  
  NET net;
  if(!cron_net(net, tmpl, *this)) return false; //! *this [2016-10-21]
  
  for(int i=0; i<7; i++) if( !in(v[i], net[i].p) ) return false; // перебор по периодам
  
  if( !spec_cond(net, *this) ) {
    return false;
  }

  return true;
}

Date
Date::cron_next(const char* tmpl) const {
  return cron_next(tmpl, *this);
}

Date
Date::cron_next(const char* tmpl, const Date& past) {
  int cnt = 0; // счётчик итераций
  NET net;
  if(!cron_net(net, tmpl, past)) { // если сеть дырявая
    //printf("cnt=%d (empty net)\n", cnt);
    return (time_t)0;//TODO WIN32
  }
  Date next = past.trunc(MDAY);
  //printf("past=%s\n", past.c_str());
  //printf("next=%s\n", next.c_str());
  
  for(int i5=0; i5<net[5].p.size(); i5++) {
    for(int i4=0; i4<net[4].p.size(); i4++) {
      for(int i3=0; i3<net[3].p.size(); i3++) {
        for(int i2=0; i2<net[2].p.size(); i2++) {
          for(int i1=0; i1<net[1].p.size(); i1++) {
            //for(int i0=0; i0<net[0].p.size(); i0++) {
            int y = net[5].p[i5];
            int m = net[4].p[i4];
            int d = net[3].p[i3];
            int H = net[2].p[i2];
            int M = net[1].p[i1];
            int S = 0;//net[0][i0];
            cnt++;
            
            if( !Date::check(y, m, d, H, M, S) ) {
              //printf("bad day=%d for month=%d\n", d, m);
              goto NEXT_MON;
            }
            next.set(y, m, d, H, M, S); // дата всегда возрастает, т.к. вся сеть возрастающая
            //printf("%s ~ ", next.c_str());
            
            long u = past - next;
            if(u>0 && u/3600/24/366>0 ) {
              //printf("NEXT_YEAR\n");
              goto NEXT_YEAR;
            }
            if(u>0 && u/3600/24/31>0 ) {
              //printf("NEXT_MON %ld\n", u);
              goto NEXT_MON;
            }
            if(u>0 && u/3600/24>0 ) {
              //printf("NEXT_DAY\n");
              goto NEXT_DAY;
            }
            
            if( !spec_cond(net, next) ) {
              goto NEXT_DAY;
            }
            
            if(u>0 && u/3600>0 ) {
              //printf("NEXT_HOUR\n");
              goto NEXT_HOUR;
            }
            if(u>0 && u/60>0 ) {
              //printf("NEXT_MIN\n");
              goto NEXT_MIN;
            }
            //if(next>past && next.cron_match(tmpl)) {
            if(next>past) {
              //printf("cnt=%d\n", cnt);
              return next;
            }
            
            NEXT_MIN:;
          }
          NEXT_HOUR:;
        }
        NEXT_DAY:;
      }
      NEXT_MON:;
    }
    NEXT_YEAR:;
  }
  
  //printf("cnt=%d\n", cnt);
  return (time_t)0;//TODO WIN32
}
