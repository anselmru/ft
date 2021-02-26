/***********************************************************************************
                         anselm.ru [2016-10-25] GNU GPL
***********************************************************************************/
#pragma once
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>  //siginfo_t
#include "event.h"

class Daemon : public Event {
friend void sig(int sig, siginfo_t *siginfo, void *context); //TODO третий параметр не такой
public:
  Daemon(const char* binfile, const char* conffile = NULL, const char* pidfile = NULL, bool simple_name = true);
  virtual ~Daemon();
  bool start();
  const char* config() const { return CONFFILE; }
protected: 
  virtual void childloop();
  virtual void reload();
private:
  const char* APPNAME;
  const char* CONFFILE; 
  const char* PIDFILE;
  
  void childcode();
};
