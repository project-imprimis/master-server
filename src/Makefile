CXXFLAGS= -O0 -ggdb3 -fexceptions -std=c++17
override CXXFLAGS+= -Wall -fsigned-char -fno-rtti

INCLUDES= -I../enet/include -Ishared

all: master_server

master_server : main.o master.o tools.o io.o
		g++ $(CXXFLAGS) $(INCLUDES) -o master_server main.o master.o tools.o io.o -L../enet -lenet -lz

main.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c main.cpp

master.o : master.h
		g++ $(CXXFLAGS) $(INCLUDES) -c master.cpp

tools.o : tools.h
		g++ $(CXXFLAGS) $(INCLUDES) -c tools.cpp

io.o : io.h
		g++ $(CXXFLAGS) $(INCLUDES) -c io.cpp

clean:
		rm main.o master.o tools.o io.o master_server
