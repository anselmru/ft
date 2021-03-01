/***********************************************************************************
*              Облегчённое создание, удаление, поиск и чтение файлов               *
*                       с шаблонными именами в нужной папке.                       *
*                                                          anselm.ru [2021-03-01]  *
***********************************************************************************/
#include "file.h"
#include <fnmatch.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h> // unlink
#include <fstream>
#include <stdlib.h> // free


extern const char *__progname;

using namespace std;


const char*  File::PREFIX       = getenv("daemon") ? getenv("daemon") : __progname;
const char*  File::SUFFIX_FIND  = "*.xml";                 // при поиске более общий шаблон
const char*  File::SUFFIX_SAVE  = "%Y%m%d%H%M%S.xml";      // при записи более частный шаблон
const string File::PATTERN_FIND = string(File::PREFIX).append("-").append(File::SUFFIX_FIND).c_str();

File::File(const char* dir)
:d_dir(dir) {
  if(d_dir[d_dir.size()-1]!='/') d_dir.append("/");
}

int
select(const struct dirent *d) {
  return 0==fnmatch(File::PATTERN_FIND.c_str(), d->d_name, 0);
}

bool
File::find() {
  struct dirent **namelist = NULL;
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
File::save(const string& xml) const {
  string format = string(d_dir).append(PREFIX).append("-").append(SUFFIX_SAVE);;
  
  time_t t = time(NULL);
  tm m;
  localtime_r( &t, &m );
  
  char buf[100];
  strftime(buf, 100, format.c_str(), &m);
  string file_name =  buf;
  
  ofstream out(file_name.c_str());
  out.write(xml.c_str(), xml.size());
  out.close();
}

string
File::read() const {
  string s;  
  if(d_file.empty()) return s;
  
  ifstream in( (d_dir+d_file).c_str() );
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

string
File::next() {
  string s;
  
  if(!find()) return s;
  s = read();
  if(!rm()) s = ""; // привлекаем внимание к неуничтожимому файлу - отказываемся его читать

  return s;
}
