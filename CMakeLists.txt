cmake_minimum_required (VERSION 3.9)
project (LINE)

set (LINE_VERSION_MAJOR 1)
set (LINE_VERSION_MINOR 0)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE "-Ofast -s -fno-ident -march=native -DNDEBUG")

set(CMAKE_C_STANDARD 99)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}")

add_definitions(-D_POSIX_C_SOURCE=200809L)

SET(MAIN
    "src/main.c"
    )

add_executable(line ${MAIN})

set_property(TARGET line PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
