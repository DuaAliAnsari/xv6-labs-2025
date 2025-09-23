#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// delimiters
char *seps = " -\r\t\n/,";

int is_sep(char c) {
  for (char *p = seps; *p; p++) {
    if (c == *p) return 1;
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sixfive <file>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDONLY);
  if(fd < 0){
    fprintf(2, "sixfive: cannot open %s\n", argv[1]);
    exit(1);
  }

  char buf[1];   // read one char at a time
  char numbuf[64]; // buffer to build number
  int n, idx = 0;

  while((n = read(fd, buf, 1)) > 0){
    char c = buf[0];
    if(is_sep(c)){
      if(idx > 0){
        numbuf[idx] = '\0';
        int val = atoi(numbuf);
        if(val % 5 == 0 || val % 6 == 0){
          printf("%d\n", val);
        }
        idx = 0; // reset buffer
      }
    } else {
      if(idx < sizeof(numbuf) - 1){
        numbuf[idx++] = c;
      }
    }
  }

  // handle last number at EOF
  if(idx > 0){
    numbuf[idx] = '\0';
    int val = atoi(numbuf);
    if(val % 5 == 0 || val % 6 == 0){
      printf("%d\n", val);
    }
  }

  close(fd);
  exit(0);
}
