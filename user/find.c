#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

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
        //if it's a file, check if the name matches
        if (strcmp(path + strlen(path) - strlen(name), name) == 0) {
            printf("%s\n", path);
        }
        break;

    case T_DIR:
        //if path is too long, skip
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf("find: path too long\n");
            break;
        }
        
        //copy path to buffer and prepare for appending
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            
            // Create full path for this entry
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
           
            if (strcmp(de.name, name) == 0) {
                printf("%s\n", buf);
            }
            
            //if it's a directory, recurse
            if (stat(buf, &st) == 0 && st.type == T_DIR) {
                find(buf, name);
            }
        }
        break;
    }
    
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: find <path> <name>\n");
        exit(1);
    }
    
    find(argv[1], argv[2]);
    exit(0);
}
