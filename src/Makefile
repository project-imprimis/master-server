CXXFLAGS= -O3 -fomit-frame-pointer -ffast-math -std=c++17
override CXXFLAGS+= -Wall -fsigned-char -fno-exceptions -fno-rtti

INCLUDES= -I../enet/include -Ishared

all: master_server

master_server : master.o tools.o crypto.o
		g++ $(CXXFLAGS) $(INCLUDES) -o master_server master.o tools.o crypto.o -L../enet -lenet -lz

master.o : master.cpp cube.h tools.h crypto.h
		g++ $(CXXFLAGS) $(INCLUDES) -c master.cpp

tools.o : tools.cpp cube.h tools.h crypto.h
		g++ $(CXXFLAGS) $(INCLUDES) -c tools.cpp

crypto.o : crypto.cpp cube.h tools.h crypto.h
		g++ $(CXXFLAGS) $(INCLUDES) -c crypto.cpp

clean:
		rm master.o tools.o crypto.o master_server
