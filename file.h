/***********************************************************************************
                         anselm.ru [2015-03-23] GNU GPL
***********************************************************************************/
#pragma once
#include <string>

class File {
friend int select(const struct dirent *);
public:
  File(const char* dir, const char* pattern = "*"):d_dir(dir),d_pattern(pattern) {
    if(d_dir[d_dir.size()-1]!='/') d_dir.append("/");
  }
  
  bool rm();
  bool find();
  //const char* path() const { return d_file.empty() ? "" : (d_path=(d_dir+d_file)).c_str(); }
  const char* path() { d_path=d_dir+d_file; return d_file.empty() ? "" : d_path.c_str(); }
  void operator<<(const std::string& s) const { save(s); }  
private:
  std::string d_path; // вспомогательная - используется только в path()
  std::string d_dir;
  std::string d_file;
  std::string d_pattern;
  std::string read() const;
  std::string next();
  void save(const std::string&) const;
};
