# 'cvsh' — Colin Venancio's Shell

A Unix shell written from scratch in C which tokenizes input, builds a command pipeline, and drives the Unix process API directly.

This project grew out of reading [*Operating Systems: Three Easy Pieces*](https://ostep.org/) (OSTEP). This is a learning project to solidify my understanding of process management, file redirection, and C programming overall.

---

## Features

- Command pipelines of arbitrary length (`cmd1 | cmd2 | cmd3 | ...`)
- I/O redirection: `<`, `>`, `>>`, `2>`
- Built-in commands: `cd`, `exit`
- Double-quoted strings (preserves spaces in arguments)
- Prompt which displays current working directory

---

## Architecture

Input goes through three sequential stages before a process is ever spawned:

```
  Raw input string
         │
         ▼
  ┌─────────────┐
  │    Lexer    │  lexer.c / lexer.h
  │             │  Breaks input into a flat array of token_t
  └──────┬──────┘
         │  token_t[]
         ▼
  ┌─────────────┐
  │   Parser    │  parser.c / parser.h
  │             │  Builds a pipeline_t containing command_t structs
  └──────┬──────┘
         │  pipeline_t *
         ▼
  ┌─────────────┐
  │  Executor   │  executor.c / executor.h
  │             │  fork/execvp, pipe/dup2, builtin dispatch
  └─────────────┘
```

Each stage is a clean handoff. The lexer owns nothing the parser needs to keep, and the parser owns nothing the executor needs to keep. Memory is freed at each boundary.

---

## How It Works

### Stage 1 — Lexer (`lexer.c`)

The lexer is a finite state machine with two states: `NORMAL` and `IN_QUOTE`.

It scans the input character by character, accumulating characters into a dynamically-resized char buffer. When it hits whitespace, an operator, or a quote boundary, it flushes the buffer as a `TOKEN_WORD` and advances. Operators (`|`, `<`, `>`, `>>`, `2>`) are matched against a table of `op_entry_t` structs — this makes adding new operators a one-line change.

```c
typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,         // |
    TOKEN_REDIRECT_IN,  // <
    TOKEN_REDIRECT_ERR, // 2>
    TOKEN_REDIRECT_OUT, // >
    TOKEN_APPEND        // >>
} token_type_t;
```

Both the char buffer and the token buffer grow with `realloc` doubling (the same strategy used in most dynamic array implementations).

### Stage 2 — Parser (`parser.c`)

The parser turns the flat token stream into a structured `pipeline_t`:

```c
typedef struct {
    char **argv;   // NULL-terminated (required by execvp)
    int argc;
    char *infile;  // < target
    char *outfile; // > or >> target
    char *errfile; // 2> target
    int append;    // >> vs >
} command_t;

typedef struct {
    command_t *commands;
    size_t n_commands;
} pipeline_t;
```

Redirect tokens (`<`, `>`, `>>`, `2>`) consume the following `TOKEN_WORD` as a filename. If the next token is missing or is not a word, the parser emits a syntax error and returns `NULL`.

### Stage 3 — Executor (`executor.c`)

The executor first checks whether the first command is a built-in. Built-ins are registered in a `CommandMap` table and matched by name:

```c
static CommandMap BUILTINS[] = {
    {"cd",   builtin_cd},
    {"exit", builtin_exit}
};
```

Built-ins run in the shell process itself (necessary for `cd`, which must modify the shell's own working directory via `chdir`).

For external commands, `exec_pipeline` runs each command in a child process:

1. For each command in the pipeline, `pipe()` creates a pair of file descriptors connecting adjacent processes.
2. `fork()` spawns the child. In the child, `dup2()` wires up the pipe ends (or redirect files) to `STDIN_FILENO` / `STDOUT_FILENO` / `STDERR_FILENO`, then closes the originals.
3. `execvp()` replaces the child's image with the target program.
4. The parent closes its copy of each pipe end after handing it off, so the child sees EOF when the prior stage finishes.
5. After all children are spawned, the parent calls `waitpid()` on each `pid` to reap them.

---

## Concepts Learned

| Concept | Where it shows up |
|---|---|
| `fork` / `execvp` | `exec_pipeline` — spawns one child per command |
| `pipe` / `dup2` | Chaining stdout of one child to stdin of the next |
| File descriptor inheritance | Why `dup2` + `close` is the correct pattern |
| I/O redirection via `open` | `<`, `>`, `>>`, `2>` in the executor |
| `waitpid` and zombie prevention | Parent reaps all `n` children after spawning |
| `chdir` / `getcwd` | `cd` built-in and the CWD display in the prompt |
| Finite state machine | Lexer's `NORMAL` / `IN_QUOTE` state |
| Dynamic arrays in C | Doubling `realloc` for char buffer and token buffer |
| Ownership and cleanup in C | Every allocation has a matching `free`; failure paths use `goto fail` |
| Pointer invalidation | Pre-sizing the `commands` array to keep `cur_cmd` stable |

---

## Build & Run

**Requirements:** `gcc`, `make`

```bash
# build
make

# run
./cvsh
```

To clean the binary:

```bash
make clean
```

---

## Supported Syntax

```bash
# basic command
ls -la

# pipeline
cat access.log | grep "404" | wc -l

# stdin redirect
sort < input.txt

# stdout redirect (truncate)
ls > files.txt

# stdout redirect (append)
echo "new line" >> log.txt

# stderr redirect
make 2> build_errors.txt

# quoted arguments (preserves spaces)
echo "hello world"

# built-ins
cd ..
cd ~/projects
exit
exit 1
```

---

## Roadmap

Features I plan to add as I work through more of OSTEP and beyond:

- [ ] Environment variable expansion (`$HOME`, `$PATH`, `$?`)
- [ ] Command history (up/down arrow navigation)
- [ ] Tab completion
- [ ] Job control (`Ctrl+C`, `Ctrl+Z`, `bg`, `fg`)
- [ ] `SIGCHLD` handling for non-blocking child reaping
- [ ] Command substitution (`$(...)`)
- [ ] Logical operators (`&&`, `||`)
