#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int ticks;
  
  // Call the uptime system call
  ticks = uptime();
  
  // Print the uptime in ticks
  printf("uptime: %d ticks\n", ticks);
  
  exit(0);  // Exit normally
}
