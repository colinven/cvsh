#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

typedef int (*CommandFunc)(command_t *cmd);

typedef struct {
    const char *keyword;
    CommandFunc function;
} CommandMap;

/**
 * @brief built-in command for 'cd'
 * @return 0 on success, -1 on failure
 */
int builtin_cd(command_t *cmd)
{
    if (!cmd->argv) return -1;
    const char *dir = cmd->argv[1] ? cmd->argv[1] : getenv("HOME");
    if (chdir(dir) != 0) {
        perror("cvsh: error changing directory");
        return -1;
    }
    return 0;
}

/**
 * @brief built-in command for 'exit' to terminate program
 */
int builtin_exit(command_t *cmd)
{
    int code = (cmd->argv[1]) ? atoi(cmd->argv[1]) : 0;
    exit(code);
    return 0;
}

/**
 * @brief registry of builtin shell commands
 */
static CommandMap BUILTINS[] = {
    {"cd", builtin_cd},
    {"exit", builtin_exit}
};

/**
 * @brief iterates through the BUILTINS array, attempting to match a command value with 
 * a builtin command keyword
 * @param cmd the command to compare
 * @return a CommandFunc pointing to the corresponding builtin function (or NULL if no match)
 */
CommandFunc lookup_builtin(command_t *cmd)
{
    for (size_t i = 0; i < sizeof(BUILTINS)/sizeof(BUILTINS[0]); i++) {
        if (strcmp(cmd->argv[0], BUILTINS[i].keyword) == 0) {
            return BUILTINS[i].function;
        }
    }
    return NULL;
}

/**
 * @brief execute a pipeline of non-builtin shell commands
 * @param cmd_pipeline pipeline_t* containing commands to run
 * @return 0 on success, -1 on failure
 */
int exec_pipeline(pipeline_t *cmd_pipeline)
{
    size_t n = cmd_pipeline->n_commands;
    command_t *cmds = cmd_pipeline->commands;
    
    int prev_pipe_read = -1;
    int curr_pipe[2];
    pid_t pids[n];

    int in_fd = -1;
    int out_fd = -1;
    int err_fd = -1;

    for (size_t i = 0; i < n; i++) {

        // if not the last process, create a pipe (curr_pipe) for child 'i' to write to
        if (i < n - 1) {
            if (pipe(curr_pipe) == -1) {
                perror("cvsh: pipe failed");
                return -1;
            }
        }

        pids[i] = fork();

        if (pids[i] < 0) { // { FORK FAILED }
            perror("cvsh: fork failed");
            return -1;

        } else if (pids[i] == 0) { // { IN CHILD }
            // if not the first process, connect read-end of previous pipe to this childs stdin
            if (i > 0) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }
            // if not the last process, connect write-end of current pipe to this childs stdout
            if (i < n - 1) {
                dup2(curr_pipe[1], STDOUT_FILENO);
                close(curr_pipe[0]);
                close(curr_pipe[1]);
            }

            // handle stdin redirection (if file was passed)
            if (cmds[i].infile) {
                in_fd = open(cmds[i].infile, O_RDONLY);
                if (in_fd == -1) {
                    perror("cvsh: stdin redirect failed");
                    _exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            // handle stdout redirection (if file was passed)
            if (cmds[i].outfile) {

                if (cmds[i].append) { // if '>>' was passed instead of '>', append to file instead of truncating
                    out_fd = open(cmds[i].outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                } else {
                    out_fd = open(cmds[i].outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                }
                
                if (out_fd == -1) {
                    perror("cvsh: stdout redirect failed");
                    _exit(EXIT_FAILURE);
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }

            // handle stderr redirection (if file was passed)
            if (cmds[i].errfile) {
                err_fd = open(cmds[i].errfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (err_fd == -1) {
                    perror("cvsh: stderr redirect failed");
                    _exit(EXIT_FAILURE);
                }
                dup2(err_fd, STDERR_FILENO);
                close(err_fd);
            }

            // execute command
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("cvsh: execution failed");
            _exit(EXIT_FAILURE);

        } else { // { IN PARENT }
            // clean-up: if not the first process, close previous pipes read-end
            if (i > 0) {
                close(prev_pipe_read);
            }
            // if not last process, pass curr read end to prev for next child, close write end to signal EOF to child.
            if (i < n - 1) { 
                prev_pipe_read = curr_pipe[0];
                close(curr_pipe[1]);
            }
        }
    }
    // reap all n children
    for (size_t i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
    }
    return 0;
}

/**
 * @brief executes a pipeline of builtin (or non-builtin) shell commands
 * @param cmd_pipeline pipeline_t* containing commands to run
 * @return 0 on success, -1 on failure
 */
int execute(pipeline_t *cmd_pipeline)
{
    if (!cmd_pipeline || !cmd_pipeline->commands) return -1;
    CommandFunc builtin = lookup_builtin(&cmd_pipeline->commands[0]);
    if (builtin) {
        return builtin(&cmd_pipeline->commands[0]);
    }
    return exec_pipeline(cmd_pipeline);
}