cmake_minimum_required(VERSION 3.10)

project(led_daemon_sysfs_plugin VERSION 1.0.0 DESCRIPTION "sysfs LED backend plugin")

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

SET(SOURCES 
  led_sysfs.c
)

SET(LIB_NAME led_daemon_sysfs_plugin)

add_library(${LIB_NAME} MODULE ${SOURCES})

target_link_libraries(${LIB_NAME}
  ubus_utils
)

set_target_properties(${LIB_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    PREFIX ""
)

install(TARGETS ${LIB_NAME} 
  LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}/led_daemon/plugins
)

