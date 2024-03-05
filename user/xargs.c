#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define BUFFER_SIZE 256

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(2, "usage: %s <command> <arguments ...>\n", argv[0]);
        exit(1);
    }


	char buffer[BUFFER_SIZE];
	char *args[MAXARG] = { 0 };
    uint arg_count = 0;

    /* copy initiale arguments */
    for (int i = 1; i < argc; ++i) {
        args[i-1] = argv[i];
    }

    for (;;) {
        /* argument count is the same as xargs arg count minus xrags */
        arg_count = argc - 1;

        gets(buffer, sizeof(buffer));

        /* remove \n \r from end of buffer */
        uint buffer_len = strlen(buffer);

        if (buffer_len == 0) {
            break;
        }

        if (buffer[buffer_len-1] == '\n' || buffer[buffer_len-1] == '\r') {
            buffer[buffer_len-1] = '\0';
            --buffer_len;
        }

        /* parse arguments */

        for (char* p = buffer, *s = buffer; ; ++p) {
            if (*p == ' ' || *p == '\0') {
                /* add argument */
                args[arg_count] = s;
                ++arg_count;
                s = p + 1;
                if (*p == '\0') {
                    /* end of string */
                    break;
                }
                *p = '\0';
            }

        }

		/* set last argument to be NULL */
		args[arg_count] = NULL;

        if (fork() == 0) {
            /* call exec */
            exec(args[0], args);
            /* only if exec faild */
            fprintf(2, "exec faild!\n");
            /* print exec call */
            fprintf(2, "exec(%s", args[0]);
            for (int i = 1; i < arg_count; ++i) {
                fprintf(2, ", %s", args[i]);
            }
            fprintf(2, ")\n");
            exit(-1);
        }
        /* wait for child */
        wait((void*)0);
    }

	return 0;
}