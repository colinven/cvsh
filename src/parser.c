#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEF_BUF_SIZE 16

/**
 * @brief appends a char * to a char **buffer, dynamically resizes as needed
 * @param buf a pre-allocated char **buffer
 * @param ptr the char * to append
 * @param argc the index at which to append the char
 * @param capacity the current size of buf
 * @return 0 if successful, -1 otherwise
 */
int argv_buf_add(char ***buf, char *ptr, int* argc, size_t *capacity)
{
    if (*argc + 1 >= (int)*capacity) {
        (*capacity) *= 2;
        char **tmp = realloc(*buf, *capacity * sizeof(char *));
        if (!tmp) {
            fprintf(stderr, "cvsh: reallocation error (parser)\n");
            return -1;
        }
        *buf = tmp;
    }
    (*buf)[(*argc)++] = strdup(ptr);
    return 0;
}

/**
 * @brief parses redirect token (<, >, >>, 2>), strdup the following token (filename) to
 * target_field. the function validates that a following token exists, 
 * and is of enum type WORD_TOKEN.
 * @param tokens the token array being iterated through
 * @param count the size/amount of tokens in the tokens array
 * @param i iterator of the calling loop
 * @param target_field the field to assign the filename token to
 * @return 0 on success, -1 otherwise
 */
int parse_redirect(token_t *tokens, size_t count, size_t *i, char **target_field)
{
    token_type_t op = tokens[*i].type;
    if (*i + 1 >= count || tokens[*i + 1].type != TOKEN_WORD) {
        fprintf(stderr, "cvsh: syntax error: expected filename after '%s'\n", 
            token_type_to_lexeme(op));
        return -1;
    }
    *target_field = strdup(tokens[++(*i)].value);
    if (!*target_field) return -1;
    return 0;
}

/**
 * @brief parses an array of token_t into a pipeline_t containing individual command_t structs.
 * each command_t contains argv/argc along with any corresponding input/output/error files passed 
 * by the user via redirect operators.
 * @param tokens array of token_t to be parsed
 * @param count size/amount of tokens in the tokens array
 * @return a pipeline_t containing parsed command_t structs
 */
pipeline_t *parse(token_t *tokens, size_t count)
{
    if (!tokens || !count) return NULL;

    size_t n_commands = 1;                              // calculate number of commands passed as input prior to parse loop.
    for (size_t i = 0; i < count; i++) {                // 
        if (tokens[i].type == TOKEN_PIPE) n_commands++; // (prevents us from needing to re-size commands array)
    }                                                   //

    command_t *commands = calloc(n_commands, sizeof(command_t));
    if (!commands) {    // calloc failed. bail
        fprintf(stderr, "cvsh: allocation error (parser)\n");
        return NULL;
    }
    size_t cmd_idx = 0;

    // cur_cmd pointer would go stale if commands array were to be resized. but works since we pre-size array (lines 68-71)
    command_t *cur_cmd = &commands[cmd_idx];

    pipeline_t *pipeline = calloc(1, sizeof(pipeline_t));
    if (!pipeline){
        fprintf(stderr, "cvsh: allocation error (parser)\n");
        free(commands);
        return NULL;
    }                                                           
    pipeline->commands = commands;

    size_t argv_cap = DEF_BUF_SIZE;
    cur_cmd->argv = malloc(argv_cap * sizeof(char *));
    if (!cur_cmd->argv) {
        fprintf(stderr, "cvsh: reallocation error (parser)\n");
        goto fail;
    }
    

    for (size_t i = 0; i < count; i++) {
        token_t *cur_token = &tokens[i];

        if (cur_cmd->argc == 0 && cur_token->type != TOKEN_WORD) {              // The parser does not allow leading redirects
            fprintf(stderr, "cvsh: syntax error near unexpected token '%s'\n",  // (e.g. "> out.txt cat")
                token_type_to_lexeme(cur_token->type));                         //
            goto fail;                                                          // this check catches if first arg is not TOKEN_WORD
        }                                                                       // (also catches empty commands (e.g. cat in.txt | |"))

        switch(cur_token->type) {
            case TOKEN_WORD:    // append word token to argv
                if (argv_buf_add(&cur_cmd->argv, cur_token->value, &cur_cmd->argc, &argv_cap) != 0) goto fail;
                break;

            case TOKEN_PIPE:    // signals transition between commands. 
                cur_cmd->argv[cur_cmd->argc] = NULL;    // Finalize current command, advance cmd_idx pointer
                cur_cmd = &commands[++cmd_idx];         //

                    cur_cmd->argv = malloc(DEF_BUF_SIZE * sizeof(char *));      // Init argv/argc for upcoming command
                    if (!cur_cmd->argv) {                                       //
                    fprintf(stderr, "cvsh: reallocation error (parser)\n");     // bail if malloc fails.
                    goto fail;                                                  //
                }                                                               //
                break;

            case TOKEN_REDIRECT_IN:
                if (parse_redirect(tokens, count, &i, &cur_cmd->infile) != 0) goto fail;
                break;

            case TOKEN_REDIRECT_OUT:
                if (parse_redirect(tokens, count, &i, &cur_cmd->outfile) != 0) goto fail;
                break;

            case TOKEN_REDIRECT_ERR:
                if (parse_redirect(tokens, count, &i, &cur_cmd->errfile) != 0) goto fail;
                break;

            case TOKEN_APPEND:
                if (parse_redirect(tokens, count, &i, &cur_cmd->outfile) != 0) goto fail;
                cur_cmd->append = 1;
                break;
        }
    }
    cur_cmd->argv[cur_cmd->argc] = NULL;    // NULL-cap current command argv array (for execvp())
    
    pipeline->n_commands = n_commands;
    return pipeline;
    
fail:
    free_pipeline(pipeline, cmd_idx + 1);
    return NULL;
}

/**
 * @brief completely frees pipeline_t including all fields in each command of the commands array
 * @param pipeline pipeline_t to free
 * @param n_commands length of internal commands array (just pass pipeline->n_commands)
 */
void free_pipeline(pipeline_t *pipeline, size_t n_commands)
{
    if (!pipeline) return;
    if (pipeline->commands) {
        for (size_t i = 0; i < n_commands; i++) {
        command_t *cmd = &pipeline->commands[i];
        free(cmd->infile);
        free(cmd->outfile);
        free(cmd->errfile);
        for (int j = 0; j < cmd->argc; j++){
            free(cmd->argv[j]);
        }
        free(cmd->argv);
        }
        free(pipeline->commands);
    }
    free(pipeline);
}