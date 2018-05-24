cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(dietchan)

file(GLOB_RECURSE SOURCES
	"${PROJECT_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE HEADERS
	"${PROJECT_SOURCE_DIR}/src/*.h"
)

SET(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG" )
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g" )
#SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g" )
#SET(CMAKE_C_FLAGS "-Os" )
SET(CMAKE_C_FLAGS "-fstack-check -fstack-protector-all -pie -fPIE -mmitigate-rop -Wl,-z,relro,-z,now" )

find_library(libowfat_LIBRARIES NAMES libowfat.a libowfat)
find_path(libowfat_INCLUDE_DIRS NAMES libowfat/cdb.h)

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
