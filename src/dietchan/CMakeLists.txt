cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(bbs)

file(GLOB_RECURSE SOURCES
	"${PROJECT_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE HEADERS
	"${PROJECT_SOURCE_DIR}/src/*.h"
)

#SET(CMAKE_BUILD_TYPE "Release")
#SET(CMAKE_BUILD_TYPE "Debug")

SET(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG" )
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g" )
#SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g" )
#SET(CMAKE_C_FLAGS "-Os" )
SET(CMAKE_C_FLAGS "-fstack-check -fstack-protector-all -pie -fPIE -mmitigate-rop -Wl,-z,relro,-z,now" )

#SET(CMAKE_C_COMPILER "${PROJECT_SOURCE_DIR}/diet-gcc")

find_library(libowfat_LIBRARIES NAMES libowfat.a libowfat)
find_path(libowfat_INCLUDE_DIRS NAMES libowfat/cdb.h)

find_library(sqlite_LIBRARIES NAMES sqlite3)
find_path(sqlite_LIBRARIES NAMES sqlite3.h)
# Actual program

add_executable(
	"dietchan"
	"${SOURCES}"
)

target_include_directories(
	"dietchan" "PRIVATE"
	"${PROJECT_SOURCE_DIR};"
	"${libowfat_INCLUDE_DIRS};"
)

target_link_libraries(
	"dietchan"
	"${libowfat_LIBRARIES}"
	"compat"
)
