#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]) //argc is no. of args, argv contains program name and first real arg
{
  if(argc < 2){
    fprintf(2, "Usage: sleep ticks\n");
    exit(1);
  }

  int n = atoi(argv[1]);   //convert argument to int
//sleep for n ticks
  if (pause(n) < 0) {
    fprintf(2, "sleep: failed\n");
    exit(1);
  }

  exit(0);
}
