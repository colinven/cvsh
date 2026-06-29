#include "lexer.h"
#include "parser.h"
#include "executor.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define COLOR_RESET "\x1b[0m"
#define COLOR_BLUE "\x1b[34;47m"
#define COLOR_YELLOW "\x1b[93m"

char *read_line(void)
{
    char *line = NULL;
    size_t len = 0;
    
    if (getline(&line, &len, stdin) == -1) {
        free(line);
        return NULL;
    }
    return line;
}

int main(void)
{
    char cwd[1024];
    char *input_line;
    token_t *tokens;
    size_t tok_count;
    pipeline_t *cmd_pipeline;

    while(1) {
        // display prompt
        if (getcwd(cwd, sizeof(cwd))) {
            printf(COLOR_BLUE "|cvsh|" COLOR_RESET COLOR_YELLOW " %s" COLOR_RESET " > ", cwd);
        } else {
            printf(COLOR_BLUE "|cvsh|" COLOR_RESET " > ");
        }

        // read a line of input
        input_line = read_line();
        if (!input_line) {
            fprintf(stderr, "cvsh: error: unable to read input\n");
            return 1;
        }
        
        // tokenize input
        tokens = lex(input_line, &tok_count);
        free(input_line);
        if (!tokens) continue;

        // parse tokens into commands & operators
        cmd_pipeline = parse(tokens, tok_count);
        free_tokens(tokens, tok_count);
        if (!cmd_pipeline) continue;

        // execute commands
        execute(cmd_pipeline);
        free_pipeline(cmd_pipeline, cmd_pipeline->n_commands);
    }
    
    
    return 0;
}    