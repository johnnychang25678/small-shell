#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <error.h>

#define PARSE_DELIMITER " "


// replace t 
void replaceString(char* input, char* replaceLocation, 
    int replaceLength, char* replaceTo, int replaceToLength, char* output){
  int firstPartLength = replaceLocation - input;
  strncpy(output, input, firstPartLength);

  char *p = output + firstPartLength; // points to the end of the output
  strcpy(p, replaceTo);

  input = replaceLocation + replaceLength; // move to the last part of the input
  p += replaceToLength; // move to the end of the output
  strcpy(p, input); // copy the last part to result

}


typedef struct parseResult {
  char *command;
  char **args;
  char **redirects;
  char **fileNames;
  char *fileName;
  bool isForeGround;
  int argCount; // the total arg count, include the command
  int fileCount;
  int redirectCount;
} parseResult;

parseResult* parseInput(char* input){
  parseResult* result = malloc(4096);

  // remove the trailing new line character
  input[strcspn(input, "\n")] = 0;

  int counter = 0;
  int fileCount = 0;
  int redirectCount = 0;
  char **args = malloc(sizeof(char*)); // [char*, char*, ...]
  char **fileNames = malloc(sizeof(char*)); // [char*, char*, ...]
  char **redirects = malloc(sizeof(char*)); // [char*, char*, ...]
  bool isFile = false;

  char *token = strtok(input, PARSE_DELIMITER); // split input by space (strok)
  
  while (token) {
    char *needle = strstr(token, "$$");
    if (needle != NULL) {
      // replace $$ with pid string
      const char* expandSymbol = "$$";
      const int expSymbolLen = strlen(expandSymbol);
      pid_t pid = getpid();
      char pidString[10];
      sprintf(pidString, "%d", pid);
      int outLen = strlen(token) - expSymbolLen + strlen(pidString) + 1;
      char output[outLen];

      replaceString(token, needle, expSymbolLen, pidString, strlen(pidString), output);
      token = malloc(sizeof(char) * outLen);
      strcpy(token, output);
    }
    
    if (counter == 0) {
       result->command = malloc(sizeof(char) * (strlen(token) + 1));
       strcpy(result->command, token);
    } else {
      if (strcmp(token, ">") == 0 || strcmp(token, "<") == 0) {
        redirectCount++;
        if (redirectCount > 1) {
          redirects = realloc(redirects, redirectCount * sizeof(char*));
        }
        redirects[redirectCount-1] = malloc(sizeof(char)+1); // only store "<" or ">"
        strcpy(redirects[redirectCount - 1], token);
        isFile = false;
      }

      if (isFile) {
        fileCount++;
        if (fileCount > 1) {
          fileNames = realloc(fileNames, fileCount * sizeof(char*)); 
        }
        fileNames[fileCount-1] = malloc(sizeof(char) * (strlen(token)+1));
        strcpy(fileNames[fileCount - 1], token);
        result->fileName = token;
        isFile = false;
      }
      
      if (redirectCount > 0) {
        // next argument is file, so we set isFile = true, then conitune
        isFile = true; 
        counter--;
      } else {
        if (counter >= 1) {
          args = realloc(args, counter * sizeof(char*)); 
        }
        args[counter - 1] = malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(args[counter-1], token);
      }
    }
    
    counter++;
    if (needle != NULL) free(token); // free if we have called replaceString for $$
    token = strtok(NULL, PARSE_DELIMITER);
  }
  result->argCount = counter; 
  result->fileCount = fileCount;
  result->redirectCount = redirectCount;
  result->args = args;
  result->fileNames = fileNames;
  result->redirects = redirects;
  result->isForeGround = true;

  // if last arg input is "&", the process should run in background
  // we need to remove it from args and set isForeGround to false
  if (counter >= 2) {
    char* lastArg = args[counter - 2];
    if (strcmp(lastArg, "&") == 0) {
      result->isForeGround = false;
      result->argCount--;
      lastArg = NULL;
    } 
  } 

  return result;
}

void handleBuiltInCommand(parseResult* result) {
  char* command = result->command;
  // handle exit
  if (strcmp(command, "exit") == 0) {
    exit(0);
  }
  // handle cd
  if (strcmp(command, "cd") == 0) {
    char* path = result->args[0];
    if (path == NULL) {
      path = getenv("HOME");
    }
    chdir(path);
    char buf[4096];
    getcwd(buf, 4096);
    printf("current path: %s\n", buf);
  }
  // TODO: handle status
}

void myHandler(int sigNo) {
    pid_t result;
    int status;
    if (sigNo != 1) {
    }
    while((result = waitpid(-1, &status, WNOHANG)) > 0) {
      printf("aaa");
      sleep(1);
    };
}

void handleOtherCommand(parseResult* result) {
  char* command = result->command;
  // fork a child
  pid_t spawnPid = fork();
  int childStatus;
  int argCount = result->argCount;
  char **argv = malloc(sizeof(char*) * (argCount + 1)); // +1 is for NULL


  switch(spawnPid) {
  case -1: {
    perror("fork error\n");
    free(argv);
    exit(1);
    break;
  }
  case 0: {
    // in child process
    int fd;
    // redirectCount should == fileCount
    int redirectCount = result->redirectCount;

    for (int i = 0; i < redirectCount; i++) {
      char* symbol = result->redirects[i];
      if (strcmp(symbol, ">") == 0) {
        fd = open(result->fileNames[i], O_WRONLY | O_CREAT, 0644);
        if (fd == -1) {
          perror("cannot open file");
          exit(1);
        }
        dup2(fd, STDOUT_FILENO);
      } else if (strcmp(symbol, "<") == 0) {
        fd = open(result->fileNames[i], O_RDONLY, 0644);
        if (fd == -1) {
          perror("cannot open file");
          exit(1);
        }
        dup2(fd, STDIN_FILENO);
      }

    }


    // generate argv to pass in execvp
    argv[0] = command;
    if (argCount > 1) {
      int i;
      for (i = 0; i < argCount; i++){
        if (i == argCount - 1) {
          argv[i+1] = NULL; // last argument is NULL for argv
        } else {
          argv[i+1] = result->args[i];
        }
      }
    }
    

    execvp(command, argv);
    perror(command);
    fflush(stdout);
    free(argv);
    exit(1);

    break;
  }
  default: 
    // in parent process, wait for child termination
    if (result->isForeGround == true) {
      waitpid(spawnPid, &childStatus, 0);
    } else {
      printf("background pid is %d\n", spawnPid); 
      waitpid(spawnPid, &childStatus, WNOHANG);
    }
    free(argv);
   
  }

}

void checkBgProcess() {
    pid_t backgroundPid;
    int status;

    while((backgroundPid = waitpid(-1, &status, WNOHANG)) > 0) {
      printf("background pid %d is done: ", backgroundPid);
      fflush(stdout);
      if (WIFEXITED(status)) {
        printf("exit value %d\n", WEXITSTATUS(status));
      } else {
        printf("terminated by signal %d\n", WTERMSIG(status));
      }
      fflush(stdout);
    }
}


int main(){
  while (1) {
    checkBgProcess();
    printf(": "); 
    char *line = NULL;
    size_t len = 0;

    getline(&line, &len, stdin);

    if (line[0] == '#' || line[0] == '\n' || line[0] == ' ') {
      free(line);
      continue;
    }
    parseResult* result = NULL;
    result = parseInput(line);
    // TODO: do stuff with commands and args
    
    char* command = result->command;
    if (strcmp(command, "exit") == 0 || strcmp(command, "cd") == 0 || strcmp(command, "status") == 0) {
      handleBuiltInCommand(result);
    }

    // handle non-builtin
    handleOtherCommand(result);

    // clean up the memory
    // TODO: clean up redirects and files
    free(result->command);
    for (int i = 0; i < result->argCount - 1; i++) {
      free(result->args[i]);
    }
    for (int i = 0; i < result->fileCount; i++) {
      free(result->redirects[i]);
      free(result->fileNames[i]);
    }
    free(result);
    free(line);
  }
}

