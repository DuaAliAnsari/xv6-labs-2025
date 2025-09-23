#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void memdump(char *fmt, char *data) {
  while (*fmt) {
    switch (*fmt) {
      case 'i': {
        int val = *(int *)data;
        printf(" %d\n", val);
        data += sizeof(int);
        break;
      }
      case 'x': {
        long long val = *(long long *)data;
        printf(" %llx\n", val);
        data += sizeof(long long);
        break;
      }
      case 'd': {
        short val = *(short *)data;
        printf(" %d\n", val);
        data += sizeof(short);
        break;
      }
      case 'c': {
        char val = *data;
        printf(" %c\n", val);
        data += sizeof(char);
        break;
      }
      case 'p': {
        char *ptr = *(char **)data;
        printf(" %s\n", ptr);
        data += sizeof(char *);
        break;
      }
      case 's': {
        printf(" %s\n", data);
        data += strlen(data) + 1; // skip string
        break;
      }
    }
    fmt++;
  }
}

int
main(int argc, char *argv[])
{
  int a = 25000;
  short b = 20;
  char c = 'Z';
  char *str = "hello xv6";

  memdump("i", (char *)&a);
  memdump("d", (char *)&b);
  memdump("c", (char *)&c);
  memdump("s", str);

  exit(0);
}
