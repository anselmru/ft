/***********************************************************************************
                         anselm.ru [2017-10-17] GNU GPLv3
***********************************************************************************/
#include "xml.h"
//#include <iostream>
#include <sys/stat.h>
#include <libxml/tree.h> //http://www.xmlsoft.org/examples/xpath2.c
#include <libxml/parser.h>
#include <libxml/xpathInternals.h>

#ifndef LOG_H
#define warning printf
#endif

string
trim(string s) {
  const char* t = (char*)" \t\n\r\f";
  s.erase(0, s.find_first_not_of(t));
  s.erase(s.find_last_not_of(t)+1);
  return s;
}

/************************************************************************************

                                         Node
                                 
************************************************************************************/

const Node&
Node::NONE = Node();

Node&
Node::append(const string& key, const string& val) {//emplace
  Node n;
  n.d_value = trim(val);
  iterator I = multimap<string, Node>::insert( pair<string, Node>(trim(key), n) );
  return I->second;
}


const Node&
Node::operator[](const string& key) const {
  const_iterator I = find(key);
  if(I!=end())
    return I->second;
  else
    return NONE;
}

Node&
Node::operator[](const string& key) {
  iterator I = find(key);
  if(I!=end())
    return I->second;
  else
    return append(key);
}
  
void
Node::dump(int level) const {
  //if(level==0) warning("---------------------\n");
  Node::const_iterator I = begin(); 
  for(; I!=end(); I++) {
    for(int i=0; i<level; i++) warning(" ");
    warning("[%s=%s]\n", I->first.c_str(), I->second.c_str());
    I->second.dump(level+1);
  }
  //if(level==0) warning("---------------------\n");
}

Node
Node::index(const char* id) {
  Node x;
  
  Node::iterator I = begin();
  for(; I!=end(); I++) {
    string val = I->second[id];
    if(!val.empty()) x[val] = I->second;
  }
  
  return x;
}

unsigned wchar_t makeword(char a, char b) {
  return (unsigned wchar_t)((a&0xff)<<8)+(unsigned wchar_t)(b&0xff);
}

const wchar_t*
Node::to_utf16(const char* defaults) {
  const char* a = empty() ? defaults : c_str();      
  const size_t n = strlen(a);   
  if(d_wchar) delete [] d_wchar;
  d_wchar = new wchar_t[n+1];
  memset(d_wchar, 0, (n+1) * sizeof(wchar_t));
  
  int j = 0;
  for(int i=0; i<n; i++) {
    if(a[i] & 0x80) { // если старший бит не ноль
      d_wchar[j]   = makeword(a[i], a[i+1]);
      if(a[i] & 0x01) // если младший бит ноль
        d_wchar[j] -= 0xCD40;
      else
        d_wchar[j] -= 0xCC80;
      i++;
    }
    else {
      d_wchar[j]   = makeword(0x00, a[i]);
    }
    j++;
  }
  return d_wchar;
}


string
Node::row() const {
  string s;
  const_iterator I = begin();
  for(; I!=end(); I++)
    s += I->first + "=" + I->second.d_value + " ";
  return s;
}

string
Node::attr() const { 
  // Атрибут - это узел без наследников
  // Повторяющиеся атрибуты параметра запрещены, но в мультикарте они запросто могут возникнуть, поэтому очистим строку атрибутов от дублей 
  map<string, string> m; // уникальная карта
  
  const_iterator I = begin();
  for(; I!=end(); I++)
    //if(I->second.has_value() && !I->second.has_children()) s += "\"" + I->first + "\"=\"" + I->second.d_value + "\" "; // хотя так тоже можно, но тогда в атрибут попадёт и узел с наследником
    //if(I->second.has_value() && I->second.size()==0) s += "\"" + I->first + "\"=\"" + I->second.d_value + "\" ";
    //if(I->second.has_value() && I->second.size()==0) s += I->first + "=\"" + I->second.d_value + "\" ";
    if(I->second.has_value() && I->second.size()==0) m[I->first] = I->second.d_value;
    
  string s;
  map<string, string>::const_iterator J = m.begin();
  for(; J!=m.end(); J++)
    s += J->first + "=\"" + J->second + "\" ";
    
  return s;
}


bool
Node::has_children() const {
  const_iterator I = begin();
  for(; I!=end(); I++)
    if(I->second.size()) return true;
    
  return false;
}

string
Node::to_xml_str(bool with_attr) const {
  return with_attr ? to_xml_with_attr() : to_xml_without_attr();
}

string
Node::to_xml_with_attr() const {
  string s = "";
  Node::const_iterator I = begin();
  for(; I!=end(); I++) {
    if(I->second.size()) {
      if(I->second.has_value() || I->second.has_children()) {
        s += "<" + I->first + " " + I->second.attr() + ">\n";
        if(I->second.has_value()) s += I->second.to_string() + "\n";
        if(I->second.has_children()) s += I->second.to_xml_with_attr();
        s += "</" + I->first + ">\n";
      }
      else
        s += "<" + I->first + " " + I->second.attr() + "/>\n";
    }
    //else s += "<" + I->first + ">" + I->second.to_string() + "</" + I->first + ">\n";
  }//for  
  return s;
}

string
Node::to_xml_without_attr() const {
  string s = "";
  Node::const_iterator I = begin();
  for(; I!=end(); I++) {
    s += "<" + I->first + ">";
    if(I->second.size()) s += "\n";
    s += I->second.to_string();
    if(I->second.size() && I->second.has_value()) s += "\n";
    s += I->second.to_xml_without_attr();
    s += "</" + I->first + ">\n";
  }//for
  return s;
}


/************************************************************************************

                                         XML
                                 
************************************************************************************/

//void xml_GenericErrorFunc(void * ctx, const char * msg, ...) {}
//void xml_StructuredErrorFunc(void * userData, xmlErrorPtr error) { //xmlResetError(error);}

XML::XML(const char* FILENAME, const char* XPATH)//: Node()
: doc(NULL)
, xpathCtx(NULL) {
  struct stat st;
  if(stat(FILENAME, &st) == -1) {
    warning("Error: file not exist \"%s\"\n", FILENAME);
    return;
  }
  
  xmlInitParser();
  //xmlSetGenericErrorFunc(NULL,  xml_GenericErrorFunc);
  //xmlSetStructuredErrorFunc(NULL, xml_StructuredErrorFunc);
  //LIBXML_TEST_VERSION
  
  doc = xmlParseFile(FILENAME); // Load XML document
  if(doc == NULL) {
    warning("Error: unable to parse file \"%s\"\n", FILENAME);
    return;
  }

  init(XPATH);
}
         
XML::XML(const char* BUFFER, int size, const char* XPATH)
: doc(NULL)
, xpathCtx(NULL) {
  if(size <= 0) {
    warning("Error: empty string\n");
    return;
  }
  
  xmlInitParser();
  //xmlSetGenericErrorFunc(NULL,  xml_GenericErrorFunc);
  //xmlSetStructuredErrorFunc(NULL, xml_StructuredErrorFunc);
  //LIBXML_TEST_VERSION
    
  doc = xmlParseMemory(BUFFER, size); // Load XML document
  if(doc == NULL) {
    warning("error: unable to parse string \"%s\"\n", BUFFER);
    return;
  }

  init(XPATH);
}

XML::XML(const string& BUFFER, const char* XPATH)
: doc(NULL)
, xpathCtx(NULL) {
  if(BUFFER.size() == 0) {
    warning("Error: empty string\n");
    return;
  }
  
  xmlInitParser();
  //xmlSetGenericErrorFunc(NULL,  xml_GenericErrorFunc);
  //xmlSetStructuredErrorFunc(NULL, xml_StructuredErrorFunc);
  //LIBXML_TEST_VERSION

  doc = xmlParseMemory(BUFFER.c_str(), BUFFER.size()); // Load XML document
  if(doc == NULL) {
    warning("Error: unable to parse string \"%s\"\n", BUFFER.c_str());
    return;
  }

  init(XPATH);
}

void
XML::init(const char* XPATH) {
  // Create xpath evaluation context
  xpathCtx = xmlXPathNewContext(doc);
  if(xpathCtx == NULL) {
    warning("Error: unable to create new XPath context\n");
    return;
  }
  
  if(XPATH==NULL) {// получим корневой элемент
    xmlNode *root = xmlDocGetRootElement(doc);
    string s  = (const char*)root->name;//"/";
    s = "/" + s + "/*";
    *((Node*)this) = xpath(s.c_str());
  }
  else {
    *((Node*)this) = xpath(XPATH);  
  }
}

XML::~XML() {
  if(xpathCtx) xmlXPathFreeContext(xpathCtx);
  if(doc) {
    xmlFreeDoc(doc);
    xmlCleanupParser();
  }
}

bool
XML::exist(const char* FILENAME) {
  struct stat st;
  return stat(FILENAME, &st) != -1;
}

//xmlChar * xmlXPathCastToString  (xmlXPathObjectPtr val)

Node
XML::xpath(const char* XPATH) const {
  Node mm;
  const xmlChar * xpathExpr = BAD_CAST XPATH;
  
  // Evaluate xpath expression
  xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
  if(xpathObj == NULL) {
    warning("Error: unable to evaluate xpath expression \"%s\"\n", xpathExpr);
    return mm;
  }

  xmlNodeSetPtr nodes = xpathObj->nodesetval;
  int size = (nodes) ? nodes->nodeNr : 0;
    
  for(int i = 0; i < size; i++) {
    xmlNode *node = nodes->nodeTab[i];
    //warning("name->%s\n", node->name);
    fill(node, mm);    
    if (node->type != XML_NAMESPACE_DECL) node = NULL;//??
  }
  
  xmlXPathFreeObject(xpathObj);
  
  return mm;
}

void
XML::fill(xmlNode *node, Node &mp) const {  
  xmlChar *content = xmlNodeGetContent(node);
  Node &mc = xmlChildElementCount(node)==0 ? mp.append((char*)node->name, (char*)content) /*TODO trim*/: mp.append((char*)node->name);
  xmlFree(content);
  
  xmlNode* cur = node->xmlChildrenNode;
  while(cur) { // пробежимся по потомкам
    char *name = (char*) cur->name;
    
    if(strcmp(name, "text")!=0) { // игнорируем это поле text - оно системное для библиотеки libxml2
      //xmlChar* path = xmlGetNodePath(cur); // x-путь узла
      fill(cur, mc);
    }
    cur = cur->next;
  }
    
  xmlAttr *attr = node->properties;
  while( attr ) { // пробежимся по атрибутам
    mc.append((char*)attr->name, (char*)attr->children->content);//TODO trim
    attr = attr->next;
  }
}
