find_library(BLOBMSG_JSON blobmsg_json CONFIG REQUIRED)
find_library(JSON_C json-c)
find_library(UBOX ubox)
find_library(UBUS ubus)
find_package(ubus_utils CONFIG REQUIRED)

add_executable(${PROJECT_NAME})

set(${PROJECT_NAME}_INCLUDE_DIR
  ${PROJECT_SOURCE_DIR}/include
  CACHE FILEPATH "${PROJECT_NAME} include directory"
)

set(PROJECT_HEADERS
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/flash_types.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/iterate_files.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_aliases.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_colours.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_control.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_states.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_daemon_ubus.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_lock.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_pattern_control.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_pattern_list.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_patterns.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_priorities.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_priority_context.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_states.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/platform_leds_plugin.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/platform_specific.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/priorities.h
)

target_sources(${PROJECT_NAME} 
  PRIVATE
    flash_types.c
    iterate_files.c
    led_aliases.c
    led_colours.c
    led_control.c
    led_daemon_ubus.c
    led_lock.c
    led_pattern_control.c
    led_patterns.c
    led_priorities.c
    led_priority_context.c
    led_states.c
    ledcmd_daemon.c
    platform_leds_plugin.c
    priorities.c
    ${PROJECT_HEADERS}
)

target_include_directories(${PROJECT_NAME} 
  PRIVATE 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}>
    $<BUILD_INTERFACE:${lib_led_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${lib_log_INCLUDE_DIR}>
  PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${PROJECT_NAME}
  ${BLOBMSG_JSON}
  ${UBUS}
  led
  ubus_utils
  ${UBOX}
  ${JSON_C}
  log
  dl
)

set(PUBLIC_HEADERS 
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_colours.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/led_states.h
  ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/platform_specific.h
)

set_target_properties(${PROJECT_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
)

install(TARGETS ${PROJECT_NAME} 
  EXPORT ${PROJECT_NAME}-config
  RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)

install(
  EXPORT ${PROJECT_NAME}-config 
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

