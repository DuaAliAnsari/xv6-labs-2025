#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("Printing kernel page table:\n");
  kpgtbl();
  printf("\nDone!\n");
  exit(0);
}
