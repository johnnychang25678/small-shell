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
#include <errno.h>
#include <signal.h>

#define PARSE_DELIMITER " "

// global vars
volatile sig_atomic_t isForeGroundOnlyMode = 0;
int lastForeGroundStatus = 0;

typedef struct parseResult {
  char *command;
  char **args;
  char **redirects;
  char **fileNames;
  bool isForeGround;
  int argCount; // the total arg count, include the command
  int fileCount;
  int redirectCount;
} parseResult;

// a helper function to replace chars in a  string
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

// parst the input from the user
parseResult* parseInput(char* input){
  parseResult* result = malloc(4096);

  // remove the trailing new line character
  input[strcspn(input, "\n")] = 0;

  int counter = 0;
  int fileCount = 0;
  int redirectCount = 0;
  char **args = malloc(sizeof(char*)); 
  char **fileNames = malloc(sizeof(char*)); 
  char **redirects = malloc(sizeof(char*));
  bool isFile = false;

  // split input by space (strok)
  char *token = strtok(input, PARSE_DELIMITER);   
  
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
  // but if isForeGroundOnlyMode = true, we set to false to ignore the bg process
  if (counter >= 2) {
    char* lastArg = args[counter - 2];
    if (strcmp(lastArg, "&") == 0) {
      result->argCount--;
      lastArg = NULL;
      if (isForeGroundOnlyMode == 0) {
        result->isForeGround = false;
      }
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
    char* path; 
    if (result->argCount == 1) {
      path = getenv("HOME"); 
    } else {
      path = result->args[0];
    }
    chdir(path);
    // char buf[4096];
    // getcwd(buf, 4096);
    // printf("current path: %s\n", buf);
  }
  // The status command prints out either the exit status or 
  // the terminating signal of the last foreground process ran by your shell.
  if (strcmp(command, "status") == 0) {
    printf("exit value %d\n", lastForeGroundStatus);
    fflush(stdout);
  }

}

pid_t spawnPid;

void handleOtherCommand(parseResult* result) {
  char* command = result->command;
  // fork a child
  spawnPid = fork();
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
    // Child processes ignore SIGTSTP
    // Foreground process should have default SIGINT handler
    if (result->isForeGround) {
      struct sigaction defaultSigInt = {0};
      defaultSigInt.sa_handler = SIG_DFL;
      defaultSigInt.sa_flags = 0;
      sigfillset(&defaultSigInt.sa_mask);
      sigaction(SIGINT, &defaultSigInt, NULL);
    }  
    
    struct sigaction ignoreSigTstp = {0};
    ignoreSigTstp.sa_handler = SIG_IGN;
    ignoreSigTstp.sa_flags = 0;
    sigfillset(&ignoreSigTstp.sa_mask);
    sigaction(SIGTSTP, &ignoreSigTstp, NULL);


    // input and output redirects
    int fd;
    int redirectCount = result->redirectCount;

    for (int i = 0; i < redirectCount; i++) {
      char* symbol = result->redirects[i];
      if (strcmp(symbol, ">") == 0) {
        fd = open(result->fileNames[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
          perror("cannot open file");
          exit(1);
        }
        dup2(fd, STDOUT_FILENO);
      } else if (strcmp(symbol, "<") == 0) {
        fd = open(result->fileNames[i], O_RDONLY, 0644);
        if (fd == -1) {
          printf("cannot open %s for input\n", result->fileNames[i]);
          fflush(stdout);
          exit(1);
        }
        dup2(fd, STDIN_FILENO);
      }

    }


    // generate argv to pass in execvp
    argv[0] = command;
    if (argCount == 1) {
      argv[1] = NULL;
    } else if (argCount > 1) {
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
    exit(1);

    break;
  }
  default: 
    // in parent process, wait for child termination
    if (result->isForeGround) {
      waitpid(spawnPid, &childStatus, 0);
      if (WIFEXITED(childStatus)) {
        lastForeGroundStatus = WEXITSTATUS(childStatus);
      } else {
        lastForeGroundStatus = WTERMSIG(childStatus);
      }
    } else {
      // background process will not blocking wait
      printf("background pid is %d\n", spawnPid); 
      fflush(stdout);
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

void sigStspHandler(int sigNo) {
  if (isForeGroundOnlyMode == 0) {
    isForeGroundOnlyMode = 1;
    char *message = "\nEntering foreground-only mode (& is now ignored)\n";
    write(STDOUT_FILENO, message, 51);
  } else {
    isForeGroundOnlyMode = 0;
    char *message = "\nExiting foreground-only mode\n";
    write(STDOUT_FILENO, message, 31);
  }
}


int main(){
  // main process and background process should ignore SIGINT
  struct sigaction ignoreSigInt = {0};
  ignoreSigInt.sa_handler = SIG_IGN;
  ignoreSigInt.sa_flags = 0;
  sigfillset(&ignoreSigInt.sa_mask);
  sigaction(SIGINT, &ignoreSigInt, NULL);

  // SIGTSTP will toggle the "foreground-only mode"
  struct sigaction handleSigTstp = {0};
  handleSigTstp.sa_handler = sigStspHandler;
  handleSigTstp.sa_flags = 0;
  sigfillset(&ignoreSigInt.sa_mask);
  sigaction(SIGTSTP, &handleSigTstp, NULL);
  
  while (1) {
    printf(": "); 
    fflush(NULL);

    char line[2048];
    char * readResult;
    if ((readResult = fgets(line, 2048, stdin)) == NULL) continue;
        

    if (line[0] == '#' || line[0] == '\n' || line[0] == ' ') {
      continue;
    }
    parseResult* result = NULL;
    result = parseInput(line);
    
    char* command = result->command;
    if (strcmp(command, "exit") == 0 || 
        strcmp(command, "cd") == 0 || 
        strcmp(command, "status") == 0) {
      handleBuiltInCommand(result);
    } else {
      // handle non-builtin
      handleOtherCommand(result);
    }

    checkBgProcess();

    // clean up the memory
    free(result->command);
    for (int i = 0; i < result->argCount - 1; i++) {
      free(result->args[i]);
     }
    for (int i = 0; i < result->fileCount; i++) {
      free(result->redirects[i]);
      free(result->fileNames[i]);
    }
    free(result->args);
    free(result->redirects);
    free(result->fileNames);
    free(result);
  }
}

