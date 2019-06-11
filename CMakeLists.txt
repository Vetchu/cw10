cmake_minimum_required(VERSION 2.8.12)
project(cw10 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-pthread -Wall -std=gnu99")

add_executable(sClient logic/client.c stream/stream.c stream/stream.h logic/common.h utils/hashmap.c utils/hashmap.h logic/template.h)
add_executable(sServer logic/server.c stream/stream.c stream/stream.h logic/common.h logic/template.h)
