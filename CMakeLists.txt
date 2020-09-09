cmake_minimum_required(VERSION 3.16)
project(imprimis_master)

set(GCC_COMPILE_FLAGS "-O3 -fomit-frame-pointer -ffast-math") # -O3 -fomit-frame-pointer -ffast-math
set(GCC_COMPILE_FLAGS_DBG "-O0 -g")
set(EXE_LINKER_FLAGS "-lz")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${GCC_COMPILE_FLAGS_DBG}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EXE_LINKER_FLAGS}")

include(FetchContent)
FetchContent_Declare(
        enet
        GIT_REPOSITORY https://github.com/project-imprimis/enet/
        GIT_TAG cmake_fix
)
FetchContent_MakeAvailable(enet)

add_executable(imprimis_master src/main.cpp src/types.h src/types.cpp src/shims.cpp src/shims.h src/network.cpp src/network.h src/io.cpp src/io.h src/config.h src/master.cpp src/master.h)
#include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/enet_git/include)
#target_link_libraries(imprimis_master ${install_dir}/src/enet_git-build/libenet.a)
target_link_libraries(imprimis_master PUBLIC enet)