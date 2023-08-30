#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>

/// description: Takes a line and splits it into args similar to how argc
///              and argv work in main
/// line:        The line being split up. Will be mangled after completion
///              of the function<
/// args:        a pointer to an array of strings that will be filled and
///              allocated with the args from the line
/// num_args:    a point to an integer for the number of arguments in args
/// return:      returns 0 on success, and -1 on failure
int lexer(char *line, char ***args, int *num_args){
    *num_args = 0;
    // count number of args
    char *l = strdup(line);
    if(l == NULL){
        return -1;
    }
    char *token = strtok(l, " \t\n");
    while(token != NULL){
        (*num_args)++;
        token = strtok(NULL, " \t\n");
    }
    free(l);
    // split line into args
    *args = malloc(sizeof(char **) * *num_args);
    *num_args = 0;
    token = strtok(line, " \t\n");
    while(token != NULL){
        char *token_copy = strdup(token);
        if(token_copy == NULL){
            return -1;
        }
        (*args)[(*num_args)++] = token_copy;
        token = strtok(NULL, " \t\n");
    }
    return 0;
}

int split_cmds(char *line, char ***cmds, int *num_cmds){
    *num_cmds = 0;
    char *l = strdup(line);
    if(l == NULL){
        return -1;
    }
    char *token = strtok(l, ";");
    while(token != NULL){
        (*num_cmds)++;
        token = strtok(NULL, ";");
    }
    free(l);
    *cmds = malloc(sizeof(char **) * *num_cmds);
    *num_cmds = 0;
    token = strtok(line, ";");
    while(token != NULL){
        char *token_copy = strdup(token);
        if(token_copy == NULL){
            return -1;
        }
        (*cmds)[(*num_cmds)++] = token_copy;
        token = strtok(NULL, ";");
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char error_message[30] = "An error has occurred\n";

    char *line= NULL;
    size_t len;
    int status = -1;

    // any additional args in shell invocation is an error
    if(argc > 1)
        write(STDERR_FILENO, error_message, strlen(error_message));

    // enter shell
    while(write(1, "smash> ", 7)) {
        // get cmd
        status = getline(&line, &len, stdin);
        if(feof(stdin)) {
            if(line != NULL)
                free(line);
            line = NULL;
            exit(0);      // exit(0) gracefully when seeing EOF
        }
        else if(status == -1) {         // other getline failures, abort and loop back
            write(STDERR_FILENO, error_message, strlen(error_message));
            if(line != NULL)
                free(line);
            line = NULL;
            continue;
        }

        // splits by ;
        char **cmds;
        int num_cmds;
        status = split_cmds(line, &cmds, &num_cmds);
        if(line != NULL)
            free(line);
        line = NULL;
        if(status == -1) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            for(int i = 0; i < num_cmds; i++)
                if(cmds != NULL && cmds[i] != NULL)
                    free(cmds[i]);
            if(cmds != NULL)
                free(cmds);
            cmds = NULL;
            continue;
        }

        // handle each command seperated by ;
        for(int cmd_idx = 0; cmd_idx < num_cmds; cmd_idx++) {
            char **args;
            int num_args;
            char *cmd_string = cmds[cmd_idx];

            // remove extra white space
            char *cmd = malloc((strlen(cmd_string) + 1) * sizeof(char));
            bool prev_is_space = false;
            bool all_space = true;
            int index = 0;
            for(int i = 0; i < strlen(cmd_string); i++) {
                if(cmd_string[i] == ' ' || cmd_string[i] == '\t' || cmd_string[i] == '\n') {
                    if(!all_space && !prev_is_space)
                        cmd[index++] = cmd_string[i];
                    prev_is_space = true;
                }
                else {
                    cmd[index++] = cmd_string[i];
                    prev_is_space = false;
                    all_space = false;
                }
            }
            while(cmd[index - 1] == ' ' || cmd[index - 1] == '\t' || cmd[index - 1] == '\n')
                index--;
            cmd[index] = '\0';

            // jump over meaningless cmd
            if(all_space) {
                if(cmd != NULL)
                    free(cmd);
                cmd = NULL;
                continue;
            }

            // parse cmd
            status = lexer(cmd, &args, &num_args);
            // for mem free
            char **args_allocated = args;
            int num_args_allocated = num_args;
            if(cmd != NULL)
                free(cmd);
            cmd = NULL;
            if(status == -1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                for(int i = 0; i < num_args_allocated; i++)
                    if(args_allocated != NULL && args_allocated[i] != NULL)
                        free(args_allocated[i]);
                if(args_allocated != NULL)
                    free(args_allocated);
                args_allocated = NULL;
                continue;
            }

            // handle loop command (run once when loop not specified)
            int loop_cnt = 1;
            if(strcmp(args[0], "loop") == 0) {
                if(num_args < 3) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    for(int i = 0; i < num_args_allocated; i++)
                        if(args_allocated != NULL && args_allocated[i] != NULL)
                            free(args_allocated[i]);
                    if(args_allocated != NULL)
                        free(args_allocated);
                    args_allocated = NULL;
                    continue;
                }
                loop_cnt = atoi(args[1]);
                if(loop_cnt == 0) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    for(int i = 0; i < num_args_allocated; i++)
                        if(args_allocated != NULL && args_allocated[i] != NULL)
                            free(args_allocated[i]);
                    if(args_allocated != NULL)
                        free(args_allocated);
                    args_allocated = NULL;
                    continue;
                }
                args += 2;
                num_args -= 2;
            }

            // execute command
            while(loop_cnt > 0) {
                loop_cnt--;
                if(strcmp(args[0], "exit") == 0) {              // built-in exit cmd
                    if(num_args != 1) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        continue;
                    }
                    // free memory
                    for(int i = 0; i < num_args_allocated; i++)
                        if(args_allocated != NULL && args_allocated[i] != NULL)
                            free(args_allocated[i]);
                    if(args_allocated != NULL)
                        free(args_allocated);
                    args_allocated = NULL;
                    for(int i = 0; i < num_cmds; i++)
                        if(cmds != NULL && cmds[i] != NULL)
                            free(cmds[i]);
                    if(cmds != NULL)
                        free(cmds);
                    cmds = NULL;
                    exit(0);            // exit
                }
                else if(strcmp(args[0], "cd") == 0) {            // built-in cd cmd
                    if(num_args != 2) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        continue;
                    }
                    status = chdir(args[1]);
                    if(status == -1) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        continue;
                    }
                }
                else if(strcmp(args[0], "pwd") == 0) {            // built-in cd cmd
                    if(num_args != 1) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        continue;
                    }
                    size_t size = 1;
                    char* cwd;
                    bool getcwd_failed = false;
                    cwd = malloc(size * sizeof(char));
                    if(cwd == NULL) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        if(cwd != NULL)
                            free(cwd);
                        cwd = NULL;
                        continue;
                    }
                    while(getcwd(cwd, size) == NULL) {
                        // getcwd fail for other reasons
                        if(errno != ERANGE) {
                            write(STDERR_FILENO, error_message, strlen(error_message));
                            getcwd_failed = true;
                            break;
                        }
                        // getcwd fail for buf not large enough
                        size *= 2;
                        cwd = realloc(cwd, size * sizeof(char));
                        if(cwd == NULL) {
                            write(STDERR_FILENO, error_message, strlen(error_message));
                            getcwd_failed = true;
                            break;
                        }
                    }
                    if(getcwd_failed) {
                        if(cwd != NULL)
                            free(cwd);
                        cwd = NULL;
                        continue;
                    }
                    write(1, cwd, strlen(cwd));
                    write(1, "\n", 1);
                    if(cwd != NULL)
                        free(cwd);
                    cwd = NULL;
                }
                else {
                    // get number of pipes needed
                    int num_pipes = 0;
                    for(int i = 0; i < num_args; i++) {
                        if(strcmp(args[i], "|") == 0)
                            num_pipes++;
                    }

                    // create pipes if needed
                    int pipefds[2 * num_pipes];
                    for(int i = 0; i < num_pipes; i++) {
                        if(pipe(pipefds + i * 2) < 0) {
                            write(STDERR_FILENO, error_message, strlen(error_message));
                            continue;
                        }
                    }

                    // parse command by | (pipe)
                    char *** pipe_cmds = malloc((num_pipes + 1) * sizeof(char*));
                    int pipe_cmd_idx = 0;
                    int pipe_cmds_num_args[num_pipes + 1];
                    char **pipe_cmd = malloc(num_args * sizeof(char*));
                    int pipe_cmd_arg_idx = 0;
                    for(int i = 0; i < num_args; i++) {
                        if(strcmp(args[i], "|") != 0) {
                            pipe_cmd[pipe_cmd_arg_idx++] = strdup(args[i]);
                        }
                        else {
                            pipe_cmds_num_args[pipe_cmd_idx] = pipe_cmd_arg_idx;
                            pipe_cmds[pipe_cmd_idx++] = pipe_cmd;
                            pipe_cmd = malloc(num_args * sizeof(char*));
                            pipe_cmd_arg_idx = 0;
                        }
                    }
                    // last cmd
                    pipe_cmds_num_args[pipe_cmd_idx] = pipe_cmd_arg_idx;
                    pipe_cmds[pipe_cmd_idx++] = pipe_cmd;

                    int child_process_cnt = 0;
                    // run commands in child processes, pipe when needed
                    for(pipe_cmd_idx = 0; pipe_cmd_idx < num_pipes + 1; pipe_cmd_idx++) {
                        char **pipe_cmd_args = pipe_cmds[pipe_cmd_idx];
                        int pipe_cmd_num_args = pipe_cmds_num_args[pipe_cmd_idx];

                        // check if program file exsits
                        if (access(pipe_cmd_args[0], F_OK) == -1) {
                            write(STDERR_FILENO, error_message, strlen(error_message));
                            for(int i = 0; i < pipe_cmd_num_args; i++) {
                                free(pipe_cmd_args[i]);
                                pipe_cmd_args[i] = NULL;
                            }
                            break;
                        }

                        // check if redirection is needed
                        int redirect_idx = -1;
                        for(int i = 0; i < pipe_cmd_num_args; i++)
                            if(strcmp(pipe_cmd_args[i], ">") == 0) {
                                redirect_idx = i;
                                break;
                            }
                        char *redirct_file = NULL;
                        if(redirect_idx != -1) {
                            if(redirect_idx != pipe_cmd_num_args - 2 || redirect_idx == 0 || pipe_cmd_args[redirect_idx + 1] == NULL || strcmp(pipe_cmd_args[redirect_idx + 1], ">") == 0) {
                                write(STDERR_FILENO, error_message, strlen(error_message));
                                break;
                            }
                            redirct_file = strdup(pipe_cmd_args[pipe_cmd_num_args - 1]);
                            free(pipe_cmd_args[pipe_cmd_num_args - 1]);
                            free(pipe_cmd_args[pipe_cmd_num_args - 2]);
                            pipe_cmd_num_args -= 2;
                        }

                        // force command args to end with NULL
                        char **args_appended = malloc(sizeof(char *) * (pipe_cmd_num_args + 1));
                        for(int i = 0; i < pipe_cmd_num_args; i++) {
                            args_appended[i] = strdup(pipe_cmd_args[i]);
                            free(pipe_cmd_args[i]);
                            pipe_cmd_args[i] = NULL;
                        }
                        args_appended[pipe_cmd_num_args] = NULL;

                        // execute cmd and wait until it returns
                        int rc = fork();
                        if(rc < 0) {
                            // fork failed
                            write(STDERR_FILENO, error_message, strlen(error_message));
                        }
                        else if(rc == 0) {
                            // child process

                            // if not last command
                            if(pipe_cmd_idx != num_pipes) {
                                if(dup2(pipefds[pipe_cmd_idx * 2 + 1], 1) < 0) {
                                    write(STDERR_FILENO, error_message, strlen(error_message));
                                    exit(1);
                                }
                            }

                            // if not first command
                            if(pipe_cmd_idx != 0) {
                                if(dup2(pipefds[pipe_cmd_idx * 2 - 2], 0) < 0) {
                                    write(STDERR_FILENO, error_message, strlen(error_message));
                                    exit(1);
                                }
                            }

                            // close not used pipe ends
                            for(int i = 0; i < 2 * num_pipes; i++) {
                                close(pipefds[i]);
                            }

                            // redirect output if needed
                            if(redirct_file != NULL) {
                                int newfd;
                                if ((newfd = open(redirct_file, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
                                    write(STDERR_FILENO, error_message, strlen(error_message));     	// open failed
                                    exit(1);
                                }
                                // redirect both stdout and stderr
                                dup2(newfd, 1);
                                dup2(newfd, 2);
                                close(newfd);
                            }

                            // execute sub-command
                            execv(args_appended[0], args_appended);
                            // execv should never return
                            write(STDERR_FILENO, error_message, strlen(error_message));
                            exit(1);      // child process terminate with error
                        }
                        else {
                            // count child proc launched
                            child_process_cnt++;
                            // free memory
                            for(int i = 0; i < pipe_cmd_num_args; i++)
                                if(args_appended != NULL && args_appended[i] != NULL) {
                                    free(args_appended[i]);
                                    args_appended[i] = NULL;
                                }
                            if(args_appended != NULL)
                                free(args_appended);
                            args_appended = NULL;
                        }

                        if(redirct_file != NULL)
                            free(redirct_file);
                    }

                    // parent process close all pipe ends and wait until all children exit
                    for(int i = 0; i < 2 * num_pipes; i++)
                        close(pipefds[i]);
                    for(int i = 0; i < child_process_cnt; i++) {
                        status = wait(NULL);
                        if(status == -1)
                            write(STDERR_FILENO, error_message, strlen(error_message));
                    }

                    // free memory
                    for(int i = 0; i < num_pipes + 1; i++) {
                        if(pipe_cmds != NULL && pipe_cmds[i] != NULL) {
                            free(pipe_cmds[i]);
                            pipe_cmds[i] = NULL;
                        }
                    }
                    if (pipe_cmds != NULL)
                        free(pipe_cmds);
                    pipe_cmds = NULL;
                }
            }
            // free memory
            for(int i = 0; i < num_args_allocated; i++)
                if(args_allocated != NULL && args_allocated[i] != NULL)
                    free(args_allocated[i]);
            if(args_allocated != NULL)
                free(args_allocated);
            args_allocated = NULL;
        }
        // free memory
        for(int i = 0; i < num_cmds; i++)
            if(cmds != NULL && cmds[i] != NULL)
                free(cmds[i]);
        if(cmds != NULL)
            free(cmds);
        cmds = NULL;
    }

    // should never reach here
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(1);
}