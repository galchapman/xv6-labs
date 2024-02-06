#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_PATH 256

void visit_directory(int fd, const char* exp, char path[]) {
    struct dirent de;
	struct stat st;
	int fe;
	int path_end = strlen(path);
	int t;

	while (read(fd, &de, sizeof(de)) == sizeof(de)) {
		if(de.inum == 0)
			continue;
		if(de.name[0] == '.' && (de.name[1] == '\0' ||
								(de.name[1] == '.'  && de.name[2] == '\0')))
			continue;

		strcpy(path + path_end, de.name);

		if (strcmp(de.name, exp) == 0) {
			printf("%s\n", path);
		}

		fe = open(de.name, 0);
		if (fe < 0) continue;
		if (fstat(fe, &st) < 0) {
			close(fe);
			continue;
		}

		switch (st.type) {
			case T_DIR:
				chdir(de.name);
				t = strlen(path);
				path[t] = '/';
				path[t+1] = '\0';
				visit_directory(fe, exp, path);
				chdir("..");
				break;
			case T_FILE:
				break;
		}


		close(fe);
	}
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(2, "usage: %s <path> <filename>\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[1], 0);
	struct stat st;

	if (fd < 0) {
		fprintf(2, "find: cannot open %s\n", argv[1]);
		return fd;
	}

	if (fstat(fd, &st) < 0) {
		fprintf(2, "find: cannot stat %s\n", argv[1]);
		close(fd);
		return -1;
	}

	if (st.type != T_DIR) {
		fprintf(2, "find: %s not a directory\n", argv[1]);
		close(fd);
		return 1;
	}

	char path[MAX_PATH] = "";
	strcpy(path, argv[1]);
	path[strlen(path)] = '/';

	visit_directory(fd, argv[2], path);

    return 0;
}