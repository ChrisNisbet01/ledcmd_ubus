cmake_minimum_required(VERSION 3.10)

project(ledcmd_cli VERSION 1.0.0 DESCRIPTION "ledcmd with UBUS support")

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
find_package(libled CONFIG REQUIRED)
find_library(UBOX ubox)
find_library(UBUS ubus)
find_package(ubus_utils CONFIG REQUIRED)

SET(SOURCES 
  ledcmd_cli.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
  led
  ubus_utils
  ${BLOBMSG_JSON}
  ${UBOX}
  ${UBUS}
)

set_target_properties(${PROJECT_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
)

install(TARGETS ${PROJECT_NAME} 
  RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

