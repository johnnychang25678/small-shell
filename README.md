# Small Shell
A shell-like program that implements a subset feature of bash shell with customized signal handlings.

It is built for practicing `C` programming and study the following topics:
- process handling (fork, execv, waitpid...)
- change directory (chdir)
- signal handling (sigaction)
- input and output redirect (dup2)

## Start the program
compile:
`gcc -std=c99 -o smallsh smallsh.c`

run:
`./smallsh`

## Functionality
General syntax:
```
command [arg1 arg2 ...] [< input_file] [> output_file] [&]
```
### The command promopt
- colon `:` symbol is the prompt for each command line
- supports input and ouput rediect with `<` and `>`
- supports background process with `&`
- supports commenting with `#`
- supports variable expansion, `$$` will be replaced with current process PID
### Built-in commands
- `exit`: exit small shell
- `cd`: works like UNIX cd command
- `status`: prints out either the exit status or the terminating signal of the last foreground process ran by your shell.
### Other commands
- All unix commands works as usual. 
### SIGINT and SIGTSTP
#### SIGINT
`CTRL-C` sends SIGINT to the parent shell process and all its child processes
- Parent process will ignore SIGINT
- Background process will ignore SIGINT
- Child process will terminate itself when received SIGINT
#### SIGTSTP
`CTRL-Z` sends SIGTSTP to the parent shell process and all its child processes
- Child process will ignore SIGTSTP
- Background process will ignore SIGTSTP
- SIGTSTP will toggle the shell to "foreground-only mode", meaning all subsequent commands with `&` will be ignored. (Will therefore run in the foreground). Can toggle it back with `CTRL-Z`.