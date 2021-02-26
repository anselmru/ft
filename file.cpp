/***********************************************************************************
                         anselm.ru [2020-08-04] GNU GPL
***********************************************************************************/
#include "file.h"
#include <fnmatch.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h> // unlink
#include <fstream>
#include <stdlib.h> // free

#define SUFFIX "%Y%m%d%H%M%S.xml"

const char * g_pattern = NULL;

int
select(const struct dirent *d) {
  return 0==fnmatch(g_pattern, d->d_name, 0);
}

bool
File::find() {
  struct dirent **namelist = NULL;
  g_pattern = d_pattern.c_str(); //TODO слабое место
  int n = scandir(d_dir.c_str(), &namelist, select, alphasort);
  if(n > 0) {
    d_file = namelist[n-1]->d_name;
    while(n--) free(namelist[n]);
    free(namelist);
    return true;
  }
  
  free(namelist);
  return false;
}

void
File::save(const std::string& xml) const {
  std::string format = d_dir + SUFFIX;
  
  time_t t = time(NULL);
  tm m;
  localtime_r( &t, &m );
  
  char buf[100];
  strftime(buf, 100, format.c_str(), &m);
  std::string file_name =  buf;
  
  std::ofstream out(file_name.c_str());
  out.write(xml.c_str(), xml.size());
  out.close();
}

std::string
File::read() const {
  std::string s;  
  if(d_file.empty()) return s;
  
  std::ifstream in( (d_dir+d_file).c_str() );
  getline(in, s, '\0'); //getline(f, s, (char)0);
  in.close();
  return s;
}

bool
File::rm() {
  if(d_file.empty()) return false;
  
  bool ret = unlink( path() )==0;
  if(ret) d_file = "";
  return ret;
}

std::string
File::next() {
  std::string s;
  
  if(!find()) return s;
  s = read();
  if(!rm()) s = ""; // привлекаем внимание к неуничтожимому файлу - отказываемся его читать

  return s;
}
