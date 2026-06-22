#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

char *cvsh_read_line(void)
{
    char *line = NULL;
    size_t len = 0;
    
    if (getline(&line, &len, stdin) == -1) {
        free(line);
        return NULL;
    }
    return line;
}

#define CVSH_TOK_BUFFERSIZE 64
#define CVSH_TOK_DELIMITER " \t\r\n\a"
char **cvsh_split_line(char *line)
{
    int bufsize = CVSH_TOK_BUFFERSIZE, pos = 0;
    char *tok, **tokens_backup;
    char **tokens = malloc(sizeof(char *) * bufsize);
    
    if (!tokens) {
        fprintf(stderr, "cvsh: allocation error.");
        exit(EXIT_FAILURE);
    }

    tok = strtok(line, CVSH_TOK_DELIMITER);
    while (tok != NULL) {
        tokens[pos++] = tok;

        if (pos >= bufsize - 1) {
            bufsize += CVSH_TOK_BUFFERSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, sizeof(char *) * bufsize);
            if (!tokens) {
                fprintf(stderr, "cvsh: allocation error.");
                free(tokens_backup);
                exit(EXIT_FAILURE);
            }
        }

        tok = strtok(NULL, CVSH_TOK_DELIMITER);
    }
    tokens[pos] = NULL;

    return tokens;
}

int main(void)
{
    char *line = cvsh_read_line();
    if (line == NULL) return 0;
    char **tokens = cvsh_split_line(line);
    int pos = 0;
    while (tokens[pos] != NULL) {
        printf("Token: %s\n", tokens[pos++]);
    }

    free(line);
    free(tokens);
    return 0;
}    