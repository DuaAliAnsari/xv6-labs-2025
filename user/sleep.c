#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int ticks;
  int start_time, current_time;

  // Check if user provided an argument
  if(argc != 2){
    fprintf(2, "Usage: sleep <ticks>\n");
    exit(1);
  }

  // Convert string argument to integer
  ticks = atoi(argv[1]);

  // Get starting time using uptime system call
  start_time = uptime();
  
  // Busy wait until the specified ticks have passed
  while(1) {
    current_time = uptime();
    if(current_time - start_time >= ticks) {
      break;
    }
  }

  // Exit successfully
  exit(0);
}


