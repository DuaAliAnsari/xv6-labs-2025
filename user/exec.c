#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int exec_mode = 0;
char *exec_cmd = 0;
char *exec_arg = 0;

void find(char *path, char *name) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE:
        // Extract filename from path and check if it matches
        p = path + strlen(path);
        while (p > path && *(p-1) != '/')
            p--;
        if (strcmp(p, name) == 0) {
            if (exec_mode) {
                // Execute command with the file path
                if (fork() == 0) {
                    // Child process
                    char *argv[] = {exec_cmd, exec_arg, path, 0};
                    exec(exec_cmd, argv);
                    fprintf(2, "find: exec %s failed\n", exec_cmd);
                    exit(1);
                } else {
                    // Parent process - wait for child to complete
                    wait(0);
                }
            } else {
                printf("%s\n", path);
            }
        }
        break;

    case T_DIR:
        // If path is too long, skip
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf("find: path too long\n");
            break;
        }
        
        // Copy path to buffer and prepare for appending
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        
        // Read directory entries
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            
            // Skip . and ..
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            
            // Create full path for this entry
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            
            // Check if this file matches our target name
            p = buf + strlen(buf);
            while (p > buf && *(p-1) != '/')
                p--;
            if (strcmp(p, name) == 0) {
                if (exec_mode) {
                    // Execute command with the file path
                    if (fork() == 0) {
                        // Child process
                        char *argv[] = {exec_cmd, exec_arg, buf, 0};
                        exec(exec_cmd, argv);
                        fprintf(2, "find: exec %s failed\n", exec_cmd);
                        exit(1);
                    } else {
                        // Parent process - wait for child to complete
                        wait(0);
                    }
                } else {
                    printf("%s\n", buf);
                }
            }
            
            // If it's a directory, recurse
            if (stat(buf, &st) == 0 && st.type == T_DIR) {
                find(buf, name);
            }
        }
        break;
    }
    
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(2, "Usage: find <path> <name> [-exec <cmd> <arg>]\n");
        exit(1);
    }
    
    // Check for -exec option
    if (argc >= 5 && strcmp(argv[3], "-exec") == 0) {
        exec_mode = 1;
        exec_cmd = argv[4];
        if (argc >= 6) {
            exec_arg = argv[5];
        }
    }
    
    find(argv[1], argv[2]);
    exit(0);
}
