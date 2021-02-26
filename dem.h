#include "daemon.h"

class Dem: public Daemon {
public:
  Dem(const char* binfile, const char* conffile = NULL, const char* pidfile = NULL);
protected:
  void childloop();
};
