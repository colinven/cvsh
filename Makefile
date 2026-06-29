CFLAGS = -Wall -Wextra -g
SRCS = src/main.c src/lexer.c src/parser.c src/executor.c

cvsh: $(SRCS)
	gcc $(CFLAGS) $(SRCS) -o cvsh
clean:
	rm -f cvsh