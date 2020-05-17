cmake_minimum_required(VERSION 3.16)
project(HW4 C)

set(CMAKE_C_STANDARD 99)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O5 -Wall -Werror -Wextra")

file(GLOB client_src
        "include/*.h"
        "src/client/*.c"
        )

file(GLOB server_src
        "include/*.h"
        "src/server/*.c"
        )

add_executable(Server ${server_src})

add_executable(Client ${client_src})