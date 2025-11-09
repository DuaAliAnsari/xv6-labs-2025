#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  pause(0);  // Pass 0 as argument
  exit(0);
}
