#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ico_common.h"
#include "ico_vm.h"

#define RUN_CODE(code) vm_interpret(code)

#ifdef DEBUG_PRINT_TOKEN
#include "ico_scanner.h"

#undef RUN_CODE
#define RUN_CODE(code) (scan_code(code), vm_interpret(code))

static void scan_code(const char* code) {
    init_scanner(code);
    Token t;
    do {
        t = scan_next_token();
        print_token(t);
    } while (t.type != TOKEN_EOF);
}
#endif

const char* repl_prompt[] = {
    [INTERPRET_IDLE] = COLOR_BOLD COLOR_BLUE "(o_o) " COLOR_RESET,
    [INTERPRET_OK] = COLOR_BOLD COLOR_GREEN "(^_^) " COLOR_RESET,
    [INTERPRET_COMPILE_ERROR] = COLOR_BOLD COLOR_RED "(-_-) " COLOR_RESET,
    [INTERPRET_RUNTIME_ERROR] = COLOR_BOLD COLOR_RED "(-_-) " COLOR_RESET,
};

// Run the Ico REPL
static void run_repl() {
    // To hold the current line of REPL code.
    // Limit the line size to 1024 for simplicity's sake.
    char line[1024];
    InterpretResult res = INTERPRET_IDLE;

    printf(COLOR_BOLD "Ico Interactive REPL.\n" COLOR_RESET
           "- %s: Idle\n"
           "- %s: Success\n"
           "- %s: Errors\n",
           repl_prompt[INTERPRET_IDLE],
           repl_prompt[INTERPRET_OK],
           repl_prompt[INTERPRET_COMPILE_ERROR]);

    for (;;) {
        printf("\n%s", repl_prompt[res]);

        if (!fgets(line, sizeof(line), stdin)) {
            printf(COLOR_BOLD COLOR_BLUE"\n\n(-.-)/"COLOR_RESET" ~( Bye! )\n");
            break;
        }

        printf(COLOR_CYAN);
        res = RUN_CODE(line);
        printf(COLOR_RESET);
    }
}

// Read an Ico file and return a string containing the source code
static char* read_file(const char* path) {
    // Open the source code file
    FILE* script = fopen(path, "rb");
    if (script == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    // Calculate the size of the source code
    fseek(script, 0L,SEEK_END);
    size_t script_size = ftell(script);
    rewind(script);

    // Read the source code content into the buffer
    char* buff = (char*)malloc(script_size + 1);

    // Check for memory allocation failure
    if (buff == NULL) {
        fprintf(stderr, "Not enough memory to read the file \"%s\".\n", path);
        exit(74);
    }

    size_t bytes_read = fread(buff, sizeof(char), script_size, script);

    // Less bytes read than allocated -> possible read error
    if (bytes_read < script_size) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buff[script_size] = '\0'; // End the string

    // Close the file
    fclose(script);

    return buff;
}

// Run a Ico script
static void run_script(char* path) {
    char* source_code = read_file(path);
    InterpretResult result = RUN_CODE(source_code);
    free(source_code);

    if (result == INTERPRET_COMPILE_ERROR) {
        exit(65);
    }
    if (result == INTERPRET_RUNTIME_ERROR) {
        exit(70);
    }
}

//------------------------------
//         MAIN RUNTIME
//------------------------------

int main(int argc, char *argv[]) {
    init_vm();

    // Script mode or REPL mode
    if (argc == 1) {
        run_repl();
    }
    else if (argc == 2) {
        run_script(argv[1]);
    }
    else {
        fprintf(stderr, "Usage:\n- Run script: %s path\n- REPL: %s\n", argv[0], argv[0]);
        exit(64);
    }

    free_vm();
    return 0;
}





