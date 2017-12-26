/***********************************************************************************
                         anselm.ru [2016-10-25] GNU GPL
***********************************************************************************/
#pragma once

#include <pthread.h>
#include <sys/time.h>

class Event {
public:
  Event();
  virtual ~Event();
  void wake();
  void reset();
  void wait();
  bool wait(const timeval& tv);
  bool wait(long msec);
  bool active() const { return d_active; }
private:
  pthread_mutex_t d_mutex;
  pthread_cond_t  d_cond;
  bool d_active;
};
