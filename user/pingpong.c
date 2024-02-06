#include "kernel/types.h"
#include "user/user.h"

int main(void) {
    int fds[2];
    char byte_buffer;

    if (pipe(fds) < 0) {
        printf("unable to create pipe!\n");
        exit(-1);
    }

    if (fork() != 0) {
        /* parent */
        byte_buffer = 'I';
        write(fds[1], &byte_buffer, sizeof byte_buffer);
        close(fds[1]);
        read(fds[0], &byte_buffer, sizeof byte_buffer);
        close(fds[0]);
        printf("%d: received pong\n", getpid());
        wait((void*)0);
    } else {
        /* child */
        read(fds[0], &byte_buffer, sizeof byte_buffer);
        printf("%d: received ping\n", getpid());
        close(fds[0]);
        byte_buffer = 'J';
        write(fds[1], &byte_buffer, sizeof byte_buffer);
        close(fds[1]);
    }
    return 0;
}