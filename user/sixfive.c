#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int char_to_digit(char c) {
    return c - '0';
}
void process_file(int fd) {
    char buf[512];
    int n, i;
    int current_number = 0;
    int has_number = 0;
    
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (i = 0; i < n; i++) {
            char c = buf[i];           
            if (is_digit(c)) {
                current_number = current_number * 10 + char_to_digit(c);
                has_number = 1;
            } else {
                if (has_number) {
                    //check if it's a multiple of 5
                    if (current_number % 5 == 0) {
                        printf("%d\n", current_number);
                    }
                    //reset for next number
                    current_number = 0;
                    has_number = 0;
                }
            }
        }
    }
    
    //handle case where file ends with a number 
    if (has_number) {
        if (current_number % 5 == 0) {
            printf("%d\n", current_number);
        }
    }
}

int main(int argc, char *argv[]) {
    int fd, i;
    
    if (argc <= 1) {
        process_file(0);
    } else {
            for (i = 1; i < argc; i++) {
            if ((fd = open(argv[i], 0)) < 0) {
                fprintf(2, "sixfive: cannot open %s\n", argv[i]);
                exit(1);
            }
            process_file(fd);
            close(fd);
        }
    }
    exit(0);
}
