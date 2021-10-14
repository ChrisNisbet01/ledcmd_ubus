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

set_target_properties(${PROJECT_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
)

install(TARGETS ${PROJECT_NAME} 
  RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES 
  led_colours.h
  led_states.h
  platform_specific.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)
