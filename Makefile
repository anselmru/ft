#####################################################
#                                                   #
#                For build daemon                   #
#                                                   #
#####################################################
CC      = g++
CPP     = $(CC)
CXX     = $(CC)
LINK    = $(CC)

RM      = rm -f
CFLAGS += -I/usr/local/include/libxml2 \
          -std=c++14 -std=gnu++14 \
          -DSYSLOG -DDEBUG=0 -DDB_DIR="\"$(DBDIR)\""
LFLAGS += -L/usr/local/lib -lpthread -lstdc++ -lxml2 -lmd -lm
BIN     = ft
SRC     = main.cpp ft.cpp event.cpp daemon.cpp dem.cpp tcp.cpp udp.cpp xml.cpp dev.cpp date.cpp file.cpp
#SRC     = $(wildcard *.cpp)
HDRS    =
OBJ_CPP = $(SRC:.cpp=.o)
OBJ     = $(OBJ_CPP:.c=.o)
DBDIR   = /var/db/ft

all: $(BIN)
	@strip -s $(BIN)

$(BIN): $(OBJ)
	$(LINK) $(OBJ) -o $(BIN) $(LFLAGS) 

%.o: %.cpp
	$(CPP) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(BIN) $(BIN).core

rebuild: clean all

#####################################################
#                                                   #
#          For install and control daemon           #
#                                                   #
#####################################################
RC      = ft_ppv
BINDIR  = /usr/local/bin
ETCDIR  = /usr/local/etc
RCDIR   = /usr/local/etc/rc.d
PIDDIR  = /var/run

install: all
	@cp -f $(BIN) $(BINDIR)
	@chmod 550 $(BINDIR)/$(BIN)
	@mkdir -p $(DBDIR)
	
	@for rc in $(RC); do \
		cp $${rc}.conf $(ETCDIR); chmod 440 $(ETCDIR)/$${rc}.conf; \
		sed -e "s/DAEMON_NAME/$$rc/g" -e 's/BINARY_NAME/$(BIN)/g' -e 's|BINDIR|$(BINDIR)|g' -e 's|ETCDIR|$(ETCDIR)|g' -e "s|PIDDIR|$(PIDDIR)|g" rc > $(RCDIR)/$$rc; \
		chmod 550 $(RCDIR)/$$rc; \
		if [ `grep $${rc}_enable= /etc/rc.conf | wc -l` = 0 ]; then \
			echo $${rc}_enable=\"YES\" >> /etc/rc.conf; \
		fi; \
		echo "$$rc -------------- install"; \
	done \
  
deinstall:
	@$(RM) $(BINDIR)/$(BIN)
	@for rc in $(RC); do \
		$(RM) $(RCDIR)/$${rc}; \
		$(RM) $(ETCDIR)/$${rc}.conf; \
		echo "$$rc -------------- deinstall"; \
	done \
  
reinstall: deinstall install  
  
start:
	@for rc in $(RC); do \
		$(RCDIR)/$${rc} start; \
	done \

stop:
	@for rc in $(RC); do \
		$(RCDIR)/$${rc} stop; \
	done \

restart: stop start
