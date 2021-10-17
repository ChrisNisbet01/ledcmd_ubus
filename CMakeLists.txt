cmake_minimum_required(VERSION 3.10)

project(ubus_led_daemon VERSION 1.0.0 DESCRIPTION "UBUS led_daemon")

option(BUILD_TEST_LED_BACKEND "Build the test LED backend" ON)
option(BUILD_SYSFS_LED_BACKEND "Build the sysfs LED backend" OFF)

option(BUILD_STDERR_LOGGING_PLUGIN "Build the stderr logging plugin" ON)

add_subdirectory(lib_led)
add_subdirectory(lib_log)

add_subdirectory(led_cli)
add_subdirectory(led_pattern)
add_subdirectory(led_daemon)

if(${BUILD_TEST_LED_BACKEND})
add_subdirectory(led_daemon_backends/test)
endif()

if(${BUILD_SYSFS_LED_BACKEND})
add_subdirectory(led_daemon_backends/sysfs)
endif()

if(${BUILD_STDERR_LOGGING_PLUGIN})
add_subdirectory(logging_stderr)
endif()

