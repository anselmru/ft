/***********************************************************************************
*     Класс-обёртка над классом FT, скрывающая все особеннности протокола FT1.2.   *
*            По сути, здесь реализовано только XML-программирование,               *
*                       где Dev - обобщённый прибор.                               *
*                                                         anselm.ru [2021-02-26]   *
***********************************************************************************/
#include "dev.h"
#include "log.h"
#include "date.h"
#include "tcp.h"
#include "file.h"
//#include <sys/types.h>
#include <sys/stat.h>

#define DB_PATTERN "*.xml"

Dev::Dev(Node cfg)
: FT(cfg["dev"]["sock"])
, d_timeout_recv(cfg["timeout"]["recv"].to_long(1000)) {
  // Подготовим список запрашиваемых параметров
  int iid=0;
  Node::Pair ret = cfg.equal_range("data");
  Node::iterator I=ret.first;
  for(; I!=ret.second; ++I) {
    Node& data = I->second;
    
    Node::Pair ret2 = data.equal_range("param");
    Node::iterator J=ret2.first;
    for(; J!=ret2.second; ++J) {
      Node& param = J->second;
      Node::iterator K = data.begin(); // перебираем все свойства ёмкости data
      for(; K!=data.end(); ++K) {
        const string key = K->first;
        if(key.compare("comment")!=0 && param[key].to_string().empty() && !K->second.to_string().empty()) {
          param[key] = K->second; // наследуем свойство родителя
        }
      }
      d_params[ ++iid ] = param;
      //debug1("INIT: id=%d fun=%d", param["id"].to_int(), param["fun"].to_int());
      //d_params[ param["iid"] ] = param;
    }
  }
  
  if(d_params.empty()) {
    warning("empty section param in conf-file");
    throw("empty section param in conf-file");
  }
  
  // Теперь поднотовим список хостов для рассылки готовых данных
  ret = cfg.equal_range("store");
  I=ret.first;
  for(; I!=ret.second; ++I) {
    Node& data = I->second;
    
    Node::Pair ret2 = data.equal_range("socket");
    Node::iterator J=ret2.first;
    for(; J!=ret2.second; ++J) {
      Node& sock = J->second;
      Node::iterator K = data.begin(); // перебираем все свойства ёмкости data
      for(; K!=data.end(); ++K) {
        const string key = K->first;
        if(key.compare("comment")!=0 && sock[key].to_string().empty() && !K->second.to_string().empty()) {
          sock[key] = K->second; // наследуем свойство родителя
        }
      }
      string iid = sock["host"].to_string()+":"+sock["port"].to_string();
      d_stores[ iid ] = sock;
    }
  }
  
  if(d_stores.empty()) {
    warning("empty section store in conf-file");
    throw("empty section store in conf-file");
  }
}

bool
Dev::send(int iid) {
  bool ret = false;
  Node &param   = d_params[iid]; //param.dump();
  string table  = param["table"].to_string();
  word fun      = param["fun"].to_int();
  word reg      = param["reg"].to_int();  
  byte a1       = param["a1"].to_int();
  byte a2       = param["a2"].to_int();
  Date dt       = param["dt"].c_str();  // заказанное время
  
  //warning("Dev: id=%d fun=%d reg=%04X", param["id"].to_int(), fun, reg);
  
  if(fun==22) { // получить мгновенные значения
    ret = send22(iid, reg, a1, a2);
  }
  else if(fun==23) { // получить архивные значения
  
    if(table.find("day")!=string::npos) { // суточный архив
      
      word ii = Dev::index_day(dt);      // вручную считаем точное время, которое есть в архиве прибора
      param["dt"] = dt.c_str(); // сразу внесём искомую дату
      
      if(!param["default"].blank()) {   // может статься, что прибор в ремонте - и это нормально  
        param["value"] = param["default"].to_string();
        ret = true;
      }
      else
        ret = send23(iid, reg, a1, a2, ii, 1);

    } 
    else if(table.find("hour")!=string::npos) {
      
      word ii =Dev::index_hour(dt);  // вручную считаем точное время, которое есть в архиве прибора
      param["dt"] = dt.c_str();       // сразу внесём искомую дату
      
      if(!param["default"].blank()) {
        param["value"] = param["default"].to_string();
        ret = true;
      }
      else
        ret = send23(iid, reg, a1, a2, ii, 1);
    }
    else {
      error("bad table %s", table.c_str());
    }
    
  }  
  else {
    error("bad command %d", fun);
  }
  
  usleep(d_timeout_recv*1000); // надо немножко подождать между запросами
  return ret;
}

void
Dev::pass() { // один проход-запрос по всем параметрам с проверкой крон-даты
  Date now = Date::now();
  
  // Цикл запросов
  Node::iterator I = d_params.begin();
  for(; I!=d_params.end(); I++) {      
    ushort iid  = atoi(I->first.c_str());     // ид транзакции (по определению не может равняться нулю)
    Node &param = I->second;
    if(param["id"].to_int()<=0) continue;
    
    //if(param["fun"].to_int()==22 || param["fun"].to_int()==23) { // если мгновенные
    if(param["fun"].to_int()==22) { // если мгновенные
      send(iid);
    }
    else if(!param["cron"].blank() ) {
      if(param["cron_next"].blank()) { // если следующая дата ещё не вычислена
        param["cron_next"] = now.cron_next(param["cron"]).c_str();
        warning1("Dev::pass: iid=%d id=%d cron=%s cron_next=%s", iid, param["id"].to_int(), param["cron"].c_str(), param["cron_next"].c_str());
      }

      Date dt = param["cron_next"].c_str();
      if(now >= dt) { // если прибор выключен - заполним значениями по умолчанию без запросов
        param["dt"] = param["cron_next"].to_string(); // заказываем время - оно совпадает с крон-временем
        send(iid);
        param["cron_next"] = now.cron_next(param["cron"]).c_str(); // вычислим следующее время опроса параметра
      }
    }//if
  }//for
  
  // Нужно сначала завершить цикл запросов, и только потом приступать к циклу ответов.
  // Цикл запросов отделён от цикла ответов, так как запросы делаются беспорядочно.
  usleep(d_timeout_recv*1000); //подождём немножко, чтобы все ответы вернулись
  //warning1("timeout.recv=%ld", d_timeout_recv);
  
  //d_params[3]["value"] = "735.1";
  //d_params[3]["dt"]    = "2015-01-16 09:00:00";
  
  parse(); // сканируем ответы
}

void
Dev::parse() {
  struct stat buf = {0};
  bool has_stdout = 0==fstat(1, &buf); // имеется ли стандартный выход, т.е. запускаем ли вручную
  
  // Цикл ответов
  string good, bad; // сюда будем сохранять ответы - успешные и неудачные соответственно
  Node::iterator I = d_params.begin();
  for(; I!=d_params.end(); I++) {      
    ushort iid  = atoi(I->first.c_str());     // ид транзакции (по определению не может равняться нулю)
    Node &param = I->second;
    if(param["id"].to_int()<=0) continue;

    warning1("Dev::parse: iid=%d id=%d fun=%d dt='%s' value=%lf", iid, param["id"].to_int(), param["fun"].to_int(), param["dt"].c_str(), param["value"].to_double());
        
    if(!param["dt"].blank() && !param["value"].blank()) { // если успех, то сохраняем в базу данных
      good += "\t<save id=\""  + param["id"].to_string()
					 + "\"\tvalue=\""    + param["value"].to_string()
					 + "\"\tdt=\""       + param["dt"].to_string()
					 + "\"\ttable=\""    + param["table"].to_string()
					 + "\" />\n";
    }
    
    /* Для имитации bad и проверки работоспособности check_file:
    if(param["fun"].to_int()==23) {
    	param["dt"]    = "2021-02-25";
    	param["value"] = "";
    }*/
    
    if(param["fun"].to_int()==23 && !param["dt"].blank() && param["value"].blank()) { // если неудача, то сохраняем в файл
      bad += "  <param ";
      Node::iterator K = param.begin(); // перебираем все свойства
      for(; K!=param.end(); ++K) {
        const string name  = K->first;
        const string value = K->second;
        if(name.compare("comment")!=0 && !value.empty()) {
          //bad += "\t" + name + "=\"" + value + "\"";
          bad += name + "=\"" + value + "\" ";
        }
      }      
      bad += "/>\n";
    }
    
    // Чистка врЕменных атрибутов, пришедших от приборов
    param["dt"]     = "";
    param["value"]  = "";
  }//for
  
  
  if(!good.empty()) { // сохраним в базу
    good = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE qscada>\n<root>\n" + good + "</root>\n";
    store(good);
    if(has_stdout) {
			printf("--------good--------\n");
			printf("%s\n", good.c_str());
    }
  }
  
  if(!bad.empty()) {  // сохраним в файл
    bad = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE qscada>\n<root>\n" + bad + "</root>\n";
    File(DB_DIR) << bad; // в любом режиме
    if(has_stdout) {  // в ручном режиме
			printf("--------bad---------\n");
			printf("%s\n", bad.c_str());
    }
    else {
    	//File(DB_DIR) << bad; // только в демоническом режиме
    }
  }
}

void
Dev::read(word iid, float val) {
  Node& param  = d_params[iid];
  param["value"] = val;
  if(param["fun"].to_int()==22) param["dt"] = Date::now().c_str();  
  warning1("Dev::read: table=%s iid=%d reg=0x%X dt=%s value=%f", param["table"].c_str(), iid, param["reg"].to_int(), param["dt"].c_str(), val);
  //const char* dt =strdup(param["dt"].c_str());
  //warning1("Dev::read: table=%s iid=%d reg=0x%X dt=%s value=%f", param["table"].c_str(), iid, param["reg"].to_int(), dt, val);
}

void // принудительный запрос по всем параметрам, у которых стоит атрибут dt, либо если аргумент a!=NULL
Dev::force(const char* filter) { 
  // Цикл запросов
  Node::iterator I = d_params.begin();
  for(; I!=d_params.end(); I++) {      
    ushort iid  = atoi(I->first.c_str());   // ид транзакции (по определению не может равняться нулю)
    Node &param = I->second;
    if(param["id"].to_int()<=0) continue;    
    if(param["fun"].to_int()==22) continue; // не трогаем мгновенные
    
    if(filter!=NULL) {
      char period[5] = {0};
      char dt[40] = {0};
      if(2==sscanf(filter, "%[^=]=%[0-9 :-]", period, dt)) {
        string table = param["table"].to_string();
        if(table.find(period)!=string::npos) {
          param["dt"] = dt;
        }
      }
      //warning1("filter=%s period=%s dt=%s", filter, period, dt);
    }
    
    if(param["dt"].blank()) continue;       // исследуем только параметры с заказанным временнем dt      
    // заказанным временем является время dt, взятое из конфига, либо из аргумента a
    send(iid);
    warning1("FORCE SEND! iid=%d id=%d dt='%s' table=%s", iid, param["id"].to_int(), param["dt"].c_str(), param["table"].c_str());
  }//for
  
  // Нужно сначала завершить цикл запросов, и только потом приступать к циклу ответов.
  // Цикл запросов отделён от цикла ответов, так как запросы делаются беспорядочно.
  usleep(d_timeout_recv*1000); //подождём немножко, чтобы все ответы вернулись
  warning1("timeout.recv=%ld", d_timeout_recv);
  
  //d_params[3]["value"] = "735.1";
  //d_params[3]["dt"]    = "2015-01-16 09:00:00";
  
  parse(); // сканируем ответы
}

void
Dev::check_file() {
  File f(DB_DIR, DB_PATTERN);
  
  if(!f.find()) return;
  
  XML xml(f.path());
  Node params = xml.xpath("/root/param");
  warning("info: find file %s\n", f.path());
  f.rm();
  if(params.empty()) return; // значит, какой-то некондиционный xml
  
  Node bak = d_params; // запомним все параметры
  d_params.clear();
  int iid=0;
  Node::Pair ret2 = params.equal_range("param");
  Node::iterator J=ret2.first;
  for(; J!=ret2.second; ++J) {
    Node& param = J->second;
    d_params[ ++iid ] = param;
  }
  //d_params.dump();
  force();
  d_params.clear();
  d_params = bak;
}

bool
Dev::store(const string& xml) {
  //std::ofstream out("/home/max/src/bo/2.xml");
  //out.write(xml.data(), xml.size());
  //out.close();
  //debug("STORE");
  bool ret = true;
  
  Node::iterator I = d_stores.begin();
  for(; I!=d_stores.end(); ++I) {
    TCP *tcp = new TCP;
    Node& sock = I->second;
    tcp->cnntimeo(sock["cnntimeo"].to_int());
    tcp->sndtimeo(sock["sndtimeo"].to_int());
    tcp->rcvtimeo(sock["rcvtimeo"].to_int());
    //tcp->rcvbuf(p["rcvbuf"].to_int());
    //debug3("Dev::store: %s", xml.c_str());
    if( !tcp->connect(sock["host"].c_str(), sock["port"].to_int()) || !tcp->send(xml.c_str(), xml.size()) ) { //отправка на сервер
      ret = false; // если хотя бы одна неудачная отсылка, то фу-ю считаем неудачной
      error("send to %s:%d", sock["host"].c_str(), sock["port"].to_int());
    }
    delete tcp;
  }
  
  return ret;
}
