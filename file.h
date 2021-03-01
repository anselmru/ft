/***********************************************************************************
*              Облегчённое создание, удаление, поиск и чтение файлов               *
*                       с шаблонными именами в нужной папке.                       *
*                                                          anselm.ru [2021-03-01]  *
***********************************************************************************/
#pragma once
#include <string>

using namespace std;

class File {
friend int select(const struct dirent *);
public:
  File(const char* dir);
    
  bool rm();
  bool find();
  const char* path() { return d_file.empty() ? "" : string(d_dir+d_file).c_str(); }
  void operator<<(const string& s) const { save(s); }
private:
  static const string PATTERN_FIND;  
  static const char*  PREFIX;
  static const char*  SUFFIX_FIND;
  static const char*  SUFFIX_SAVE;

  string d_dir;
  string d_file;
  string read() const;
  string next();
  void save(const string&) const;
};
