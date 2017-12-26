/***********************************************************************************
                         anselm.ru [2016-10-25] GNU GPL
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

#define debug1 debug

#if DEBUG==1
#define debug(A) A
#define debug2(A)
#define debug3(A)
#elif DEBUG==2
#define debug(A) A
#define debug2(A) A
#define debug3(A)
#elif DEBUG==3
#define debug(A) A
#define debug2(A) A
#define debug3(A) A
#else
#define debug(A)
#define debug2(A)
#define debug3(A)
#endif


extern const char* SYSLOGNAME;

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
  //if(SYSLOGNAME) {
    openlog(SYSLOGNAME, 0, LOG_USER);
    syslog(LOG_NOTICE, "%s", s);
    closelog();
  //}
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
  //if(SYSLOGNAME) {
    openlog(SYSLOGNAME, 0, LOG_USER);
    if(ern==0)
      syslog(LOG_ERR, "error: %s", s);
    else    
      syslog(LOG_ERR, "error %d: %s - %s", ern, strerror(ern), s);
    closelog();
  //}
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
  openlog(SYSLOGNAME, 0, LOG_USER);
  syslog(LOG_NOTICE, "%s%s", prefix, buf);
  closelog();
#else
  printf("%s%s\n", prefix, buf);
#endif

  delete [] buf;
}

#endif //LOG_H
