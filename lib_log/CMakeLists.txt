cmake_minimum_required(VERSION 3.10)

project(lib_log VERSION 1.0.0 DESCRIPTION "Logging plugin library")

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

set(LIB_HEADERS 
  include/${PROJECT_NAME}/log.h
  include/${PROJECT_NAME}/logging_plugin.h
)

SET(LIB_SOURCES 
  src/lib_log.c
  ${LIB_HEADERS}
)

SET(LIB_NAME log)

add_library(${LIB_NAME} SHARED ${LIB_SOURCES})

set(${PROJECT_NAME}_INCLUDE_DIR
  ${PROJECT_SOURCE_DIR}/include
  CACHE FILEPATH "${PROJECT_NAME} include directory"
)

target_include_directories(${LIB_NAME}
  PUBLIC
    ${ubus_utils_path}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set(PUBLIC_HEADERS 
  include/${PROJECT_NAME}/log.h
  include/${PROJECT_NAME}/logging_plugin.h
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

