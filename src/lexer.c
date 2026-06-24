#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
    TODO: refactor char and token buffers into their own structs.
*/

#define DEFAULT_BUFSIZE 16

enum LexerState {
    NORMAL,
    IN_QUOTE
};

typedef struct {
    char *lexeme;
    token_type_t type;
} op_entry_t;

static op_entry_t OPERATORS[] = {
    {">>", TOKEN_APPEND},
    {"2>", TOKEN_REDIRECT_ERR},
    {">", TOKEN_REDIRECT_OUT},
    {"<", TOKEN_REDIRECT_IN},
    {"|", TOKEN_PIPE}
};

/**
 * @brief attempts to match a character sequence to a op_entry_t entry in OPERATORS[]
 * @param s the string to compare against
 * @param length out-param indicating the length of the matched operator
 * @return the matched op_entry_t OR NULL if no match was found
 */
static op_entry_t *match_operator(const char *s, size_t *length)
{
    // loop condition computed with sizeof to allow for future OPERATORS[] entries without mod
    for (size_t i = 0; i < sizeof(OPERATORS)/sizeof(OPERATORS[0]); i++) {
        size_t  l = strlen(OPERATORS[i].lexeme);
        if (strncmp(s, OPERATORS[i].lexeme, l) == 0) {
            *length = l;
            return &OPERATORS[i];
        }
    }
    return NULL;
}

/**
 * @brief appends a char to a char buffer, dynamically resizes as needed
 * @param buf a pre-allocated char buffer
 * @param c the character to append
 * @param position the index at which to append the char
 * @param capacity the current size of buf
 * @return 0 if successful, -1 otherwise
 */
int char_buf_add(char **buf, char c, size_t *position, size_t *capacity)
{
    if (*position + 1 >= *capacity) {
        (*capacity) *= 2;
        char *tmp = realloc(*buf, *capacity);
        if (!tmp) {
            fprintf(stderr, "lexer: reallocation error. (char buffer)\n");
            return -1;
        }
        *buf = tmp;
    }
    (*buf)[(*position)++] = c;
    (*buf)[(*position)] = '\0';
    return 0;
}

void char_buf_clear(char **buf, size_t *position)
{
    *position = 0;
    (*buf)[(*position)] = '\0';
}

/**
 * @brief appends a token to the token_t *tok_buf at given position. dynamically resizes as needed
 * @param tok_buf pre-allocated buffer to hold token
 * @param token the token to append
 * @param position the position/index in tok_buf at which to append the token
 * @param capacity the current size of tok_buf
 * @return 0 if successful, -1 otherwise
 */
int tok_buf_add(token_t **tok_buf, token_t *token, size_t *position, size_t *capacity)
{
    if (*position == *capacity) {
        (*capacity) *= 2;
        token_t *tmp = realloc(*tok_buf, *capacity * sizeof(token_t));
        if (!tmp) {
            fprintf(stderr, "lexer: reallocation error. (token buffer)\n");
            return -1;
        }
        *tok_buf = tmp;
    }
    (*tok_buf)[(*position)++] = *token;
    return 0;
}

/**
 * @brief finalizes the accumulated char buffer into a TOKEN_WORD and appends it to
 *        the token buffer, then clears the char buffer. No-op if the buffer is empty.
 * @param tokens the token buffer to append to
 * @param tok_pos out-param token count / insert position
 * @param tok_cap current capacity of the token buffer
 * @param buf the char buffer to finalize and clear
 * @param buf_len current length of the char buffer
 * @return 0 on success (including the empty-buffer no-op), -1 on failure
 */
static int flush_word(token_t **tokens, size_t *tok_pos, size_t *tok_cap, char **buf, size_t *buf_len)
{
    if (*buf_len == 0) return 0; // nothing to flush, not an error

    token_t tok = { .type = TOKEN_WORD, .value = strdup(*buf) };
    if (!tok.value) return -1;

    if (tok_buf_add(tokens, &tok, tok_pos, tok_cap) != 0) {
        free(tok.value);
        return -1;
    }

    char_buf_clear(buf, buf_len);
    return 0;
}

/**
 * @brief splits a string of characters into individual tokens (words, operators, quotes)
 * @param line a string of chars terminated with a NULL character
 * @param count out-param which the callee modifies to reflect length of returned array
 * @return an array of token_t* with a length of 'count' OR NULL on failure
 */
token_t *lex(const char *line, size_t *count)
{
    *count = 0;
    if (line == NULL) return NULL;

    size_t tok_buf_cap = DEFAULT_BUFSIZE; // capacity of token buffer
    token_t *tokens = malloc(DEFAULT_BUFSIZE * sizeof(token_t)); // token buffer to append to
    if (!tokens) {
        fprintf(stderr, "lexer: allocation error. (token buffer)\n");
        return NULL;
    }
    token_t tok;
    op_entry_t *op;
    size_t op_len;
    size_t line_pos = 0; // current position in text line
    size_t buf_len = 0, buf_cap = DEFAULT_BUFSIZE; // current position and capacity of char buffer
    char *buf = malloc(buf_cap); // temp buffer to accumulate chars into
    if (!buf) {
        fprintf(stderr, "lexer: allocation error. (char buffer)\n");
        return NULL;
    }

    enum LexerState state = NORMAL; // state of lexer

    while (line[line_pos] != '\0') {
        char c = line[line_pos];

        switch (state) {
        case IN_QUOTE: 
            if (c == '"') {     // c is an ending quotation —> return to normal state
                state = NORMAL;
            } else {            // c is NOT an ending quotation —> append c to buffer
                if (char_buf_add(&buf, line[line_pos], &buf_len, &buf_cap) != 0) {
                    goto fail;
                }
            }
            break;
        case NORMAL:
            if (isspace(c)) {   // c is whitespace —> inspect buffer
                if (flush_word(&tokens, count, &tok_buf_cap, &buf, &buf_len) != 0) goto fail;
            } else if ((op = match_operator(&line[line_pos], &op_len))) {
                if (flush_word(&tokens, count, &tok_buf_cap, &buf, &buf_len) != 0) goto fail;
                tok.type = op->type;
                tok.value = strdup(op->lexeme);
                if (!tok.value) goto fail;
                if (tok_buf_add(&tokens, &tok, count, &tok_buf_cap) != 0) {
                        free(tok.value);
                        goto fail;
                }
                char_buf_clear(&buf, &buf_len);
                line_pos += op_len - 1;
            } else if (c == '"') { // c is an opening quotation —> switch states
                state = IN_QUOTE;
            } else {               // c is a normal character —> append c to buffer
                if (char_buf_add(&buf, line[line_pos], &buf_len, &buf_cap) != 0) {
                    goto fail;
                }
            }
            break;
        }
        line_pos++;
    }
    
    if (flush_word(&tokens, count, &tok_buf_cap, &buf, &buf_len) != 0) goto fail;
    free(buf);
    return tokens;

fail:
    free(buf);
    free(tokens);
    *count = 0;
    return NULL;
}

/**
 * @brief frees an array of token_t tokens along with their internal char *value buffers
 * @param tokens the array of token_t tokens to free
 * @param count the size of the provided tokens array
 */
void free_tokens(token_t *tokens, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free(tokens[i].value);
    }
    free(tokens);
}