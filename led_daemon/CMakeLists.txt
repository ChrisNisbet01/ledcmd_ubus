cmake_minimum_required(VERSION 3.10)

project(led_daemon VERSION 1.0.0 DESCRIPTION "led_daemon")

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

find_library(BLOBMSG_JSON blobmsg_json CONFIG REQUIRED)
find_library(JSON_C json-c)
find_package(libled CONFIG REQUIRED)
find_package(liblog CONFIG REQUIRED)
find_library(UBOX ubox)
find_library(UBUS ubus)
find_package(ubus_utils CONFIG REQUIRED)

add_subdirectory(src)
add_subdirectory(include)

