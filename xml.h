/***********************************************************************************
                         anselm.ru [2017-10-17] GNU GPLv3
***********************************************************************************/
#pragma once
#include <map>
#include "libxml/xpath.h"
#include <string>
#include <string.h> //strcmp strdup
//#include "date.h" // можно изъять - и тогда исчезнет метод to_date

using namespace std;

class Node: public multimap<string, Node> {
public:
  typedef pair<Node::iterator, Node::iterator> Pair;
  typedef pair<Node::const_iterator, Node::const_iterator> CPair;
  virtual ~Node() {}
  //Node& operator->() { return *this; }
  Node& operator=(const string& s) { d_value=s; return *this; }
  Node& operator=(int i)           { char buf[30]={0}; sprintf(buf, "%d", i); d_value=buf; return *this; }
  Node& operator=(float f)         { char buf[30]={0}; sprintf(buf, "%f", f); d_value=buf; return *this; }
  operator string()          const { return d_value; }
  //operator const char*() const { return d_value.c_str(); }
  size_type erase(int i)           { char buf[30]={0}; sprintf(buf, "%d", i); return multimap<string, Node>::erase(buf); }
  size_type erase(const string& s) { return multimap<string, Node>::erase(s); }

  Node& append(const string& key, const string& val = ""); //emplace  
  const Node& operator[](const string& key) const;
  Node& operator[](const string& key);
  Node& operator[](int i) { char buf[30]={0}; sprintf(buf, "%d", i); return (*this)[buf]; }
  void dump(int = 0) const;
  Node index(const char*);

  string         to_string(const string& defaults = "")    const { return d_value.empty() ? defaults : d_value; }
  const char*    c_str    (const char*   defaults = "")    const { return d_value.empty() ? defaults : d_value.c_str(); }
  char*          strdup   (const char*   defaults = "")    const { return ::strdup(c_str(defaults)); }
  int            to_int   (const int     defaults = 0 )    const { return to_long(defaults); }
  long           to_long  (const long    defaults = 0 )    const { 
    if(d_value.empty()) return defaults;
    size_t i = d_value.find("0x");
    return (i!=string::npos) ? strtol(d_value.c_str()+i+2, NULL, 16 ) : strtol(d_value.c_str(), NULL, 10 );
  }
  float          to_float (const float   defaults = 0.0)   const { return d_value.empty() ? defaults : strtof(d_value.c_str(), NULL); }
  double         to_double(const double  defaults = 0.0)   const { return d_value.empty() ? defaults : strtod(d_value.c_str(), NULL); }
  bool           to_bool  (const bool    defaults = false) const { return d_value.empty() ? defaults : (d_value==string("true") || d_value==string("yes") || d_value==string("on") || d_value==string("1")); }
#ifdef DATE_H
  Date           to_date() { return Date(d_value); }
#endif
  string         to_xml_str(bool with_attr = true) const;
  string         row()          const;
  string         attr()         const;
  bool           blank()        const { return d_value.empty(); }
  bool           has_value()    const { return !d_value.empty(); }  
  bool           has_attr()     const { return size()>0; }
  bool           has_children() const;
  
  void           clear()        { d_value.clear(); multimap<string, Node>::clear(); }
  const wchar_t* to_utf16(const char* defaults = "");
  static const Node& NONE;
private:
  string to_xml_without_attr() const;
  string to_xml_with_attr()    const;
  string d_value;
  wchar_t* d_wchar;
};

class XML: public Node {
public:
  XML(const char* FILENAME, const char* XPATH = NULL);
  XML(const char* BUFFER, int size, const char* XPATH = NULL);
  XML(const string& BUFFER, const char* XPATH = NULL);
  virtual ~XML();
  void dumpXML() const { xmlDocDump(stdout, doc); }
  Node xpath(const char*) const;
  static bool exist(const char*);
  bool valid() const { return doc!=NULL; }
private:
  xmlDocPtr doc;
  xmlXPathContextPtr xpathCtx;  
  void init(const char* XPATH);
  void fill(xmlNode*, Node&) const;
};
