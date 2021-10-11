#ifndef __ITERATE_FILES_H__
#define __ITERATE_FILES_H__

void
iterate_files(
	char const * const pattern,
	void (*cb)(char const * filename, void * user_ctx),
	void * user_ctx);

#endif /* __ITERATE_FILES_H__ */
