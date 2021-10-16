cmake_minimum_required(VERSION 3.10)

project(led_daemon VERSION 1.0.0 DESCRIPTION "led_daemon")

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

add_subdirectory(src)
add_subdirectory(led_aliases)
add_subdirectory(led_patterns)

