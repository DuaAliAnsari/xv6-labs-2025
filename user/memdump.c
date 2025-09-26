#include "kernel/types.h"
#include "user/user.h"

#define NULL 0   // since xv6 doesn't have stddef.h

void memdump(char *fmt, char *data) {
    while (*fmt) {
        switch (*fmt) {
        case 'i': {
            int val = *(int *)data;
            printf("%d\n", val);
            data += sizeof(int);
            break;
        }
        case 'p': {
            // xv6 printf can't handle %llx, so split into two 32-bit parts
            uint low = *(uint *)data;
            uint high = *((uint *)data + 1);
            printf("%x%x\n", high, low);
            data += 8;
            break;
        }
        case 'h': {
            short val = *(short *)data;
            printf("%d\n", val);
            data += sizeof(short);
            break;
        }
        case 'c': {
            char val = *data;
            printf("%c\n", val);
            data += 1;
            break;
        }
        case 's': {
            char *str = *(char **)data;
            if (str)
                printf("%s\n", str);
            else
                printf("(null)\n");
            data += sizeof(char *);
            break;
        }
        case 'S': {
            printf("%s\n", data);
            return; // done
        }
        default:
            printf("Unknown format: %c\n", *fmt);
            return;
        }
        fmt++;
    }
}

int
main(int argc, char *argv[])
{
    if(argc == 1) {
        // Example 1
        int x = 61810;
        short y = 2025;
        printf("Example 1:\n");
        memdump("i", (char *)&x);
        memdump("h", (char *)&y);

        // Example 2
        char *str1 = "a string";
        printf("Example 2:\n");
        memdump("s", (char *)&str1);

        // Example 3
        char *str2 = "another";
        printf("Example 3:\n");
        memdump("s", (char *)&str2);

        // Example 4
        struct {
            char c;
            int i;
            short h;
            char z;
            char *str;
        } example4 = { 'B', 1819438967, 100, 'z', "xyzzy" };

        printf("Example 4:\n");
        memdump("cihc s", (char *)&example4);

        // Example 5
        char *str5 = "hello\nworld";
        printf("Example 5:\n");
        memdump("S", str5);

    } else {
        char buf[512];
        int n = read(0, buf, sizeof(buf)-1);
        if(n > 0) {
            buf[n] = '\0';
            memdump(argv[1], buf);
        }
    }
    exit(0);
}
