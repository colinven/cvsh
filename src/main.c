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