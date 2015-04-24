CPP     = g++
LINK    = g++
RM      = rm -f
CFLAGS += -I/usr/local/include -O2 -DNDEBUG
LFLAGS += -L/usr/local/lib -lpthread
BIN     = k104h2
HDRS    = tcpclient.h
SRC     = main.cpp tcpclient.cpp
OBJ     = $(SRC:.cpp=.o)

all: $(BIN)
	strip -s $(BIN)

$(BIN): $(OBJ)
	$(LINK) $(OBJ) -o $(BIN) $(LFLAGS) 

%.o: %.cpp
	$(CPP) -c $(CFLAGS) $< -o $@

clean:
	$(RM) $(OBJ) $(BIN)

rebuild: clean all
