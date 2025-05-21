#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clox_common.h"
#include "clox_chunk.h"
#include "clox_debug.h"
#include "clox_vm.h"

//------------------------------
//      STATIC PROTOTYPES
//------------------------------

// Run the clox REPL
static void run_repl() {
    // To hold the current line of REPL code.
    // Limit the line size to 1024 for simplicity's sake.
    char line[1024];

    for (;;) {
        printf("clox> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("Exiting Lox REPL...\n");
            break;
        }

        vm_interpret(line);
    }
}

// Read a Lox script and return a string containing
// the source code
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

// Run a Lox script
static void run_script(char* path) {
    char* source_code = read_file(path);
    InterpretResult result = vm_interpret(source_code);
    free(source_code);

    if (result == INTERPRET_COMPILE_ERROR) {
        exit(65);
    }
    if (result == INTERPRET_RUNTIME_ERROR) {
        exit(70);
    }
}

//------------------------------
//      MAIN CLOX RUNTIME
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
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }

    free_vm();
    return 0;
}





