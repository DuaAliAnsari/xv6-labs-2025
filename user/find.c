#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//regular expression matching functions from grep.c
int matchhere(char*, char*);
int matchstar(int, char*, char*);

int match(char *re, char *text) {
  if(re[0] == '^')
    return matchhere(re+1, text);
  do {
    if(matchhere(re, text))
      return 1;
  } while(*text++ != '\0');
  return 0;
}

int matchhere(char *re, char *text) {
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

int matchstar(int c, char *re, char *text) {
  do {
    if(matchhere(re, text))
      return 1;
  } while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

void find(char *path, char *pattern) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  if ((fd = open(path, 0)) < 0) {
    return;
  }
  
  if (fstat(fd, &st) < 0) {
    close(fd);
    return;
  }
  
  if (st.type == T_FILE) {
    //extract filename from path
    char *fname = path + strlen(path);
    while (fname > path && fname[-1] != '/')
      fname--;
    if (match(pattern, fname))
      printf("%s\n", path);
    close(fd);
    return;
  }
  
  if (st.type != T_DIR) {
    close(fd);
    return;
  }
  
  if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
    close(fd);
    return;
  }
  
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';
  
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    char *name = p;
    
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;
    
    if (stat(buf, &st) < 0)
      continue;
    
    //check if filename matches the regex pattern
    if (match(pattern, name))
      printf("%s\n", buf);
    
//recursively search subdirectories
    if (st.type == T_DIR)
      find(buf, pattern);
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: find <path> <pattern>\n");
    exit(1);
  }
  
  find(argv[1], argv[2]);
  exit(0);
}
