#include "lexer.h"
#ifndef PARSER_H
#define PARSER_H

typedef struct {
    char **argv;    // NULL-terminated for execvp
    int argc;       // arg count
    char *infile;   // '<' target or NULL
    char *outfile;  // '> or >>' target or NULL
    char *errfile;  // '2>' target or NULL
    int append;     // '> vs >>' (0 or 1 respectively)
} command_t;

typedef struct {
    command_t *commands;    // array of piped commands
    size_t n_commands;         // number of commands
} pipeline_t;

pipeline_t *parse(token_t *tokens, size_t count);
void free_pipeline(pipeline_t *pipeline, size_t n_commands);

#endif