add_library(led_daemon_aliases INTERFACE)

set(PUBLIC_HEADERS 
  ${PROJECT_SOURCE_DIR}/led_aliases/led_aliases.json
  ${PROJECT_SOURCE_DIR}/led_aliases/test_aliases.json
  ${PROJECT_SOURCE_DIR}/led_aliases/wdr4300.json
)

set_target_properties(led_daemon_aliases 
  PROPERTIES 
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
)

install(TARGETS led_daemon_aliases
    PUBLIC_HEADER DESTINATION share/led_daemon/aliases
)

