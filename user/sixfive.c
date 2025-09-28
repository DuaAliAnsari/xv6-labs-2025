#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char *seps = " -\r\t\n./,"; //numbers are separated by spaces, tabs, newlines, commas, periods, slashes, and dashes

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sixfive file...\n");
    exit(1);
  }

  for(int i = 1; i < argc; i++){
    int fd = open(argv[i], 0);
    if(fd < 0){
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      exit(1);
    }

    char buf[1];
    char numbuf[32];  //store digits of current number
    int npos = 0;

    while(read(fd, buf, 1) == 1){
      char c = buf[0];

      if(strchr(seps, c)){
        //IF CHAR IS SEP THEN IF NUMBUF HAS DIGITS THEN TERMINATE STRING AND CONVERT TO INT, CHECK DIVISIBILITY WITH 5 0R 6 AND PRINT, RESET npos
//IF CHAR IS DIGIT, APPEND TO numbuf
        if(npos > 0){
          numbuf[npos] = '\0';
          int num = atoi(numbuf);

          if(num % 5 == 0 || num % 6 == 0){
            printf("%d\n", num);
          }

          npos = 0; 
        }
      } else if(c >= '0' && c <= '9'){
        if(npos < sizeof(numbuf)-1){
          numbuf[npos++] = c;
        }
      }
    }

    // handle last number if file doesn’t end with separator
    if(npos > 0){
      numbuf[npos] = '\0';
      int num = atoi(numbuf);
      if(num % 5 == 0 || num % 6 == 0){
        printf("%d\n", num);
      }
    }

    close(fd);
  }

  exit(0);
}
