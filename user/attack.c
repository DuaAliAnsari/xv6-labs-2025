#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSZ 4096
#define NPGS 512
#define MAX_SECRET 128
#define MIN_LEN 6

static int is_alnum(char c) {
  if (c >= '0' && c <= '9') return 1;
  if (c >= 'A' && c <= 'Z') return 1;
  if (c >= 'a' && c <= 'z') return 1;
  return 0;
}

int main(void) {
  char *p;
  char buf[MAX_SECRET+1];
  int i, j, k;

  for (int round = 0; round < 2; round++) {
    for (i = 0; i < NPGS; i++) {
      p = sbrk(PGSZ);
      if ((uint64)p == (uint64)-1) break;
      for (j = 0; j < PGSZ; j++) {
        if (!is_alnum(p[j])) continue;
        k = 0;
        while (j + k < PGSZ && k < MAX_SECRET && is_alnum(p[j + k])) {
          buf[k] = p[j + k];
          k++;
        }
        if (k >= MIN_LEN) {
          buf[k] = '\0';
          
          if (strcmp(buf, "secret") != 0 && strcmp(buf, "attack") != 0 && strcmp(buf, "README") != 0) {
            printf("%s\n", buf);
            exit(0);
          }
        }
        j += k;
      }
    }
  }
  exit(0);
}
