// Enhanced Shell with Tab Completion, Wait, and Batch Mode

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5
#define MAXARGS 10

// Global variables
static int batch_mode = 0;  // Flag for batch processing

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);
void panic(char*);
struct cmd *parsecmd(char*);
void runcmd(struct cmd*) __attribute__((noreturn));

// Helper functions
int
mystrlen(char *s)
{
  int n;
  for(n = 0; s[n]; n++)
    ;
  return n;
}

void
mystrcpy(char *dst, char *src)
{
  while((*dst++ = *src++) != 0)
    ;
}

int
mystrcmp(char *s1, char *s2)
{
  while(*s1 && *s1 == *s2)
    s1++, s2++;
  return (unsigned char)*s1 - (unsigned char)*s2;
}

// Tab completion function
void
tab_complete(char *buf, int pos)
{
  char *commands[] = {"ls", "cat", "grep", "find", "sleep", "echo", "mkdir", 
                      "rm", "cd", "pwd", "wc", "cp", "mv", "chmod", "wait", 0};
  char partial[50];
  int i, j, matches = 0;
  int match_idx = -1;
  int start_pos;
  
  // Find start of current word (go backwards from cursor)
  start_pos = pos - 1;
  while(start_pos >= 0 && buf[start_pos] != ' ' && buf[start_pos] != '\t')
    start_pos--;
  start_pos++;
  
  // Extract partial command
  int partial_len = pos - start_pos;
  if(partial_len >= 50 || partial_len == 0) return;
  
  for(i = 0; i < partial_len; i++)
    partial[i] = buf[start_pos + i];
  partial[partial_len] = 0;
  
  // Count matches
  for(i = 0; commands[i]; i++) {
    int match = 1;
    // Check if command starts with partial
    for(j = 0; j < partial_len; j++) {
      if(!commands[i][j] || commands[i][j] != partial[j]) {
        match = 0;
        break;
      }
    }
    if(match) {
      matches++;
      match_idx = i;
      if(matches == 1) {
        printf("\n"); // New line before showing completion
      }
      if(matches <= 10) { // Limit output
        printf("%s  ", commands[i]);
      }
    }
  }
  
  if(matches > 0) {
    printf("\n$ %s", buf); // Redisplay prompt and current line
  }
  
  // If exactly one match, auto-complete
  if(matches == 1) {
    char *cmd = commands[match_idx];
    int cmd_len = mystrlen(cmd);
    
    // Clear the buffer from start_pos and insert full command
    for(i = start_pos; i < start_pos + cmd_len; i++) {
      buf[i] = cmd[i - start_pos];
    }
    buf[start_pos + cmd_len] = ' '; // Add space after command
    buf[start_pos + cmd_len + 1] = 0; // Null terminate
  }
}

// Enhanced getcmd with tab completion
int
getcmd(char *buf, int nbuf)
{
  int pos = 0;
  char c;
  
  // Only print $ prompt in interactive mode
  if(!batch_mode) {
    write(2, "$ ", 2);
  }
  
  memset(buf, 0, nbuf);
  
  // If in batch mode, use simple gets
  if(batch_mode) {
    gets(buf, nbuf);
    if(buf[0] == 0) // EOF
      return -1;
    return 0;
  }
  
  // Interactive mode with tab completion
  while(pos < nbuf - 1) {
    if(read(0, &c, 1) != 1) {
      return -1; // EOF
    }
    
    if(c == '\t') {
      // Tab completion
      tab_complete(buf, pos);
      continue;
    } else if(c == '\n') {
      buf[pos] = c;
      buf[pos + 1] = 0;
      write(2, &c, 1); // Echo newline
      return 0;
    } else if(c == 127 || c == '\b') {
      // Backspace
      if(pos > 0) {
        pos--;
        buf[pos] = 0;
        write(2, "\b \b", 3); // Erase character on screen
      }
    } else if(c >= 32) {
      // Printable character
      buf[pos] = c;
      pos++;
      write(2, &c, 1); // Echo character
    }
  }
  
  buf[pos] = 0;
  return 0;
}

// Check for built-in commands
int
handle_builtin(char *cmd)
{
  // Skip whitespace
  while(*cmd == ' ' || *cmd == '\t')
    cmd++;
  
  // Check for "wait" command
  if(cmd[0] == 'w' && cmd[1] == 'a' && cmd[2] == 'i' && cmd[3] == 't' &&
     (cmd[4] == 0 || cmd[4] == '\n' || cmd[4] == ' ')) {
    wait(0);
    return 1;
  }
  
  return 0;
}

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    printf("exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      printf("open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  static char buf[100];
  int fd;

  // Check if we're in batch mode (reading from file)
  if(argc > 1) {
    batch_mode = 1;
    fd = open(argv[1], O_RDONLY);
    if(fd < 0) {
      printf("cannot open %s\n", argv[1]);
      exit(1);
    }
    // Redirect stdin to the file
    close(0);
    dup(fd);
    close(fd);
  }

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    char *cmd = buf;
    
    // Skip leading whitespace
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
      
    if (*cmd == '\n' || *cmd == 0) // is a blank command
      continue;
    
    // Handle built-in commands
    if(handle_builtin(cmd))
      continue;
      
    if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' '){
      // Chdir must be called by the parent, not the child.
      cmd[mystrlen(cmd)-1] = 0;  // chop \n
      if(chdir(cmd+3) < 0)
        printf("cannot cd %s\n", cmd+3);
    } else {
      if(fork1() == 0)
        runcmd(parsecmd(cmd));
      wait(0);
    }
  }
  exit(0);
}

void
panic(char *s)
{
  printf("%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;
  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

// Keep all the original parsing code
struct cmd*
execcmd(void)
{
  struct execcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;
  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;
  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    printf("leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;
  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;
  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  if(peek(ps, es, "("))
    return parseblock(ps, es);
  ret = execcmd();
  cmd = (struct execcmd*)ret;
  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;
  if(cmd == 0)
    return 0;
  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;
  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;
  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;
  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;
  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
