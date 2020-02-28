#ifndef __LIB_LOG_H__
#define __LIB_LOG_H__

#include <stdbool.h>

int
log_error(char const *fmt, ...);

int
log_info(char const *fmt, ...);

bool
logging_plugin_load(
    char const *plugin_directory, char const *progname, int a, int b);

void
logging_plugin_unload(void);

#endif /* __LIB_LOG_H__ */
