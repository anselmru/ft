#include "dem.h"
#include "log.h"
#include "date.h"
#include "dev.h"

Dem::Dem(const char* binfile, const char* conffile, const char* pidfile)
: Daemon(binfile, conffile, pidfile)  {
}

void
Dem::childloop() {
  warning("info: start");

  XML cfg = config();
  const int rate = cfg["timeout"]["rate"].to_int(10) * 1000;
  debug2("Dem::childloop rate=%d", rate);
  Dev k(cfg);
  
  while(1) { // раз в минуту сверяем крон-шаблоны с текущим временем
    k.check_file();
    if(wait(rate)) break;// чутко спим
    //sleep();
    k.pass();
    sleep(1); // против зацикливания
  }
  
  warning("info: stop");
}
