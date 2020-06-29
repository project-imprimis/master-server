cmake_minimum_required(VERSION 3.16)
project(master_server)

set(CMAKE_CXX_STANDARD 14)

include_directories(enet/include)
include_directories(enet/include/enet)
include_directories(src)

add_executable(master_server
        enet/include/enet/callbacks.h
        enet/include/enet/enet.h
        enet/include/enet/etime.h
        enet/include/enet/list.h
        enet/include/enet/protocol.h
        enet/include/enet/types.h
        enet/include/enet/unix.h
        enet/include/enet/utility.h
        enet/include/enet/win32.h
        enet/callbacks.c
        enet/compress.c
        enet/host.c
        enet/list.c
        enet/packet.c
        enet/peer.c
        enet/protocol.c
        enet/unix.c
        enet/win32.c
        src/cube.h
        src/io.cpp
        src/io.h
        src/main.cpp
        src/master.cpp
        src/master.h
        src/tools.cpp
        src/tools.h)
