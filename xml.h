/***********************************************************************************
                         anselm.ru [2020-12-16] GNU GPLv3
***********************************************************************************/
#pragma once
#include <map>
#include "libxml/xpath.h"
#include <string>
#include <string.h> //strcmp strdup
#include "date.h" // можно изъять - и тогда исчезнет метод to_date

using namespace std;

class Node: public multimap<string, Node> {
public:
  typedef pair<Node::iterator, Node::iterator> Pair;
  typedef pair<Node::const_iterator, Node::const_iterator> CPair;
  Node(const string& s = ""):d_node(NULL),d_attr(NULL),d_value(s) {}
  virtual ~Node() {}
  //Node& operator->() { return *this; }
  //Node& operator=(xmlNodePtr xnod);
  Node& assign(xmlNodePtr xnod);
  Node& operator=(const string& s);
  Node& operator=(int i)           { char buf[30]={0}; sprintf(buf, "%d", i); d_value=buf; return *this; }
  Node& operator=(float f)         { char buf[30]={0}; sprintf(buf, "%f", f); d_value=buf; return *this; }
  bool operator==(string s)        { return d_value==s; }
  bool operator==(int a)           { return atoi(d_value.c_str())==a; }
  bool operator!=(int a)           { return atoi(d_value.c_str())!=a; }
  
  operator string()          const { return d_value; }
  //operator const char*() const { return d_value.c_str(); }
  size_type erase(int i)           { char buf[30]={0}; sprintf(buf, "%d", i); return multimap<string, Node>::erase(buf); }
  size_type erase(const string& s) { return multimap<string, Node>::erase(s); }

  const Node& operator[](const string& key) const;
  Node& operator[](const string& key);
  Node& operator[](int i) { char buf[30]={0}; sprintf(buf, "%d", i); return (*this)[buf]; }
  void dump(int = 0) const;
  Node index(const char*);

  string         to_string(const string& defaults = "")    const { return d_value.empty() ? defaults : d_value; }
  const char*    c_str    (const char*   defaults = "")    const { return d_value.empty() ? defaults : d_value.c_str(); }
  const char*    strdup   (const char*   defaults = "")    const {
    if(c_str(defaults)!=NULL) return ::strdup(c_str(defaults));
    else return NULL; 
  }
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
  string   to_xml_without_attr() const;
  string   to_xml_with_attr()    const;
  string   d_value;
  wchar_t* d_wchar;
  xmlNodePtr d_node;
  xmlAttrPtr d_attr;
};

class XML: public Node {
public:
  XML(const char* FILENAME, const char* XPATH = NULL);
  XML(const char* BUFFER, int size, const char* XPATH = NULL);
  XML(const string& BUFFER, const char* XPATH = NULL);
  virtual ~XML();
  void dumpXML() const { xmlDocDump(stdout, d_doc); }
  Node xpath(const char*) const;
  static bool exist(const char*);
  bool valid() const { return d_doc!=NULL; }
  void save() const;
private:
  xmlDocPtr d_doc;
  xmlXPathContextPtr d_ctx;  
  string d_name;
  void init() const;
  void init(const char* XPATH);
  void set();
};
