cmake_minimum_required(VERSION 3.10)

project(led_daemon_test_plugin VERSION 1.0.0 DESCRIPTION "test LED backend plugin")

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
find_package(led_daemon CONFIG REQUIRED)
find_path(led_daemon_path led_daemon)

SET(SOURCES 
  led_test.c
)

SET(LIB_NAME led_daemon_test_plugin)

add_library(${LIB_NAME} MODULE ${SOURCES})

target_include_directories(${LIB_NAME}
  PUBLIC
    ${led_daemon_path}
)

set_target_properties(${LIB_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    PREFIX ""
)

install(TARGETS ${LIB_NAME} 
  LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}/led_daemon/plugins
)

