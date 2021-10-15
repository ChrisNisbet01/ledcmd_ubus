add_library(led_daemon_patterns INTERFACE)

set(PUBLIC_HEADERS 
  ${PROJECT_SOURCE_DIR}/led_patterns/patterns.json
  ${PROJECT_SOURCE_DIR}/led_patterns/wdr4300.json
)

set_target_properties(led_daemon_patterns 
  PROPERTIES 
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
)

install(TARGETS led_daemon_patterns
    PUBLIC_HEADER DESTINATION share/led_daemon/patterns
)

