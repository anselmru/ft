CC      = clang
CPP     = $(CC)
CXX     = $(CC)
LINK    = $(CC)

RM      = rm -f
CFLAGS += -I/usr/local/include -I/usr/local/include/libxml2 -O2 -DNDEBUG
LFLAGS += -L/usr/local/lib -lpthread -lmd -lstdc++ -lxml2
BIN     = k104
RC      = k104
SRC     = main.cpp tcpclient.cpp modbustcp.cpp udp.cpp xml.cpp event.cpp
HDRS    = 
OBJ_CPP = $(SRC:.cpp=.o)
OBJ     = $(OBJ_CPP:.c=.o)
BINDIR  = /usr/local/bin
ETCDIR  = /usr/local/etc
RCDIR   = /usr/local/etc/rc.d


all: $(BIN)
	strip -s $(BIN)

$(BIN): $(OBJ)
	$(LINK) $(OBJ) -o $(BIN) $(LFLAGS) 

clean:
	$(RM) $(OBJ) $(BIN) $(BIN).core

rebuild: clean all
  
install: all
	cp -f $(BIN) $(BINDIR)/
	cp $(RC).conf $(ETCDIR)/
	sed -e 's/DAEMON_NAME/$(RC)/g' -e 's/BINARY_NAME/$(BIN)/g' rc > $(RCDIR)/$(RC)
	chmod 550 $(BINDIR)/$(BIN)
	chmod 550 $(RCDIR)/$(RC)
	chmod 440 $(ETCDIR)/$(RC).conf
	if [ `grep '$(RC)_enable=' /etc/rc.conf | wc -l` = 0 ]; then echo '$(RC)_enable="YES"' >> /etc/rc.conf; fi
  
deinstall:
	$(RM) $(RCDIR)/$(RC)
	$(RM) $(BINDIR)/$(BIN)
	$(RM) $(ETCDIR)/$(RC).conf
  
reinstall: deinstall install  
  
start:
	$(RCDIR)/$(RC) start

stop:
	$(RCDIR)/$(RC) stop

restart: stop start
