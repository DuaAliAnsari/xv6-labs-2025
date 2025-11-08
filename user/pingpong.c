#include "kernel/types.h"
#include "user/user.h"

int
main()
{
int p2c[2];
int c2p[2];
char buf[1];

pipe(p2c);
pipe(c2p);

int pid = fork();

if (pid < 0) {
printf("fork has failed\n");
exit(1);}

if (pid == 0) {
close(p2c[1]);
close(c2p[0]);

for (int i = 0; i < 10; i++) {
read(p2c[0], buf, 1);
printf("Child: received ping %d\n", i + 1);
buf[0] = 'c';
write(c2p[1], buf, 1);}

close(p2c[0]);
close(c2p[1]);

exit(0);}

close(p2c[0]);
close(c2p[1]);

for (int i = 0; i < 10; i++) {
buf[0] = 'p';
write(p2c[1], buf, 1);
printf("Parent: sent ping %d\n", i + 1);

read(c2p[0], buf, 1);
printf("Parent: received pong %d\n", i + 1);}

close(p2c[1]);
close(c2p[0]);

wait(0);
exit(0);
}
