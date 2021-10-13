#ifndef ITERATE_FILES_H__
#define ITERATE_FILES_H__

void
iterate_files(
    char const * const pattern,
    void (*cb)(char const * filename, void * user_ctx),
    void * user_ctx);

#endif /* ITERATE_FILES_H__ */

