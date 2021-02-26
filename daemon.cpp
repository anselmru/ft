/***********************************************************************************
                         anselm.ru [2016-10-25] GNU GPL
***********************************************************************************/
#include "daemon.h"
#include "log.h"

#include <libgen.h>   //basename
#include <sys/stat.h> //O_RDWR | O_EXCL | O_CREAT
#include <unistd.h>   //read
#include <syslog.h>
#include <strings.h>  //bzero

const char* SYSLOGNAME = NULL;
Daemon * self = NULL;  // здесь будет храниться адрес экземпляра класса

void
sig(int sig, siginfo_t *siginfo, void *context) {
  //warning("Sending PID: %ld, UID: %ld\n", (long)siginfo->si_pid, (long)siginfo->si_uid);
  //warning("debug: stop signal %d", signum);
  if(!self) return;
  
  switch(siginfo->si_signo) {
    case SIGHUP:   self->reload(); break;
    case SIGTERM:; self->wake();   break;
  }
}

Daemon::Daemon(const char* binfile, const char* conffile, const char* pidfile, bool simple_name) {
  self = this;
  
  if(!binfile) {
    fprintf(stderr, "Error: variable \"APPNAME\" not defined\n");
    throw "binfile";
  }
  APPNAME = strdup(binfile);

  const char *tmp = conffile ? conffile : getenv("config");
  if(!tmp) {
    fprintf(stderr, "Error: variable environment \"config\" not defined\n");
    throw "conffile";
  }
  CONFFILE = strdup(tmp);//strdup - обязательно!!
  
  if(simple_name)
    SYSLOGNAME = strdup(APPNAME);
  else {
    const char *CONF = basename((char*)CONFFILE);
    char *tmp = new char[strlen(APPNAME)+strlen(CONF)+1];
    sprintf(tmp, "%s %s", APPNAME, CONF);
    SYSLOGNAME = strdup(tmp);
    delete [] tmp;
  }
  
  tmp = pidfile ? pidfile : getenv("pidfile");
  if(!tmp) {
    fprintf(stderr, "Error: variable environment \"pidfile\" not defined\n");
    throw "pidfile";
  }
  PIDFILE = strdup(tmp);//strdup - обязательно!!
}

Daemon::~Daemon() {
  delete [] APPNAME;
  delete [] CONFFILE;
  delete [] PIDFILE;
}

void
Daemon::childcode() {
  warning1("debug: childcode - enter");
  //warning("info: start");
  childloop();
  //warning("info: stop");
  warning1("debug: childcode - exit");
}


void
Daemon::childloop() {

// Example code, may be overload:

  warning1("debug: childloop - enter");

  timeval tv = {10,0};
  
  while(1) {
    time_t t = time(NULL);
    warning("%d", t); //printf("%d\n", t);
    if(wait(tv)) break;// чутко спим до следующего опроса
  }
  
  warning1("debug: childloop - exit");
}

bool
Daemon::start() {

  int f = open(PIDFILE, O_RDWR | O_EXCL | O_CREAT, S_IRUSR | S_IWUSR);
  if(f<0) {
    f = open(PIDFILE, O_RDONLY);
    char buf[21];
    read(f, buf, 20);
    int pid = atoi(buf);
    close(f);
    int i = kill(pid, 0);
    if(i<0) {
      remove(PIDFILE);
      f = open(PIDFILE, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
    }
    else {
      fprintf(stderr, "%s (pid=%d) already running\n", APPNAME, pid);
      return false;
    }
  }
  
  int pid;
  pid = fork();
  switch(pid) {
  case 0:
    close(f);
    setsid();
    chdir("/");
    close(0);
    close(1);
    close(2);
    
    struct sigaction act;
    bzero(&act, sizeof(act));    
    act.sa_sigaction = &sig; //act.sa_handler = term;
    act.sa_flags = SA_SIGINFO; // говорим, что не используем "act.sa_handler = term"
    sigaction(SIGTERM, &act, NULL); //signal(SIGTERM, term);
    sigaction(SIGHUP,  &act, NULL); //signal(SIGHUP,  reload);
    signal(SIGPIPE, SIG_IGN);  // отключаем обработку этого сигнала
    childcode();
    remove(PIDFILE);
    
    return true;
  case -1:
    fprintf(stderr, "Error: unable to fork\n");
    break;
  default:
    char buf[21] = {0};
    sprintf(buf, "%d\n", pid);
    write(f, buf, 20);
    break;
  }
    
  close(f);
  
  return true;
}

void
Daemon::reload() {
  warning("reload");
}
