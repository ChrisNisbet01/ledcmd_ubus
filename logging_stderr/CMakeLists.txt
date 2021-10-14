cmake_minimum_required(VERSION 3.10)

project(logging_plugin VERSION 1.0.0 DESCRIPTION "stderr logging plugin")

set(CMAKE_C_COMPILER "gcc")

add_compile_options(
   -std=gnu11
  -O3 
  -Wall 
  -Werror
  -Wextra 
  -g 
  -D_GNU_SOURCE 
)

include(GNUInstallDirs)

find_package(ubus_utils CONFIG REQUIRED)
find_package(lib_log CONFIG REQUIRED)

SET(SOURCES 
  logging_stderr.c
)

SET(LIB_NAME logging_plugin)

add_library(${LIB_NAME} MODULE ${SOURCES})

target_include_directories(${LIB_NAME}
  PUBLIC
    ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)

set_target_properties(${LIB_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    PREFIX ""
)

install(TARGETS ${LIB_NAME} 
  LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}/logging_plugins
)

