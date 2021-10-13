#include "iterate_files.h"

#include <glob.h>
#include <stddef.h>
#include <string.h>

void
iterate_files(
    char const * const pattern,
    void(* const cb)(char const * filename, void * user_ctx),
    void * const user_ctx)
{
    glob_t globbuf;

    memset(&globbuf, 0, sizeof globbuf);

    glob(pattern, 0, NULL, &globbuf);

    for (size_t i = 0; i < globbuf.gl_pathc; i++)
    {
        cb(globbuf.gl_pathv[i], user_ctx);
    }

    globfree(&globbuf);
}
