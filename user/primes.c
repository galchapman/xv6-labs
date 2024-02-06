#include "kernel/types.h"
#include "user/user.h"

#define MAX_NUMBER 35

int main(void) {
    int fds[2];

    if (pipe(fds) < 0) {
        fprintf(2, "Unable to create pipe!\n");
        exit(-1);
    }

    if (fork() != 0) {
        /* first parent */
        close(fds[0]);
        /* write numbers 2...35 */
        for (int j = 2; j <= MAX_NUMBER; ++j) {
            write(fds[1], &j, sizeof(j));
        }
        close(fds[1]);
        /* wait for child */
        wait((void*)0);
        exit(0);
    }

    /* a child */
    while (1) {
        int parent_read_fd;
        int n, x;

        /* update pipes */
        close(fds[1]);
        parent_read_fd = fds[0];
        fds[0] = fds[1] = -1;

        /* read first number */

        if (read(parent_read_fd, &n, sizeof(n)) == 0) {
            /* end of chain*/
            exit(0);
        }

        printf("prime %d\n", n);

        if (pipe(fds) < 0) {
            fprintf(2, "Unable to create pipes!\n");
            exit(-1);
        }

        if (fork() == 0) {
            /* child process */
            /* close read_fd of grandparent */
            close(parent_read_fd);
            /* back to start of loop*/
            continue;
        }

        /* close read-end of child */

        close(fds[0]);

        /* sieve all numbers */
        while (read(parent_read_fd, &x, sizeof(x)) > 0) {
            if (x % n != 0) {
                write(fds[1], &x, sizeof(x));
            }
        }

        close(fds[1]);
        close(parent_read_fd);

        /* wait for child to finish */
        wait((void*)0);
        exit(0);
    }

    return 0;
}