/***********************************************************************************
*                                 Логирование.                                     *
*                                                         anselm.ru [2021-02-26]   *
***********************************************************************************/
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <libgen.h> //basename
#include <fstream>
#include <fcntl.h>
#include <unistd.h> //read
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h> //strlen
#include <stdlib.h> //malloc

#define debug warning

#define debug1(...)     if(DEBUG>=1) debug(__VA_ARGS__)
#define debug2(...)     if(DEBUG>=2) debug(__VA_ARGS__)
#define debug3(...)     if(DEBUG>=3) debug(__VA_ARGS__)

#define error1(...)     if(DEBUG>=1) error(__VA_ARGS__)
#define error2(...)     if(DEBUG>=2) error(__VA_ARGS__)
#define error3(...)     if(DEBUG>=3) error(__VA_ARGS__)

#define warning1(...)   if(DEBUG>=1) warning(__VA_ARGS__)
#define warning2(...)   if(DEBUG>=2) warning(__VA_ARGS__)
#define warning3(...)   if(DEBUG>=3) warning(__VA_ARGS__)

#define print_bin1(...) if(DEBUG>=1) print_bin(__VA_ARGS__)
#define print_bin2(...) if(DEBUG>=2) print_bin(__VA_ARGS__)
#define print_bin3(...) if(DEBUG>=3) print_bin(__VA_ARGS__)


extern const char *__progname;

inline void
warning(const char* format, ...) {
  va_list paramList;
  va_start(paramList, format);
  int len = 2*strlen(format)+1; // предполагаем длину строки
  char *s = (char*)malloc(len);
  int i = vsnprintf(s, len, format, paramList);
  if(i>=len) {
    len = i+1;
    s = (char*)realloc(s, len);
    vsnprintf(s, len, format, paramList);
  }
  va_end(paramList);
    
#ifdef SYSLOG
  const char* SYSLOGNAME = getenv("daemon") ? getenv("daemon") : __progname;
  openlog(SYSLOGNAME, 0, LOG_USER);
  syslog(LOG_NOTICE, "%s", s);
  closelog();
#else
  printf("%s\n", s);
#endif
  
  free(s);
}


inline void
error(const char* format, ...) {
  int ern = errno;
  
  va_list paramList;
  va_start(paramList, format);
  int len = 2*strlen(format)+1; // предполагаем длину строки
  char *s = (char*)malloc(len);
  int i = vsnprintf(s, len, format, paramList);
  if(i>=len) {
    len = i+1;
    s = (char*)realloc(s, len);
    vsnprintf(s, len, format, paramList);
  }
  va_end(paramList);
  
#ifdef SYSLOG
  const char* SYSLOGNAME = getenv("daemon") ? getenv("daemon") : __progname;  
  openlog(SYSLOGNAME, 0, LOG_USER);
  if(ern==0)
    syslog(LOG_ERR, "error: %s", s);
  else    
    syslog(LOG_ERR, "error %d: %s - %s", ern, strerror(ern), s);
  closelog();
#else  
  if(ern==0)
    fprintf(stderr, "error: %s\n", s);
  else    
    fprintf(stderr, "error %d: %s - %s\n", ern, strerror(ern), s);
#endif

  free(s);
}

inline void
print_bin(const char* prefix, const char* s, const size_t n) {
  char *buf = new char[n*3+1];
  memset(buf, 0, n*3+1);
  for(int i=0; i<n; i++) sprintf(buf+i*3, "%02X ", s[i]&0xff);
  
#ifdef SYSLOG
  const char* SYSLOGNAME = getenv("daemon") ? getenv("daemon") : __progname;
  openlog(SYSLOGNAME, 0, LOG_USER);
  syslog(LOG_NOTICE, "%s%s", prefix, buf);
  closelog();
#else
  printf("%s%s\n", prefix, buf);
#endif

  delete [] buf;
}

#endif //LOG_H
