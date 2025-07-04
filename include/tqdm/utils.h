#ifndef TQDM_UTILS_H
#define TQDM_UTILS_H

#include <stddef.h> /* size_t */
#include <stdbool.h> /* bool */

#ifdef __cplusplus
extern "C" {
#endif

const char *term_move_up(void);

void wait_for_write(int fd); /* TODO: not needed? */
bool write_harder(int fd, const char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TQDM_UTILS_H */
