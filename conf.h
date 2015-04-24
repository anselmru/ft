#pragma once
#include <string>
#include <map>

class Conf
{
public:
  Conf(const char* name = 0, const char* rem = "#;");
  virtual ~Conf();                                                             
  
  typedef std::map<std::string, std::string> Section;
  
  const char* readString (const char* section, const char* key, const char*  defaults = "" );
  int         readInteger(const char* section, const char* key, const int    defaults = 0  );
  double      readDouble (const char* section, const char* key, const double defaults = 0.0);
  bool        readBool   (const char* section, const char* key, const bool   defaults = false);
  
  const Section* readSection(const char* section);
  
  bool        open       (const char* = 0);
  bool        guess      (const char* = 0);
  bool        exist      () const { return d_exist; }
  const char* name       () const { return d_name; } 
  unsigned int size      () const { return d_size; }
  
  static bool module_name(char *);
private:
  bool d_exist;
  unsigned int d_size;
  typedef std::map<std::string, Section*> MM;
  char *d_name;
  char *d_rem;

  MM d_conf;
};
