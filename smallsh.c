#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

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


struct parseResult {
  char *command;
  char* *args;
};

void parseInput(char* input, ssize_t length, struct parseResult *result){
  // split input by space (strok)
  const char* expandSymbol = "$$";
  const int expSymbolLen = strlen(expandSymbol);

  char inputWithoutNewLine[length];
  strncpy(inputWithoutNewLine, input, length);

  int counter = 0;
  char* *ar;
  char *token = strtok(inputWithoutNewLine, PARSE_DELIMITER);
  while (token) {
    char *needle = strstr(token, "$$");
    if (needle != NULL) {
      pid_t pid = getpid();
      printf("pid: %d\n", pid);
      char pidString[10];
      sprintf(pidString, "%d", pid);
      int outLen = strlen(token) - expSymbolLen + strlen(pidString) + 1;
      char output[outLen];

      replaceString(token, needle, expSymbolLen, pidString, strlen(pidString), output);
      printf("%s\n", output);
      token = output;
    }
    
    if (counter == 0) {
       result->command = token; 
    } else {
      // TODO
    }

    token = strtok(NULL, PARSE_DELIMITER);
  }
  // TODO, scan output to a struct so we can do other stuff based on command and args
  printf("%s\n", result->command);
}

int main(){
  while (1) {
    printf("Hello world\n"); 
    fflush(stdout);
    char *line = NULL;
    size_t len = 0;
    ssize_t lineSize;

    struct parseResult result;
    lineSize = getline(&line, &len, stdin);
    parseInput(line, lineSize - 1, &result);
    free(line);
  }
}

