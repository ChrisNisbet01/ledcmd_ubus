cmake_minimum_required(VERSION 3.10)

set(EXE_NAME led_pattern)

project(${EXE_NAME} VERSION 1.0.0 DESCRIPTION "led pattern player")

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
find_package(ubus_utils CONFIG REQUIRED)
find_library(UBUS ubus)

SET(SOURCES 
  led_pattern.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
  led
  ubus_utils
  ${BLOBMSG_JSON}
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

