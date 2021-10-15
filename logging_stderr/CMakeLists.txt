cmake_minimum_required(VERSION 3.10)

project(logging_stderr_plugin VERSION 1.0.0 DESCRIPTION "stderr logging plugin")

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

find_package(lib_log REQUIRED)
find_path(lib_log_path lib_log)

SET(SOURCES 
  logging_stderr.c
)

add_library(${PROJECT_NAME} MODULE ${SOURCES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
    ${lib_log_path}
)

add_dependencies(${PROJECT_NAME} 
  log
)

set_target_properties(${PROJECT_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    PREFIX ""
)

install(TARGETS ${PROJECT_NAME} 
  LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}/logging_plugins
)

