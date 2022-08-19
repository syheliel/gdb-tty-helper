// Open a pty and let it idle, so that a process spawned in a different window
// can attach to it, start a new session, and set it as the controlling
// terminal. Useful for gdb debugging with gdb's `tty` command.
 
#include <inttypes.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef  int8_t i8; typedef  int16_t i16; typedef  int32_t i32; typedef  int64_t i64;
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <termios.h>
#include <pty.h>
#include <liburing.h>

#define BSIZE 4096

void raw_terminal(void)
{
    if (!isatty(0))
        return;
    struct termios t;
    tcgetattr(0, &t);
    t.c_lflag &= ~(ISIG | ICANON | ECHO);
    tcsetattr(0, TCSANOW, &t);
}

// Refers to the state of a Joint /while it's waiting in io_uring_enter/.
enum State {
    READ,
    WRITE
};
// Joins two fds together, like splice, but not a syscall and works on any two
// fds.
struct Joint {
    u8 buf[BSIZE];
    i32 ifd;
    i32 ofd;
    enum State state;
    u32 nread;
};
void roll_joint(struct Joint *j, struct io_uring *ur, i32 ifd, i32 ofd)
{
    j->ifd = ifd;
    j->ofd = ofd;
    j->state = READ;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ur);
    io_uring_prep_read(sqe, j->ifd, j->buf, BSIZE, 0);
    io_uring_sqe_set_data(sqe, j);
    io_uring_submit(ur);
}
i32 main(i32 argc, char **argv)
{
    raw_terminal();
    struct io_uring ur;
    assert(io_uring_queue_init(256, &ur, 0) == 0);

    i32 ptm, pts;
    assert(openpty(&ptm, &pts, NULL, NULL, NULL) == 0);
    dprintf(2, "pid = %u   tty = %s\n", getpid(), ttyname(pts));

    struct Joint jkbd;
    roll_joint(&jkbd, &ur, 0, ptm);
    struct Joint jscreen;
    roll_joint(&jscreen, &ur, ptm, 1);

    for (;;) {
        struct io_uring_cqe *cqe;
        for (;;) {
            // Actions like suspend to RAM can interrupt the io_uring_enter
            // syscall. If we get interrupted, try again. For all other errors,
            // bail. Also, wait_cqe negates the error for no reason. It never
            // returns positive numbers. Very silly.
            u32 res = -io_uring_wait_cqe(&ur, &cqe);
            if (res == 0)
                break;
            else if (res != EINTR) {
                dprintf(2, "io_uring_enter returns errno %d\n", res);
                exit(res);
            }
        }
        struct Joint *j = io_uring_cqe_get_data(cqe);
        if (j->state == READ) {
            // Exiting READ state. Finish with the read...
            j->nread = cqe->res;
            assert(j->nread > 0);

            // Now, start the write.
            j->state = WRITE;
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ur);
            io_uring_prep_write(sqe, j->ofd, j->buf, j->nread, 0);
            io_uring_sqe_set_data(sqe, j);
            io_uring_submit(&ur);
        }
        else if (j->state == WRITE) {
            // Exiting WRITE state. Finish with the write...
            i64 nwritten = cqe->res;
            assert(nwritten == j->nread);

            // Now, start the read.
            j->state = READ;
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ur);
            io_uring_prep_read(sqe, j->ifd, j->buf, BSIZE, 0);
            io_uring_sqe_set_data(sqe, j);
            io_uring_submit(&ur);
        }
        io_uring_cqe_seen(&ur, cqe);
    }

    io_uring_queue_exit(&ur);
    return 0;
}
