cmake_minimum_required(VERSION 2.8)

include(ExternalProject)

set(BUILD_ROOT "${PROJECT_SOURCE_DIR}/build")

ExternalProject_Add(
	dietlibc
	CVS_REPOSITORY ":pserver:cvs@cvs.fefe.de:/cvs"
	CVS_MODULE "dietlibc"
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/dietlibc"
	PREFIX "${PROJECT_SOURCE_DIR}/.prefix/dietlibc"
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ""
	UPDATE_COMMAND ""
	TEST_COMMAND ""
	PATCH_COMMAND ""
	BUILD_COMMAND make prefix=${BUILD_ROOT}
	INSTALL_COMMAND make prefix=${BUILD_ROOT} install && 
		echo -e "#!/bin/sh\\nDIETHOME=${BUILD_ROOT} ${BUILD_ROOT}/bin/diet gcc \"$@\"" > ${BUILD_ROOT}/diet-gcc &&
		chmod +x ${BUILD_ROOT}/diet-gcc
)

ExternalProject_Add(
	libowfat
	CVS_REPOSITORY ":pserver:cvs@cvs.fefe.de:/cvs"
	CVS_MODULE "libowfat"
	#URL "https://www.fefe.de/libowfat/libowfat-0.31.tar.xz"
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/libowfat"
	PREFIX "${PROJECT_SOURCE_DIR}/.prefix/libowfat"
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ""
	UPDATE_COMMAND ""
	TEST_COMMAND ""
	PATCH_COMMAND ""
	BUILD_COMMAND DIETHOME=${BUILD_ROOT} make DIET=${BUILD_ROOT}/bin/diet
	INSTALL_COMMAND make DESTDIR=${BUILD_ROOT} install
)

ExternalProject_Add(
	captcha
	GIT_REPOSITORY "https://gitgud.io/zuse/captcha.git"
	GIT_TAG master
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/captcha"
	PREFIX "${PROJECT_SOURCE_DIR}/.prefix/captcha"
	BUILD_IN_SOURCE 1
	BUILD_ALWAYS 1
	UPDATE_COMMAND ""
	INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/captcha/captcha ${PROJECT_SOURCE_DIR}
	CMAKE_ARGS -DCMAKE_C_COMPILER=${BUILD_ROOT}/diet-gcc
	           -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
)

ExternalProject_Add(
	dietchan
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/dietchan"
	PREFIX "${PROJECT_SOURCE_DIR}/.prefix/dietchan"
	BUILD_IN_SOURCE 1
	BUILD_ALWAYS 1
	UPDATE_COMMAND ""
	INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/dietchan/dietchan ${PROJECT_SOURCE_DIR} &&
		${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/src/dietchan/data ${PROJECT_SOURCE_DIR}/data
	CMAKE_ARGS -DCMAKE_C_COMPILER=${BUILD_ROOT}/diet-gcc
	           -DCMAKE_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/libowfat
	           -DCMAKE_INCLUDE_PATH=${PROJECT_SOURCE_DIR}/src
	           -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
)

add_dependencies(libowfat dietlibc)
add_dependencies(captcha dietlibc)
add_dependencies(dietchan libowfat dietlibc)


# Only for IDE support
FILE(GLOB_RECURSE DIETCHAN_SOURCES "src/dietchan/src/*.c")
add_custom_target(dietchan_src SOURCES ${DIETCHAN_SOURCES})
