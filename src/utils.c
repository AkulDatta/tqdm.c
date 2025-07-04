#include "tqdm/utils.h"
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

/* =============================
 * Terminal helper
 * ============================= */

const char *term_move_up(void) {
#if defined(_WIN32) && !defined(__MINGW32__)
    return ""; /* no ANSI on old Windows console */
#else
    return "\x1b[A"; /* ANSI: move cursor one line up */
#endif
}

/* =============================
 * Write helpers
 * ============================= */
static void wait_for_write_internal(int fd) {
    struct pollfd pfd = { .fd = fd, .events = POLLOUT };
    (void)poll(&pfd, 1, -1);
}

/* TODO */
void wait_for_write(int fd) {
    wait_for_write_internal(fd);
}

bool write_harder(int fd, const char *buf, size_t len) {
    bool did_anything = false;
    while (len > 0) {
        ssize_t res = write(fd, buf, len);
        if (res == -1) {
            if (errno == EAGAIN || errno == EINTR) {
                if (!did_anything) return false;
                wait_for_write_internal(fd);
                continue;
            }
            return false;
        }
        assert(res > 0);
        did_anything = true;
        buf += res;
        len -= (size_t)res;
    }
    return true;
} 