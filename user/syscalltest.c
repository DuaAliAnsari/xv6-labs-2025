#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
int initial_count, final_count;
int pid;
initial_count = getsyscallcount();
printf("initial syscall count: %d\n", initial_count);
printf("making system calls...\n");
pid = getpid();
printf("process PID: %d\n", pid);
int child_pid = fork();
if (child_pid == 0) {
printf("child process created\n");
getpid();
int child_count = getsyscallcount();
printf("child syscall count: %d\n", child_count);
exit(0);} 
else {
//parent process
wait(0);
//get final syscall count
final_count = getsyscallcount();
printf("Final syscall count: %d\n", final_count);
printf("System calls made: %d\n", final_count - initial_count);}
 exit(0);
}
