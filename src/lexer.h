#include <stddef.h>
#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_WORD,         // commands, args, filenames
    TOKEN_PIPE,         // |
    TOKEN_REDIRECT_IN,  // <
    TOKEN_REDIRECT_ERR, // 2>
    TOKEN_REDIRECT_OUT, // >
    TOKEN_APPEND        // >>
} token_type_t;

typedef struct {
    char *value;
    token_type_t type;
} token_t;

token_t *lex(const char *line, size_t *count);
void free_tokens(token_t *tokens, size_t count);

#endif