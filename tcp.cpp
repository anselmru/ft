/*************************************************************************
*                        Класс для работы с TCP                          *
*                                               anselm.ru [2021-03-11]   *
**************************************************************************/
#include "tcp.h"
#include "log.h"

#include <sys/socket.h>   //socket
#include <netinet/in.h>   //sockaddr_in
#include <string.h>       //strdup
#include <arpa/inet.h>    //inet_addr
#include <unistd.h>       //close
#include <netdb.h>        //gethostbyname
#include <netinet/tcp.h>  //TCP_NODELAY
#include <signal.h>       //signal
#include <errno.h>        //errno
#include <fcntl.h>
#include <sys/poll.h>
#include <pthread.h>      //PTHREAD_CANCEL_ENABLE
#include <algorithm>      //find_if

#define TIMEOUT 500000    // перекур после хорошенького сбоя

void*
thread_event(void *a) {// ожидание события в дочернем потоке
  warning3("TCP::thread_event enter thread_event");
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);  // отложенный выход - установлен по умолчанию
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL); // отложенный выход - в ближайшей точке выхода
  TCP *tcp = (TCP*) a;
  
  pollfd *ufds = NULL;
  unsigned int nfds;
  
  while(1) {
    pthread_testcancel();
    //warning3("debug: while(1)");
    if(tcp->d_sock==-1) { // допустимый случай - например, когда произошло переподключение, или слушающий поток запустился раньше, чем произошло соединение
      //warning3("error: bad main socket");
      if(nfds) { 
        // Придётся приостановить обслуживание всех клиентов. Можно было поступить иначе и продолжать их обслуживать
        // при отрицательном главном сокете, но тогда не понятно, как при создании главного сокета заставить 
        // слушающий поток перечитать массив слушаемых сокетов ufds.
        // Важно: уничтожение массива ufds - это проистановка обслуживания, но не отключение клинтов. Отключение - это close.
        delete [] ufds;
        ufds = NULL;
      }
      usleep(TIMEOUT); // ожидаем, когда главный поток создаст сокет, чтобы предотвратить загрузку процессора
      continue;
    }
    
    if(ufds==NULL) {
      nfds = tcp->d_socks.size()+1;   // количество клиентов плюс главный сокет сервера
      warning3("TCP::thread_event rebuild clients list, new count %d", nfds);
      ufds = new pollfd[nfds];
      ufds[0].fd = tcp->d_sock;                     // под номером ноль будет находиться главный сокет сервера
      ufds[0].events = POLLIN | POLLHUP | POLLNVAL; // интересуют подключение новго клиента, уничтожение сокета
      ufds[0].revents &= 0x0000;   // необязательно
      
      int i=1;
      std::list<int>::iterator I = tcp->d_socks.begin();
      for(; I!=tcp->d_socks.end(); I++) {
        if(i>=nfds) { // Это возможно!! Здесь может возникнуть разница между количеством клиентов предыдущим и текущим,
          // что не недопустимо. Поэтому обязательно отправляем поток на пересоздание массива.
          warning("TCP::thread_event wrong client count %d", nfds);
          delete [] ufds;
          ufds = NULL;
          break;// покидаем цикл for...
        }
        ufds[i].fd = *I;             // все оставшиеся места занимают клиенты
        ufds[i].events = POLLIN | POLLHUP | POLLNVAL;
        ufds[i].revents &= 0x0000;   // необязательно
        i++;
      }//for
    }//if
    
    if(ufds==NULL) { // это возможно и нормально, т.к. предыдущий код может вернуть неинициализированный массив сокетов ufds
      warning3("TCP::thread_event ufds==NULL");
      continue;
    }
    
    warning3("TCP::thread_event poll");
    const int COUNT_EVENT = poll(ufds, nfds, -1);// количество случившихся событий  (poll является точкой выхода из потока по требованию)
    if( COUNT_EVENT == -1 ) { // серьёзная ошибка (поломка сокетов) - лучше закрыть все соединения, пусть все клиенты переподключаются
      error("TCP::thread_event poll: main socket and all client sockets will be closed");
      tcp->close_main_socket(); // здесь присваевается d_sock=-1
      tcp->close_all_clients();
      delete [] ufds;
      ufds = NULL;
      //usleep(TIMEOUT);// чтобы предотвратить загрузку процессора в случае неполадок с poll
      continue; // переделать список сокетов
    }//if
    
    warning3("TCP::thread_event COUNT_EVENT=%d sock[0].fd=%d sock[0].revents=0x%X", COUNT_EVENT, ufds[0].fd, ufds[0].revents);
    //!! COUNT_EVENT>0, т.к. случай COUNT_EVENT==0 исключён в силу того, что ожидаем неограниченно долго
    if(ufds[0].revents & POLLIN) { // что-то пришло на сервер
      int ret = tcp->recv(ufds[0].fd);  //
      //warning3("debug: event %d on server sock %d ret=%d", ufds[0].revents, ufds[0].fd, ret);
      if(ret==0) {// соединение было завершено собеседником
        //warning3("warning: interlocutor completed connection on main socket %d", ufds[0].fd);
        error3("TCP::thread_event recv on main socket %d", ufds[0].fd); //ECONNRESET 54 Connection reset by peer 
        ufds[0].revents |= POLLHUP;
      }
      else if(ret==-1) {// какая-то серьёзная ошибка - вероятно главный сокет уничтожен
        warning3("TCP::thread_event problem with connection on main socket %d", ufds[0].fd); 
        error("TCP::thread_event recv==-1");
        ufds[0].revents |= POLLHUP;
      }
    }
    
    if(ufds[0].revents & POLLHUP || ufds[0].revents & POLLNVAL ) {  // плохое событие на главном сокете   POLLHUP="положили трубку"
      //warning3("POLLIN & POLLNVAL & 0x%X", ufds[0].revents);
      error("TCP::thread_event main socket %d error 0x%X", ufds[0].fd, ufds[0].revents); //EBADF 9 Bad file descriptor
      //TODO tcp->close_main_socket(); // здесь присваевается d_sock=-1
      //TODO tcp->disconnect(); // сокет не уничтожаем, а просто разъединяем
      tcp->d_is_connect = false; // не нужно создавать здесь новых событий на сокете
      sleep(1);
    }
    
    for(int i=1; i<nfds; i++) { // вначале обработаем ВСЕ клиентские сокеты
      int sock      = ufds[i].fd;       // дочерний сокет
      short revents = ufds[i].revents;  // событие
      warning3("TCP::thread_event event %d on client sock %d", revents, sock);
      pthread_testcancel();
      
      if(revents & POLLIN) { // события чтения на сокетах        
        int ret = tcp->recv(sock); // что-то пришло от клиента
        if(ret==-1) { // серьёзная ошибка на сокете
          warning("TCP::thread_event on client socket %d", sock);
          revents = POLLHUP; // передаём следующему обработчику обрыва
        }
        else if( ret==0 ) { // это пришло правильное закрытие клиента
          warning3("TCP::thread_event good close on client socket %d", sock);
          revents = POLLHUP; // передаём следующему обработчику обрыва
        }
      }
      
      if(revents & POLLHUP || revents & POLLNVAL) { // обрыв связи с клиентом
        tcp->close_client(sock); // не будет тут прерывать цикл ради одного отключившегося клиента, чтобы остальные клиенты не пострадали
      }
    }//for
    
    
    
    warning3("TCP::thread_event count client=%d, listen all sock=%d", tcp->d_socks.size(), nfds);
    if(tcp->d_sock==-1 || tcp->d_socks.size()!=nfds-1) { // если в итоге предыдущего чтения клиентов отвалилось несколько соединений, то перестраиваем массив сокетов
      warning3("TCP::thread_event restruct array of sockets");
      delete [] ufds;
      ufds = NULL;
      continue;// переделать список сокетов
    }
  } // while
  
  warning3("TCP::thread_event exit thread");
  pthread_exit(NULL); // pthread_exit(&ret); ret - возвращаемый результат
}

TCP::TCP()
: d_sock(-1)
, d_is_connect(false)
, d_thread(NULL)
, d_timeout_conn(CNNTIMEO)
, d_timeout_send(0)
, d_timeout_recv(0)
, d_recv_buff_size(RCVBUF) { 
  
}

TCP::TCP(const Node& a)
: d_sock(-1)
, d_is_connect(false)
, d_thread(NULL)
, d_timeout_conn(CNNTIMEO)
, d_timeout_send(0)
, d_timeout_recv(0)
, d_recv_buff_size(RCVBUF) { 

  if(!a["host"].blank())     d_host = a["host"].c_str();
  if(!a["port"].blank())     d_port = a["port"].to_int();
  if(!a["sndtimeo"].blank()) d_timeout_send   = a["sndtimeo"].to_int();
  if(!a["rcvtimeo"].blank()) d_timeout_recv   = a["rcvtimeo"].to_int();
  if(!a["cnntimeo"].blank()) d_timeout_conn   = a["cnntimeo"].to_int();
  if(!a["rcvbuf"].blank())   d_recv_buff_size = a["rcvbuf"].to_int();
  debug3("TCP::TCP %s:%d", d_host.c_str(), d_port);
  debug3("TCP::TCP d_timeout_send   = %ld", d_timeout_send);
  debug3("TCP::TCP d_timeout_recv   = %ld", d_timeout_recv);
  debug3("TCP::TCP d_timeout_conn   = %ld", d_timeout_conn);
  debug3("TCP::TCP d_recv_buff_size = %ld", d_recv_buff_size);
}


TCP::~TCP() {
  join();    
  close_main_socket();
  close_all_clients();
  warning3("TCP::~TCP");
}

int
TCP::create_socket() {
  int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  set_attr(sock);
  warning3("TCP::create_socket TCP=%d", sock);
  return sock;
}

void
TCP::join() {
  if(d_thread) {
    int ret;
    ret = pthread_cancel(d_thread);
    void *rval_ptr;
    ret = pthread_join(d_thread, &rval_ptr);
    warning3("TCP::join pthread_join=%d", ret); //ESRCH=3, EINVAL=22
    d_thread = NULL;
  }
}

void
TCP::cnntimeo(long msec) {
  d_timeout_conn = msec;
  set_attr(d_sock);
}

void
TCP::sndtimeo(long msec) {
  d_timeout_send = msec;
  set_attr(d_sock);
}

void
TCP::rcvtimeo(long msec) {
  d_timeout_recv = msec;
  set_attr(d_sock);
}

void
TCP::rcvbuf(int size) {
  if(size>0) d_recv_buff_size = size;
  //set_attr(d_sock);
}

void
TCP::reuse() const { //Разделяем порт (если надо)
  const int on = 1;
  if(setsockopt(d_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) == -1) {
    error("TCP::reuse setsockopt(%d, SO_REUSEADDR", d_sock);
  }
}

void // Наследование сокета не во всех системах имеет место, поэтому, по-хорошему,
TCP::set_attr(int sock) const { // не надо полагаться на автоматическое наследование и задать все атрибуты сокета вручную
  if(sock<0) return;

  int flag = 1; 
  if( setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (char *) &flag, sizeof(int))<0 ) { // предотвращение события "Broken Pipe" - аварийного завершения всей программы; наследуется всеми порождёнными сокетами
    warning("TCP::set_attr setsockopt(%d, SO_NOSIGPIPE)", sock);
  }

  flag = 0; 
  if( setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int))<0 ) { //отключить обмен, а то через 2 часа пошлёт неизвестно что
    warning("TCP::set_attr setsockopt(%d, SO_KEEPALIVE)", sock);
  }
  
  //http://rudocs.exdat.com/docs/index-45978.html?page=43
  flag = 1; 
  if( setsockopt(sock, SOL_SOCKET, TCP_NODELAY, (char *) &flag, sizeof(int))<0 ) { //отключить Нагла
    warning("TCP::set_attr setsockopt(%d, TCP_NODELAY)", sock);
  }

  //fcntl(sock, F_SETFL, O_NONBLOCK); //все новые сокеты переводим в неблокирующий режим
  
  //Устанавка размера принимающего буфера - !!сомнительная возможность!! - неожиданные результаты
  /*socklen_t size = sizeof(int);
  if(d_recv_buff_size>0 && setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void*) &d_recv_buff_size, size) == -1) {
    warning("warning: setsockopt(%d, SO_RCVBUF, %d)", sock, d_recv_buff_size);
  }*/
  
  /*int recv_buff_size = 0;
  if(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buff_size, &size) != -1) {
    warning("warning: getsockopt(%d, SO_RCVBUF)= %d", sock, recv_buff_size);
  }*/
  
  // Устанавливаем время приёма
  if( d_timeout_recv>0 && setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &d_timeout_recv, sizeof(d_timeout_recv))<0 ) { //время ожидания приёма
    error("TCP::set_attr setsockopt(%d, SO_RCVTIMEO, %lld)", sock, d_timeout_recv);
  }
  
  // Таймаут на отправку надо устанавливать обязательно - иначе сервер долго ожидает какого-нибудь тормозного клиента
  if( d_timeout_send>0 && setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &d_timeout_send, sizeof(d_timeout_send))<0 ) { //время ожидания отправки
    error("TCP::set_attr setsockopt(%d, SO_SNDTIMEO, %lld)", sock, d_timeout_send);
  }
}

bool
TCP::accept(int maxcon, const char* iface, int port) { // Переводим сокет в режим сервера
  // Привязываем к сокету локальный адрес - для приёма соединений
  sockaddr_in addr = mkaddr(iface, port);  
  if(addr.sin_family==0) {
    warning("TCP::accept bad listen name %s:%d", iface, port);
    return false;
  }
  socklen_t len = sizeof(addr);
  if( bind(d_sock, (sockaddr*)&addr, len)==-1 ) {
    error("TCP::accept bind %s:%d", iface, port);
    return false;
  }
  
  if(d_thread==0) { // следим, что поток создаётся однократно
    if(pthread_create(&d_thread, NULL, thread_event, (void*) this)!=0) {
      error("TCP::accept pthread_create(thread_event)");
      d_thread = 0;
      return false;
    }
  }
  
  if(::listen(d_sock, maxcon)==-1) {  // слушать соединения на сокете  
    //EADDRINUSE - Другой сокет уже слушает на этом же порту
    error("TCP::accept listen(%d)", maxcon);
    return false;
  }
  
  while(1) {
    sockaddr_in addr = {0};
    socklen_t len = sizeof(sockaddr_in);
    int new_sock = ::accept(d_sock, (sockaddr*)&addr, &len); // создаём новые сокеты и запихиваем их в прослушиваемую очередь
    // ECONNREFUSED - если очередь переполнена
    if(new_sock>-1) {
      set_attr(new_sock);
      d_socks.push_back(new_sock);  // добавляем на прослушивание новые сокеты
      warning3("TCP::accept accept new socked %d, count sock=%d", new_sock, d_socks.size());
    }
  }

  return true;
}

void
TCP::disconnect() {
  //close_main_socket(); // всегда считал, что отсоединение сокета равносильно его разрушению, - может я не прав?? я был не прав - переподключаться можно!!
  if(d_sock>=0) {
    if(shutdown(d_sock, SHUT_RDWR)==-1) error("TCP::disconnect shutdown main socket %d", d_sock); //SHUT_RDWR=2
    warning3("TCP::disconnect shutdown main socket %d", d_sock);
  }
  d_is_connect = false;
}

bool
TCP::connect(const char* host, int port) {
  sockaddr_in addr = mkaddr(host, port);
  if(addr.sin_family==0) {
    warning("TCP::connect bad connect name %s:%d", host, port);
    return false;
  }
  return connect(addr);
}

bool
TCP::connect() {
  if(d_host.empty() || d_port==0) return false;
  
  sockaddr_in addr = mkaddr(d_host.c_str(), d_port);
  if(addr.sin_family==0) {
    warning("TCP::connect bad connect name %s:%d", d_host.c_str(), d_port);
    return false;
  }
  return connect(addr);
}

bool
TCP::connect(const sockaddr_in& addr) {
  if(d_is_connect) return true;
  
  if(d_sock<0) d_sock = create_socket(); // создадим, если ещё не создавали
  
  if(d_sock<0) {    
    warning("TCP::connect bad socket for connect");
    return false;
  }
  
  if(addr.sin_family==0) {
    warning("TCP::connect bad connect name");
    return false;
  }
  
  //if(!listen()) return false;
  
  // В FreeBSD отсутствует управление временем ожидания соединения. Чтобы выйти из положения,
  // придётся работать с сокетом в неблокирующем режиме:
  const int flags = fcntl(d_sock, F_GETFL);
  if(flags==-1 || fcntl(d_sock, F_SETFL, flags | O_NONBLOCK)==-1) error("O_NONBLOCK"); // переходим в неблокирующий режим
  int ret = ::connect(d_sock, (sockaddr*)&addr, sizeof(sockaddr_in));
  //if(ret==-1) error("connect"); //EINPROGRESS - Сокет является неблокирующим
  
  fd_set readfds, writefds;
  FD_ZERO(&readfds); FD_ZERO(&writefds);
  FD_SET(d_sock, &readfds); FD_SET(d_sock, &writefds);
  
  timeval tv = {d_timeout_conn/1000, (d_timeout_conn%1000)*1000 }; // {time_t tv_sec [seconds] suseconds_t, tv_usec [microseconds]}
  const int event_count = select(d_sock + 1, &readfds, &writefds, NULL, &tv);
  if(errno==EISCONN) {
    warning("TCP::connect: socket already connected errno=EISCONN=%d", errno);
    d_is_connect = true;
    //flags ^= O_NONBLOCK;
    if(fcntl(d_sock, F_SETFL, flags )==-1) error("ONBLOCK"); // вернём флаги на место
    return d_is_connect;
  }
  
  
  
  //flags ^= O_NONBLOCK;
  if(fcntl(d_sock, F_SETFL, flags )==-1) error("ONBLOCK"); // вернём флаги на место
  d_is_connect = event_count>0;
  if(!d_is_connect) {//(event_count==0)
    error("TCP::connect: sock %d to %s:%d", d_sock, mkip(addr), ntohs(addr.sin_port));  //EINPROGRESS 36 "Operation now in progress"
    disconnect(); // прекращаем все попытки соединения в фоновом(неблокирующем) режиме
    //get_error();    
  }
  else {
    warning3("TCP::connect: success connect sock %d to %s:%d", d_sock, mkip(addr), ntohs(addr.sin_port));
  }
  
  return d_is_connect;
}

bool
TCP::listen() {
  //if(d_sock<0) d_sock = create_socket(); // создадим, если ещё не создавали
  
  /*if(d_sock<0) {
    warning("error: bad socket for connect");
    return false;
  }*/
  
  if(d_thread!=0) { // следим, что поток создаётся однократно
    warning("TCP::listen: thread already created");
    return true;
  }
  
  if(pthread_create(&d_thread, NULL, thread_event, (void*) this)!=0) {
    error("TCP::listen pthread_create");
    d_thread = 0;
    return false;
  }
  
  return true;
}

bool
TCP::send(const char* s, size_t len, int sock) const {
  return sock!=-1 && ::send(sock, s, len, 0) == len;
}

bool
TCP::send(const char* s, size_t len) const { // если d_sock клиент, то отослать сообщение через d_sock
  if(len==0) len=strlen(s);
  //debug3("TCP::send: len=%d, xml=%s", len, s);
  return send(s, len, d_sock);
}

void
TCP::send_all(const char* s, size_t len) const { // отослать всем клиентам одно и то же
  std::list<int>::const_iterator I = d_socks.begin();
  for(; I!=d_socks.end(); I++) send(s, len, *I);
}

sockaddr_in
TCP::mkaddr(const char* HOST, int port) {
  char host[255] = {0};
  if(port==0)
    sscanf(HOST, "%[^:]:%d", host, &port);
  else  
    strcpy(host, HOST);
    
    
  sockaddr_in to = {0};
  if(port==0) {
    //warning("error: bad port %s:%d", host, port);
    return to;
  }
  warning3("TCP::mkaddr mkaddr=%s:%d", host, port);
  
  
  hostent *phe = gethostbyname(host);
  if(!phe) {
    error("TCP::mkaddr gethostbyname host=%s", host);
    return to;
  }
  in_addr inaddr = {0};
  memcpy(&inaddr, phe->h_addr, phe->h_length);
  char *ip = inet_ntoa(inaddr); //адрес удаленной машины
  
  to.sin_family = AF_INET;
  to.sin_addr.s_addr = inet_addr(ip);
  to.sin_port = htons(port);
  return to;
}

const char*
TCP::mkip(sockaddr_in addr) {
  return inet_ntoa(addr.sin_addr);
}

int
TCP::recv(int sock) {
  char *buf = new char[d_recv_buff_size+1];
  bzero(buf, d_recv_buff_size+1);// иначе после ret попадает мусор
  int ret;
  //if( (ret = ::recv(d_sock, buf, sizeof(buf), MSG_DONTWAIT))>0 ) {
  if( (ret = ::recv(sock, buf, d_recv_buff_size, 0))>0 ) {
    read(buf, ret /*TODO from?*/);
    warning3("TCP::recv rcvbuf=%d recv=%d buf=%s", d_recv_buff_size, ret, buf);
  }
  
  delete [] buf;
  return ret;
}

void
TCP::read(const char*, size_t n, const void*) {
  warning3("TCP::read size=%d", n);
}

void
TCP::close_all_clients() {
  //std::list<int>::iterator I = d_socks.begin();
  //for(; I!=d_socks.end(); I++) {
  while(!d_socks.empty()) {
    std::list<int>::iterator I = d_socks.begin();
    int sock = *I;
    d_socks.erase(I);
    shutdown(sock, SHUT_RDWR); //SHUT_RDWR=2    
    close(sock);
    warning("TCP::close_all_clients shutdown and close client sock %d, now clients count is %d", sock, d_socks.size());
  }
}

void
TCP::close_client(int sock) {
  std::list<int>::iterator I = find(d_socks.begin(), d_socks.end(), sock); // найдём экземпляр класса TCP в стопке d_socks с сокетом sock
  if(I==d_socks.end()) // если не нашли такой сокет
    warning("TCP::close_client client socket %d not find", sock);
  else
    d_socks.erase(I); // безопаснее сначала исключить из списка, а потом уже освободить сам сокет; в противном случае есть вероятность, что освобождённый сокет вновь будет добавлен в список, из которого мы его ошибочно удалим следующей командой
  shutdown(sock, SHUT_RDWR); //SHUT_RDWR=2
  close(sock); // по-любому надо закрывать сокет
  warning("TCP::close_client shutdown and close client sock %d, now clients count is %d", sock, d_socks.size());
}

void
TCP::close_main_socket() {
  if(d_sock>=0) {
    disconnect();
    close(d_sock);
    warning3("TCP::close_main_socket close main socket %d", d_sock);
  }
  d_sock = -1; // присваиваем сокету признак недостоверности
}

int TCP::get_error() { // получить и "сбросить" ожидающую обработки ошибку сокета
  int err = 0; socklen_t len = sizeof(err);
  int ret = getsockopt(d_sock, SOL_SOCKET, SO_ERROR, &err, &len);
  return ret==0 ? err : ret;
}
