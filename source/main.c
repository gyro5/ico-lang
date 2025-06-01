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
#endif // DEBUG_PRINT_TOKEN

#ifdef USE_LIBEDIT

#include <editline/readline.h>

// Need to make these because the one provided by editline are char literals
// which cannot be concatenated with string literals
#define EL_PS "\1"   // RL_PROMPT_START_IGNORE '\1'
#define EL_PE "\2"   // RL_PROMPT_END_IGNORE '\2'

// Libedit needs a space " " at the end to actually reset the colors
#define RED_PROMPT(s)   EL_PS COLOR_RED COLOR_BOLD EL_PE s EL_PS COLOR_RESET EL_PE " "
#define GREEN_PROMPT(s) EL_PS COLOR_GREEN COLOR_BOLD EL_PE s EL_PS COLOR_RESET EL_PE " "
#define BLUE_PROMPT(s)  EL_PS COLOR_BLUE COLOR_BOLD EL_PE s EL_PS COLOR_RESET EL_PE " "

#define ICO_INIT_REPL(l)        rl_readline_name = "ico"; char* l
#define ICO_READLINE(l)         ((l = readline(repl_prompt[res])) != NULL)
#define ICO_NOT_EMPTY_LINE(l)   (*l)
#define ICO_SAVELINE(l)         add_history(l)
#define ICO_FREELINE(l)         free(l)

/* Note:
When the program prints right before a readline prompt, such as "hello(^_^) ",
the editing behaviour is bugged, ie. when the user types in then deletes, the
prompt is deleted too, possibly due to incorrect cursor calculation.
Therefore, the '\n' in ICO_SAVELINE() above is required.*/

#else // Non libedit version

#define RED_PROMPT(s)   COLOR_RED COLOR_BOLD s COLOR_RESET " "
#define GREEN_PROMPT(s) COLOR_GREEN COLOR_BOLD s COLOR_RESET " "
#define BLUE_PROMPT(s)  COLOR_BLUE COLOR_BOLD s COLOR_RESET " "

#define ICO_INIT_REPL(l)        char l[1024];
#define ICO_READLINE(l)         (fputs(repl_prompt[res], stdout), fgets(l, sizeof(l), stdin))
#define ICO_NOT_EMPTY_LINE(l)   (!(l[0] == '\n' && l[1] == '\0'))
#define ICO_SAVELINE(l)
#define ICO_FREELINE(l)

#endif // USE_LIBEDIT

const char* repl_prompt[] = {
    [INTERPRET_IDLE] = BLUE_PROMPT("(o_o)"),
    [INTERPRET_OK] = GREEN_PROMPT("(^_^)"),
    [INTERPRET_COMPILE_ERROR] = RED_PROMPT("(-_-)"),
    [INTERPRET_RUNTIME_ERROR] = RED_PROMPT("(-_-)"),
};

// Run the Ico REPL
static void run_repl() {
    // Print the REPL start
    printf(COLOR_BOLD "Ico Interactive REPL.\n" COLOR_RESET
           "- %s: Idle\n"
           "- %s: Success\n"
           "- %s: Errors\n",
           repl_prompt[INTERPRET_IDLE],
           repl_prompt[INTERPRET_OK],
           repl_prompt[INTERPRET_COMPILE_ERROR]
    );

    // The REPL loop
    InterpretResult res = INTERPRET_IDLE;
    ICO_INIT_REPL(line);
    while (ICO_READLINE(line)) {                        // Read
        if (ICO_NOT_EMPTY_LINE(line)) {
            printf(COLOR_CYAN);
            res = RUN_CODE(line);                       // Eval
            vm_print_stored_val();                      // Print
            // See above for explanation of "\n"
            printf(COLOR_RESET "\n");
            ICO_SAVELINE(line);
        }
        else { // Empty line
            res = INTERPRET_IDLE;
        }

        ICO_FREELINE(line);
    }

    // Exit the REPL with Ctrl-D
    printf(COLOR_BOLD COLOR_BLUE"\n(-.-)/"COLOR_RESET" ~( Bye! )\n");
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
    if (argc == 1) { // REPL mode
        init_vm(true);
        run_repl();
    }
    else if (argc == 2) { // Script mode
        init_vm(false);
        run_script(argv[1]);
    }
    else {
        fprintf(stderr, "Usage:\n- Run script: %s path\n- REPL: %s\n", argv[0], argv[0]);
        exit(64);
    }

    free_vm();
    return 0;
}





