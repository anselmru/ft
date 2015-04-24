#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <syslog.h>
#include <stdio.h>
#include <string.h> //strcmp
#include <stdlib.h> //atoi
#include <libgen.h> //basename
#include "modbustcp.h"
#include "conf.h"

void help();
void err(const char* msg);
void vers();
void start();
bool env();
void mainloop();
void term(int signum);

char appname[256]  = {0};
char conffile[256] = {0};
char pidfile[256]  = {0};



int main(int argc, char* argv[])
{
  strcpy(appname, basename(argv[0]));
  
  if(argc==2) {
    if( strcmp(argv[1], "-v")==0 ) {
      vers();
      return 0;
    }
    else if( strcmp(argv[1], "-h")==0 ) {
      help();
      return 0;
    }
  }
  
  if(!env()) {
    return 1;
  }
  
  Conf cfg(conffile, "#");
  if(!cfg.exist()) {
    err("Config file not found ");
    return 1;
  }
  
  start();
  
  return 0;
}

void start() {
  int f = open(pidfile, O_RDWR | O_EXCL | O_CREAT, S_IRUSR | S_IWUSR);
  if(f<0) {
    f = open(pidfile, O_RDONLY);
    char buf[21];
    read(f, buf, 20);
    int pid = atoi(buf);
    close(f);
    int i = kill(pid, 0);
    if(i<0) {
      remove(pidfile);
      f = open(pidfile, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
    }
    else {
      printf("%s (pid=%d) already running\n", appname, pid);
      return;
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
    signal(SIGTERM, term);
    signal(SIGPIPE, SIG_IGN);
    openlog(appname,0,LOG_USER);
    syslog(LOG_NOTICE, "start");
    mainloop();
    syslog(LOG_NOTICE, "stop");
    closelog();
    return;
  case -1:
    printf("Error: unable to fork\n");
    break;
  default:
    char buf[21] = {0};
    sprintf(buf, "%d\n", pid);
    write(f, buf, 20);
    break;
  }
    
  close(f);
}

void term(int signum) {
  remove(pidfile);
  syslog(LOG_NOTICE, "stop (signal %d)", signum);
  raise(SIGKILL);
}

void mainloop() {
  Conf cfg(conffile, "#");
	const char *host = cfg.readString("tcp", "host");
  const int port   = cfg.readInteger("tcp", "port");
  const int wait   = cfg.readInteger("tcp", "wait", 1000);
  const int rate   = cfg.readInteger("general", "rate", 10);
  const Conf::Section *s = cfg.readSection("command");
  
  while(1) {
  	
 	  ModbusTCP tcp(host, port);
		tcp.setWait(wait);
		
		Conf::Section::const_iterator i = s->begin();
		for(; i!=s->end(); i++) {
			tcp << *i;  // insert commands
		}
  	
  	tcp.connect();
  	tcp.disconnect();
  	sleep(rate);
  	//printf("sleep %d\n", rate);
  }
}

bool env() {
  char *tmp = getenv("conffile");
  if(!tmp) {
    printf("Error: variable environment \"conffile\" not defined\n");
    return false;
  }
  strcpy(conffile, tmp);
  tmp = getenv("pidfile");
  if(!tmp) {
    printf("Error: variable environment \"pidfile\" not defined\n");
    return false;
  }
  strcpy(pidfile, tmp);
  return true;
}

void help() {
  printf("Usage: %s [-v|-h] [file.conf]\n", appname);
  printf("Options:\n");
  printf("  -h  \t\t: command line options (this page)\n");
  printf("  -v  \t\t: version of service\n");
}

void vers() {
  printf("k104d-1.0, anselm.ru, 2012-12-03.\n");
  printf("Asks device k104.\n");
  printf("License GNU GPL.\n");
}

void err(const char* msg) {
  printf("%s.\nUse \"%s -h\" for getting help.\n", msg, appname);
}


int main1(int argc, char* argv[])
{
  Conf cfg("k104.conf", "#");
  if(!cfg.exist()) {
    err("Config file not found ");
    return 1;
  }

  const char *host = cfg.readString("tcp", "host");
  const int port   = cfg.readInteger("tcp", "port");
  const int wait   = cfg.readInteger("tcp", "wait", 1000);
  const int rate   = cfg.readInteger("general", "rate", 10);
  const Conf::Section *s = cfg.readSection("command");
  

  ModbusTCP tcp(host, port);
  tcp.setWait(wait);
	
	Conf::Section::const_iterator i = s->begin();
	for(; i!=s->end(); i++) {
		tcp << *i;  // insert commands
	}  

	if(tcp.connect() ) {
		//sleep(3);  
		tcp.disconnect();
		
	}
	return 0;
}
