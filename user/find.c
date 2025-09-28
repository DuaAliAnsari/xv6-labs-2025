#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user/user.h"


void find(char *path, char *target, char **exec_argv) {
    char buf[512];          //buffer to build paths
    char *p;                //pointer for path manipulation
    int fd;                 //file descriptor
    struct dirent de;       //directory entry (name+ inode number)
    struct stat st;         //file metadata (type,size etc.)

    fd = open(path, 0);
    if (fd < 0) {
        printf("find: cannot open %s\n", path);
        return;
    }

     if (fstat(fd, &st) < 0) {
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
        case T_FILE: 
            //extract the filename from the full path
            p = path;
            while (*p) p++;                
            while (p > path && *(p - 1) != '/') p--; //backtrack to last '/'

            
            if (strcmp(p, target) == 0) {
                if (exec_argv) { //if -exec option is given
                    int pid = fork();
                    if (pid == 0) { //child process
                        char *argv[MAXARG];
                        int i = 0;
                        while (exec_argv[i]) {
                            argv[i] = exec_argv[i];
                            i++;
                        }
                        argv[i++] = path;   //append filename to args
                        argv[i] = 0;        
                        exec(argv[0], argv); 
                        printf("find: exec failed\n");
                        exit(1);
                    } else {
                        wait(0);                     }
                } else {
                    //default:print path of matching file
                    printf("%s\n", path);
                }
            }
            break;

        case T_DIR: // directory
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
                printf("find: path too long\n");
                break;
            }

            //copy path into buffer
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';

            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) continue; 
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue; //skip "." and ".."
                int i;
                for (i = 0; i < DIRSIZ && de.name[i]; i++) 
                    p[i] = de.name[i];
                p[i] = 0; 
                find(buf, target, exec_argv);
            }
            break;
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: find path name [-exec cmd args...]\n");
        exit(1);
    }

    char **exec_argv = 0;
//parse arguments for -exec
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-exec") == 0) {
            if (i + 1 >= argc) {
                printf("Usage: find path name [-exec cmd args...]\n");
                exit(1);
            }
            exec_argv = &argv[i + 1]; //save pointer to command args
            break;
        }
    }
    //recursive search
    find(argv[1], argv[2], exec_argv);
    exit(0);
}
