# ubus-based LED manager and associated apps

## basic description

### LED manager
The core of this group of applications is a manager that is responsible for the
management of LEDs on the device.
The manager loads a plugin when it starts, and this plugin is responsible for
identifying the LEDs that the manager can control, and a basic set of features
supported by the plugin (e.g. flashing). If the plugin does not support a
feature (e.g. fast flashing) the manager itself will flash an LED itself by
turning the LED on at off at the approriate time.

### LED CLI app
A simple 'ledcmd' CLI appication is provided that allows for identifying the 
LEDs controlled by the manager, and getting/setting the LED states. This is
achieved behind the scenes by sending ubus requests to the LED manager.

### Patterns
The manager also supports the concept of 'patterns'. A 'pattern' describes a
set of actions to take with an LED of group of LEDs. Patterns are defined in
a JSON file, which is loaded when the manager starts. Patterns may be played
once, multiple times, or indefinitely.

A simple led_pattern CLI application is provided that allows for listing, 
starting and stopping of a pattern.

### Aliases
LED aliases are supported, which allow for grouping a number of LEDs together
using an alias. The LEDs included in the group will all be controlled together
which means that (e.g.) multiple LEDs can be turned ON or OFF at the same time.
An example of a useful alias might be to group all LEDs on the front panel of a
device together, and call it (e.g.) "front_panel"

### Logging
A logging library is provided. The library allows the user to supply a plugin
which deals with the log messages generated by the application. An example
plugin is provided which outputs log messages to stderr. Another plugin might
output messages to syslog, or to the system log, or anywhere else that is 
desired.

