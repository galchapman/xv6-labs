#include "kernel/types.h"
#include "user/user.h"

#define STDERR 2

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(STDERR, "usage: %s <seconds>\n", argv[0]);
    }

    sleep(atoi(argv[1]));

    exit(0);
}