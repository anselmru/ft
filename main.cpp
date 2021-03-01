#include <stdio.h>
#include <libgen.h> //basename
#include <string.h> //strdup
#include "dem.h"
#include "xml.h"
#include "dev.h"
#include "file.h"

#define VERSION     "ft/tcp-2.1, anselm.ru [2021-03-01]"
#define DESCRIPTION "Get data from TEKON by proto FT1.2 over TCP"
#define LICENSE     "GNU GPLv3"
#define HELP        "Usage: %s [-v|-h] [--config=file.conf] [--day=XXXX-XX-XX|--hour='XXXX-XX-XX XX:XX']\n" \
                    "Options:\n" \
                    "  -h [--help]     : command line options (this page)\n" \
                    "  -v [--version]  : version of daemon\n" \
                    "  --day           : put the day for manual getting\n" \
                    "  --hour          : put the hour for manual getting\n" \
                    "  --config        : put the config for manual getting\n"

                    
extern const char *__progname;
                    
int
main( int argc, char *argv[], char *envp[] ) {
  const char *APPNAME  = strdup(basename(argv[0]));
  const char *CONFFILE = NULL;
  const char *PIDFILE  = NULL;
  const char *FILTER   = NULL; 
  
  for(int i=1; i<argc; i++) {
    if( strncmp(argv[i], "--config=", 9)==0) {
      CONFFILE = strdup(&argv[i][9]);
    }
    if( strncmp(argv[i], "--pidfile=", 10)==0) {
      PIDFILE = strdup(&argv[i][10]);
    }
    if( strncmp(argv[i], "--day=", 6)==0 || strncmp(argv[i], "--hour=", 7)==0) {
      FILTER = strdup(&argv[i][2]);
    }
    if( strcmp(argv[1], "-v")==0 ||  strcmp(argv[1], "--version")==0 ) {
      printf("%s\n", VERSION);
      printf("%s\n", DESCRIPTION);
      printf("License %s\n", LICENSE);
      return 0;
    }
    if( strcmp(argv[1], "-h")==0 || strcmp(argv[1], "--help")==0 ) {
      printf(HELP, APPNAME);      
      return 0;
    }
  }
  
  //XML cfg("a.conf");
  //Dev k(cfg);
  //k.pass();
  //k.check_file();
  //return 0;
  
  if(FILTER && CONFFILE) {
    XML cfg(CONFFILE);
		Dev k(cfg);
		k.force(FILTER);    
    return 0;
  }      
  
  Dem d(APPNAME, CONFFILE, PIDFILE);
  d.start();
  return 0;
}

