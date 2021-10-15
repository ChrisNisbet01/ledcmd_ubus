SET(SOURCES 
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
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} 
  PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
  led
  ubus_utils
  ${BLOBMSG_JSON}
  ${UBOX}
  ${UBUS}
  ${JSON_C}
  dl
  log
)

set(PUBLIC_HEADERS 
  ${PROJECT_SOURCE_DIR}/include/led_colours.h
  ${PROJECT_SOURCE_DIR}/include/led_states.h
  ${PROJECT_SOURCE_DIR}/include/platform_specific.h
)

set_target_properties(${PROJECT_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
)

install(TARGETS ${PROJECT_NAME} 
  RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)

