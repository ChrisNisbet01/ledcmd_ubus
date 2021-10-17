cmake_minimum_required(VERSION 3.10)

set(EXE_NAME ledcmd_cli)

project(${EXE_NAME} VERSION 1.0.0 DESCRIPTION "ledcmd with UBUS support")

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
find_library(UBOX ubox)
find_library(UBUS ubus)
find_package(ubus_utils REQUIRED)

SET(SOURCES 
  ledcmd_cli.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

add_dependencies(${PROJECT_NAME} 
  led
  ubus_utils
)

target_include_directories(${PROJECT_NAME}
  PRIVATE
    $<BUILD_INTERFACE:${lib_led_INCLUDE_DIR}>
)

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
    OUTPUT_NAME ${EXE_NAME}
)

install(TARGETS ${PROJECT_NAME} 
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

