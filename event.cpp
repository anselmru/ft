/***********************************************************************************
                         anselm.ru [2016-10-25] GNU GPL
***********************************************************************************/
#include "event.h"
#include <errno.h>

Event::Event()
: d_mutex(PTHREAD_MUTEX_INITIALIZER)
, d_cond(PTHREAD_COND_INITIALIZER)
, d_active(false)
{
  //d_mutex = PTHREAD_MUTEX_INITIALIZER;
  //d_cond  = PTHREAD_COND_INITIALIZER;
  /*int err;
  pthread_mutexattr_t mutex_attr; 
  err = pthread_mutexattr_init(&mutex_attr);                                if(err) warning("pthread_mutexattr_init=%d", err);
  err = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);   if(err) warning("pthread_mutexattr_settype=%d", err); //PTHREAD_MUTEX_NORMAL
  err = pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_PRIVATE); if(err) warning("pthread_mutexattr_setpshared=%d", err);
  err = pthread_mutex_init(&d_alert_mutex, &mutex_attr);                    if(err) warning("pthread_mutex_init=%d", err);
  err = pthread_mutexattr_destroy(&mutex_attr);                             if(err) warning("pthread_mutexattr_destroy=%d", err);
  
  pthread_condattr_t cond_attr;
  err = pthread_condattr_init(&cond_attr);                                  if(err) warning("pthread_condattr_init=%d", err);
  err = pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_PRIVATE);   if(err) warning("pthread_condattr_setpshared=%d", err);
  err = pthread_condattr_setclock(&cond_attr, CLOCK_REALTIME);              if(err) warning("pthread_condattr_setclock=%d", err); //CLOCK_MONOTONIC "спасает от NTP"
  err = pthread_cond_init(&d_alert_cond, &cond_attr);                       if(err) warning("pthread_cond_init=%d", err);
  err = pthread_condattr_destroy(&cond_attr);                               if(err) warning("pthread_condattr_destroy=%d", err);
  */
}

Event::~Event() {
  pthread_cond_destroy(&d_cond);
  pthread_mutex_destroy(&d_mutex);
}

// ф-я пробуждения
void
Event::wake() {
  d_active = true;
  pthread_cond_signal(&d_cond);  
}

// сброс события
void
Event::reset() {
  d_active = false;
}

// ф-я бесконечного ожидания
void
Event::wait() {
  if(d_active) return;
  
  pthread_mutex_lock(&d_mutex); // перед вызовом pthread_cond_wait мютекс должен быть заблокирован
  pthread_cond_wait(&d_cond, &d_mutex);
  pthread_mutex_unlock(&d_mutex);
}

// ф-я конечного ожидания
bool
Event::wait(const timeval& tv) {
  if(d_active) return d_active; // если сигнал пробуждения был послан во время бодрствования, т.е. раньше, чем началась процедура wait
  
  pthread_mutex_lock(&d_mutex); // перед вызовом pthread_cond_wait мютекс должен быть заблокирован
    
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  
  ts.tv_nsec += tv.tv_usec * 1000; // usec to nsec
  ts.tv_sec  += tv.tv_sec + ts.tv_nsec / 1000000000;
  ts.tv_nsec %= 1000000000; // переполнение не допускается
  
  int ret = pthread_cond_timedwait(&d_cond, &d_mutex, &ts);
  pthread_mutex_unlock(&d_mutex);
  
  
  return ret!=ETIMEDOUT; // дождались ли ответа вовремя //ETIMEDOUT=60
}

// ф-я конечного ожидания
bool
Event::wait(long msec) {  
  struct timeval tv = {msec/1000, (msec%1000)*1000000};
  return wait(tv);
}
