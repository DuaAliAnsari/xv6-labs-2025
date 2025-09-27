#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// Simple string matching - checks if pattern appears anywhere in text
int simple_match(char *pattern, char *text) {
  int i, j;
  int pattern_len = strlen(pattern);
  int text_len = strlen(text);
  
  // Simple substring search
  for(i = 0; i <= text_len - pattern_len; i++) {
    for(j = 0; j < pattern_len; j++) {
      if(text[i + j] != pattern[j])
        break;
    }
    if(j == pattern_len)
      return 1;
  }
  return 0;
}

// Execute command with filename
void run_exec(char **args, int argc, char *filename) {
  char *exec_args[16];
  int i;
  
  // Build argument list, replace {} with filename
  for(i = 0; i < argc && i < 15; i++) {
    if(strcmp(args[i], "{}") == 0) {
      exec_args[i] = filename;
    } else {
      exec_args[i] = args[i];
    }
  }
  exec_args[i] = 0;
  
  // Fork and execute
  if(fork() == 0) {
    exec(exec_args[0], exec_args);
    exit(1);
  }
  wait(0);
}

void find_files(char *path, char *pattern, int use_exec, char **exec_args, int exec_argc) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  if((fd = open(path, 0)) < 0)
    return;
    
  if(fstat(fd, &st) < 0) {
    close(fd);
    return;
  }
  
  // If it's a file, check if it matches
  if(st.type == T_FILE) {
    char *name = path + strlen(path);
    while(name > path && name[-1] != '/')
      name--;
      
    if(simple_match(pattern, name)) {
      if(use_exec) {
        run_exec(exec_args, exec_argc, path);
      } else {
        printf("%s\n", path);
      }
    }
    close(fd);
    return;
  }
  
  // If not a directory, skip
  if(st.type != T_DIR) {
    close(fd);
    return;
  }
  
  // Make sure we have room for path/name
  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
    close(fd);
    return;
  }
  
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';
  
  // Read directory entries
  while(read(fd, &de, sizeof(de)) == sizeof(de)) {
    if(de.inum == 0)
      continue;
      
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    
    // Skip . and ..
    if(strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
      continue;
      
    // Check if name matches pattern
    if(simple_match(pattern, p)) {
      if(use_exec) {
        run_exec(exec_args, exec_argc, buf);
      } else {
        printf("%s\n", buf);
      }
    }
    
    // Recurse into subdirectories
    if(stat(buf, &st) == 0 && st.type == T_DIR) {
      find_files(buf, pattern, use_exec, exec_args, exec_argc);
    }
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  char *exec_args[16];
  int exec_argc = 0;
  int use_exec = 0;
  
  if(argc < 3) {
    printf("Usage: find <path> <pattern> [-exec cmd args {} \\;]\n");
    exit(1);
  }
  
  // Look for -exec
  int exec_pos = -1;
  for(int i = 3; i < argc; i++) {
    if(strcmp(argv[i], "-exec") == 0) {
      exec_pos = i;
      use_exec = 1;
      break;
    }
  }
  
  if(use_exec) {
    // Copy exec arguments
    int j = 0;
    for(int i = exec_pos + 1; i < argc && j < 15; i++) {
      if(strcmp(argv[i], "\\;") == 0)
        break;
      exec_args[j++] = argv[i];
    }
    exec_argc = j;
  }
  
  find_files(argv[1], argv[2], use_exec, exec_args, exec_argc);
  exit(0);
}
