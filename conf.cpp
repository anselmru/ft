/**********************************************************
 **      Class for Config-file using stl and boost.      **
 **                 anselm.ru [2012-02-09]               **
 **********************************************************/
 
#include "conf.h"
#include <fstream>
#include <iostream>
#include <boost/regex.hpp>

#ifdef WIN32
#include <windows.h>    //GetModuleFileNameA
#include <shlwapi.h>    //PathFindFileNameA
#else
#define MAX_PATH 256
#include <libgen.h>     //basename
#include <dlfcn.h>
#endif


using namespace std;
using namespace boost;

string trim(string& s)
{
  const char* t = (char*)" \t\n\r\f";
  s.erase(0, s.find_first_not_of(t));
  s.erase(s.find_last_not_of(t)+1);
  return s;
}

Conf::Conf(const char *name, const char *rem) :
  d_size(0)
{
  d_name = new char[MAX_PATH];
  bzero(d_name, MAX_PATH);
  if(rem) d_rem = strdup(rem);
  guess(name);
}

Conf::~Conf() 
{
  MM::iterator i = d_conf.begin();
  for(; i!=d_conf.end(); i++) {
    delete i->second;
  }
  delete [] d_name;
}



bool Conf::module_name(char* a)
{
  Dl_info info;
  bool cool = dladdr((void*)module_name, &info);
  if(cool) {
    strcpy(a, info.dli_fname);
  }
  
  return cool;
}


bool Conf::open(const char* name)
{
  ifstream f(name, ios::binary);
  d_exist = f.good();
  if(!d_exist) {    
    f.close();
    return false;
  }
  
  const char *T1 = "\\[(.*)\\]\\s*([%s].*|$)";
  const char *T2 = "([^%s]*)=([^%s]*)\\s*([%s].*|$)";
  const char *T3 = "([^%s]+)\\s*([%s].*|$)";
  
  char t1[50] = {0};
  char t2[50] = {0};
  char t3[50] = {0};
  
  sprintf(t1, T1, d_rem);
  sprintf(t2, T2, d_rem, d_rem, d_rem);
  sprintf(t3, T3, d_rem, d_rem);
  
  //regex r1("\\[(.*)\\]\\s*(#.*|$)", regex::perl);
  //regex r2("([^#]*)=([^#]*)\\s*(#.*|$)", regex::perl);
  //regex r3("([^#;]+)\\s*([#;].*|$)", regex::perl);
  regex r1(t1, regex::perl);
  regex r2(t2, regex::perl);
  regex r3(t3, regex::perl);

  smatch p;
  string s;
  Section *m = NULL;
  
  while( !f.eof() ) {
    getline(f, s, '\n');
    
    if( regex_match(s, p, r1) ) {
       string section(p[1].first, p[1].second);
       m = new Section;
       d_conf[ trim(section) ] = m;
    }
    else if( m && regex_match(s, p, r2) ) {
       string key(p[1].first, p[1].second);
       string value(p[2].first, p[2].second);
       pair<string, string> e;
       e.first = trim(key);
       e.second = trim(value);
       m->insert(e);
    }
    else if( m && regex_match(s, p, r3) ) {
       string key(p[1].first, p[1].second);
       pair<string, string> e;
       e.first = trim(key);
       //e.second = trim(value);
       m->insert(e);
    }
  }
  
  f.close();
  
  return true;
}

bool Conf::guess(const char* name)
{
#ifdef WIN32
  GetModuleFileNameA(0, d_name, MAX_PATH);
  strlwr(d_name);
  if(name) {
    char *bname = PathFindFileNameA(name);
    char *last_slash = strrchr(d_name, '\\');
    *(++last_slash) = 0;
    strcat(d_name, bname);
  }
  else {
    char *e = strstr(d_name, ".exe");
    strcpy(e, ".conf");
  }
#else
  if(name) {
    char *bname = basename(name);
    if(strcmp(bname,name)!=0) {
      strcpy(d_name, name);
      return open(d_name);                           // find strongly
    }
    
    if( module_name(d_name) ) {
      char *last_slash = strrchr(d_name, '/');
      *(++last_slash) = 0;
      strcat(d_name, bname);
      if(open(d_name))                               // find in local
        return true;
      else
        sprintf(d_name, "/usr/local/etc/%s", bname); // find in etc
    }
    else {
      sprintf(d_name, "/usr/local/etc/%s", bname);   // find in etc
    }
  }
  
  else if( getenv("conffile")!=NULL )                    // if envirenment
    strcpy(d_name, getenv("conffile") );
  else {
    if(module_name(d_name)) {
      
      strcat(d_name, ".conf");
      if(open(d_name))                               // find in local
        return true;
      else {
        char *bname = basename(d_name);
        sprintf(d_name, "/usr/local/etc/%s", bname); // find in etc
      }
      
    }
    else
      return false;
  }
#endif

  return open(d_name);
}


const char* Conf::readString(const char* section, const char* key, const char* defaults)
{
  if(!d_conf[ section ]) return defaults;
  string s = (*d_conf[ section ])[ key ];
  return s.empty() ? defaults : s.c_str();
}

const Conf::Section* Conf::readSection(const char* section)
{
  return d_conf[ section ];
}

int Conf::readInteger(const char* section, const char* key, const int defaults)
{
  if(!d_conf[ section ]) return defaults;
  string s = (*d_conf[ section ])[ key ];
  return s.empty() ? defaults : atoi( s.c_str() );
}

double Conf::readDouble(const char* section, const char* key, const double defaults)
{
  if(!d_conf[ section ]) return defaults;
  string s = (*d_conf[ section ])[ key ];
  char **endptr;
  return s.empty() ? defaults : strtod(s.c_str(), endptr);
}

bool Conf::readBool(const char* section, const char* key, const bool defaults)
{
  if(!d_conf[ section ]) return defaults;
  string s = (*d_conf[ section ])[ key ];
  return s.empty() ? defaults : (s=="true" || s=="yes" || s=="on" || s=="1");
}
