#include "dem.h"
#include "log.h"
#include "date.h"
#include "dev.h"
#include "udp.h"
#include "tcp.h"

Dem::Dem(const char* binfile, const char* conffile, const char* pidfile)
: Daemon(binfile, conffile, pidfile)  {
}

void
Dem::childloop() {
  warning("info: start");

  XML cfg = config();
  const int rate = cfg["timeout"]["rate"].to_int(10) * 1000;
  debug2("Dem::childloop rate=%d", rate);
  
  const char* proto = cfg["dev"]["sock"]["proto"].strdup(); 
  debug2("Dem::childloop proto=%s", proto);
  if( strcmp("udp", proto)==0 ) {
    Dev<UDP> k(cfg);
    while(1) {
      k.check_file();
      if(wait(rate)) break;// чутко спим
      k.pass();
      sleep(1); // против зацикливания
    }
  }
  else if( strcmp("tcp", proto)==0 ) {      
    Dev<TCP> k(cfg);
    while(1) {
    	if(!k.connected()) {
    		warning("disconnected, attempt to reconnect after %dmsec", rate);
    		if(wait(rate)) break;// чутко спим
    		bool cool = k.connect();
    		if(cool)
      		warning("connection restored");
      	else {
      		warning("connection faild");
      		if(wait(rate)) break;// чутко спим
      		continue;
      	}
    	}
      k.check_file();
      if(wait(rate)) break;// чутко спим
      k.pass();
      sleep(1); // против зацикливания
    }
  }
  else {
    error("Unexpected proto = %s\n", proto);
  }
  
  warning("info: stop");
}
