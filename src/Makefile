CXXFLAGS= -O3 -fomit-frame-pointer -ffast-math -std=c++17
override CXXFLAGS+= -Wall -fsigned-char -fno-exceptions -fno-rtti

INCLUDES= -I../enet/include -Ishared

all: master_server

master_server : master.o tools.o
		g++ $(CXXFLAGS) $(INCLUDES) -o master_server master.o tools.o -L../enet -lenet -lz

master.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c master.cpp

tools.o :
		g++ $(CXXFLAGS) $(INCLUDES) -c tools.cpp

clean:
		rm master.o tools.o master_server
