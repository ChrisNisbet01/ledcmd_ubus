cmake_minimum_required(VERSION 3.10)

project(lib_led VERSION 1.0.0 DESCRIPTION "UBUS LED library")

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

SET(LIB_SOURCES 
  src/lib_led.c
  src/lib_led_control.c
  src/lib_led_pattern.c
  src/string_constants.c
)

SET(LIB_NAME led)

add_library(${LIB_NAME} SHARED ${LIB_SOURCES})

target_include_directories(${LIB_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${LIB_NAME}
  ubus_utils
)

set(PUBLIC_HEADERS 
  include/lib_led.h
  include/lib_led_control.h
  include/lib_led_pattern.h
  include/string_constants.h
)

set_target_properties(${LIB_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
)

install(TARGETS ${LIB_NAME} 
  EXPORT ${PROJECT_NAME}-config
  LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)

install(
  EXPORT ${PROJECT_NAME}-config 
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

