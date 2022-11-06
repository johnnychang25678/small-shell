#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

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
  int argCount; // the total arg count, include the command
  char *command;
  char* *args;
} parseResult;

parseResult* parseInput(char* input){
  // split input by space (strok)
  parseResult* result = malloc(4096);
  const char* expandSymbol = "$$";
  const int expSymbolLen = strlen(expandSymbol);

  // remove the trailing new line character
  input[strcspn(input, "\n")] = 0;

  int counter = 0;
  char **args = malloc(sizeof(char*)); // [char*, char*, ...]
  char *token = strtok(input, PARSE_DELIMITER);
  while (token) {
    char *needle = strstr(token, "$$");
    if (needle != NULL) {
      // replace $$ with pid string
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
      if (counter >= 1) {
        args = realloc(args, counter * sizeof(char*)); 
      }
      args[counter - 1] = malloc(sizeof(char) * (strlen(token) + 1));
      strcpy(args[counter-1], token);
      // printf("index: %d, element: %s\n", counter - 1, token);
      
    }
    
    counter++;
    if (needle != NULL) free(token); // free if we have called replaceString for $$
    token = strtok(NULL, PARSE_DELIMITER);
  }
  result->argCount = counter; 
  result->args = args;

  printf("result.command %s\n", result->command);
  int i;
  for (i = 0; i < counter - 1; i++) {
    printf("index: %d, arg: %s\n", i, result->args[i]);
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

void handleOtherCommand(parseResult* result) {
  char* command = result->command;
  // fork a child
  pid_t spawnPid = fork();
  int childStatus;

  int argCount = result->argCount;
  char **argv = malloc(sizeof(char*) * (argCount + 1)); // +1 is for NULL

  switch(spawnPid){
  case -1: {
    perror("fork error\n");
    exit(1);
    break;
  }
  case 0: {
    // in child process
    char* cmd;
    asprintf(&cmd, "%s%s", "/bin/", command);
    // an array of {cmd, ...args, NULL}
    argv[0] = cmd;
    if (argCount > 1) {
      int i;
      for (i = 0; i < argCount; i++){
        if (i == argCount - 1) {
          argv[i+1] = NULL; 
        } else {
          argv[i+1] = result->args[i];
        }
      }
    }

    execv(argv[0], argv);
    perror("execv error\n");
    exit(1);
    break;
  }
  default:
    // in parent process, wait for child termination
    waitpid(spawnPid, &childStatus, 0);
    printf("after wait...\n");
    free(argv);
  }
}

int main(){
  while (1) {
    printf("Hello world\n"); 
    fflush(stdout);
    char *line = NULL;
    size_t len = 0;

    // parseResult result;
    getline(&line, &len, stdin);
    if (line[0] != ':') continue;
    if (line[1] != ' ') continue;
    char *lineCopy;
    lineCopy = line + 2; 
    parseResult* result = NULL;
    result = parseInput(lineCopy);
    // TODO: do stuff with commands and args
    char* command = result->command;
    if (strcmp(command, "exit") == 0 || strcmp(command, "cd") == 0 || strcmp(command, "status") == 0) {
      handleBuiltInCommand(result);
    }

    // handle non-builtin
    handleOtherCommand(result);


    // clean up the memory
    free(result->command);
    for (int i = 0; i < result->argCount - 1; i++) {
      free(result->args[i]);
    }
    free(result);
    free(line);
  }
}

