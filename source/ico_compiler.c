#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ico_common.h"
#include "ico_compiler.h"
#include "ico_scanner.h"
#include "ico_value.h"
#include "ico_object.h"
#include "ico_memory.h"

#ifdef DEBUG_PRINT_BYTECODE
#include "ico_debug.h"
#endif

// Note that C implicitly creates enum values
// from lowest to highest, which is what we want.
typedef enum {
    PREC_NONE,          // lowest precedence level
    PREC_ASSIGNMENT,    // =
    PREC_TERNARY,       // ?:
    PREC_OR,            // |
    PREC_AND,           // &
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * / %
    PREC_UNARY,         // ! -
    PREC_POW,           // ^
    PREC_CALL,          // . () []
    PREC_PRIMARY
} Precedence;

// A type for parse function pointer (which takes
// no parameter and return void).
typedef void (*ParseFn)(bool can_assign);

// Represent a row in the function pointer table
typedef struct {
    ParseFn parse_prefix;
    ParseFn parse_infix;
    Precedence infix_precedence;
} ParseRule;

typedef struct {
    Token var_name;     // The local variable name
    int depth;          // The scope depth of the local variable
    bool is_captured;   // Whether the variable is captured by a closure
} LocalVar;

// Type of "function environment"
typedef enum {
    TYPE_FUNCTION,
    TYPE_TOP_LEVEL,
} FunctionType;

// Struct to hold compilation information about an upvalue
typedef struct {
    uint8_t index;
    bool is_local;
} Upvalue;

// Struct to hold the metadata for a "function" being compiled
typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType func_type;
    LocalVar local_vars[UINT8_COUNT]; // To mirror the VM stack at runtime
    int local_var_count;
    int scope_depth;
    Upvalue upvalues[UINT8_COUNT]; // To mirror the array of ObjUpvalue at runtime
} Compiler;

Compiler* curr_compiler = NULL;

#define MAX_NESTED_FUNCTIONS 64 // Should be equals to VM's FRAMES_MAX
unsigned int n_nested_compiler = 0;

typedef struct {
    Token curr_token;
    Token prev_token;
    bool had_error;
    bool panicking;
} Parser;

Parser parser; // Singleton parser struct

//---------------------------------------
//  PRATT PARSER FUNCTION POINTER TABLE
//---------------------------------------

// Each row:
// [TOKEN_TYPE] = [prefix_parse_func, infix_parse_func, infix_precedence]
//
// Note that the precedence is only for the infix form. All prefix forms
// are assumed to have higher precedence than all infix forms.

// Necessary parse function prototypes:
static void parse_grouping(bool can_assign);
static void parse_unary(bool can_assign);
static void parse_binary(bool can_assign);
static void parse_power(bool can_assign);
static void parse_variable(bool can_assign);
static void parse_string_literal(bool can_assign);
static void parse_int_literal(bool can_assign);
static void parse_float_literal(bool can_assign);
static void parse_literal(bool can_assign);
static void parse_func_literal(bool can_assign);
static void parse_and(bool can_assign);
static void parse_or(bool can_assign);
static void parse_ternary(bool can_assign);
static void parse_call(bool can_assign);
// static void parse_dot(bool can_assign);
static void parse_down_triangle(bool can_assign);
static void parse_declaration();
static void parse_statement();

// The Pratt parser table is for parsing expressions (not statements)
ParseRule parse_rules[] = {
    [TOKEN_VAR]             = {NULL, NULL, PREC_NONE},
    [TOKEN_LOOP]            = {NULL, NULL, PREC_NONE},
    [TOKEN_QUESTION]        = {NULL, parse_ternary, PREC_TERNARY},
    [TOKEN_SEMICOLON]       = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_PAREN]      = {parse_grouping, parse_call, PREC_CALL},
    [TOKEN_RIGHT_PAREN]     = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_SQUARE]    = {NULL, NULL, PREC_NONE}, // TODO // TODO
    [TOKEN_DOT]             = {NULL, NULL, PREC_CALL}, // TODO parse_dot
    [TOKEN_COMMA]           = {NULL, NULL, PREC_NONE},
    [TOKEN_PIPE]            = {NULL, parse_or, PREC_OR},
    [TOKEN_AND]             = {NULL, parse_and, PREC_AND},
    [TOKEN_CARET]           = {NULL, parse_power, PREC_POW}, // Arithmetic power
    [TOKEN_PLUS]            = {NULL, parse_binary, PREC_TERM},
    [TOKEN_STAR]            = {NULL, parse_binary, PREC_FACTOR},
    [TOKEN_PERCENT]         = {NULL, parse_binary, PREC_FACTOR},
    [TOKEN_NULL]            = {parse_literal, NULL, PREC_NONE},
    [TOKEN_EQUAL]           = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     = {NULL, parse_binary, PREC_EQUALITY},
    [TOKEN_BANG]            = {parse_unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL, parse_binary, PREC_EQUALITY},
    [TOKEN_COLON]           = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE]            = {parse_literal, NULL, PREC_NONE},
    [TOKEN_FALSE]           = {parse_literal, NULL, PREC_NONE},
    [TOKEN_LESS]            = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_RETURN]          = {NULL, NULL, PREC_NONE},
    [TOKEN_READ]            = {NULL, NULL, PREC_NONE}, // TODO // TODO
    [TOKEN_READ_BOOL]       = {NULL, NULL, PREC_NONE}, // TODO // TODO
    [TOKEN_READ_NUM]        = {NULL, NULL, PREC_NONE}, // TODO // TODO
    [TOKEN_SLASH]           = {NULL, parse_binary, PREC_FACTOR},
    [TOKEN_UP_TRIANGLE]     = {parse_func_literal, NULL, PREC_NONE},
    [TOKEN_BACK_SLASH]      = {NULL, NULL, PREC_NONE},
    [TOKEN_DOWN_TRIANGLE]   = {parse_down_triangle, NULL, PREC_NONE},
    [TOKEN_MINUS]           = {parse_unary, parse_binary, PREC_TERM},
    [TOKEN_ARROW]           = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER]         = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_2_GREATER]       = {NULL, NULL, PREC_NONE},
    [TOKEN_3_GREATER]       = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_SQUARE]     = {NULL, NULL, PREC_NONE}, // TODO // TODO
    [TOKEN_TABLE]           = {NULL, NULL, PREC_NONE}, // TODO // TODO
    [TOKEN_IDENTIFIER]      = {parse_variable, NULL, PREC_NONE},
    [TOKEN_INT]             = {parse_int_literal, NULL, PREC_NONE},
    [TOKEN_FLOAT]           = {parse_float_literal, NULL, PREC_NONE},
    [TOKEN_STRING]          = {parse_string_literal, NULL, PREC_NONE},
    [TOKEN_ERROR]           = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF]             = {NULL, NULL, PREC_NONE},
};

//------------------------------
//      UTILITY FUNCTIONS
//------------------------------

// Report error at the passed token.
static void error_at(Token* token, const char* msg) {
    // Don't report errors if already in panic mode
    if (parser.panicking) return;

    parser.panicking = true;

    // Print the line number of the token
    fprintf(stderr, "[Line %d] Error", token->line_num);

    // Check for token type (and optionally print "at end" or the lexeme)
    if (token->type == TOKEN_EOF) {
        // End of file
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR) {
        // Error token: The lexeme is actually the error
        // message created by the scanner, so no need to
        // print the lexeme.

        // Nothing to do here!
    }
    else {
        // Other token types --> Print the lexeme
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    // Print the passed error message
    fprintf(stderr, ": %s\n", msg);

    parser.had_error = true;
}

// Report error at the current token.
static void error_curr_token(const char* msg) {
    error_at(&parser.curr_token, msg);
}

// Report error at the previous token.
static void error_prev_token(const char* msg) {
    error_at(&parser.prev_token, msg);
}

// Move the parser's "cursor" to the next token,
// while report and skip all error tokens.
static void next_token() {
    parser.prev_token = parser.curr_token;

    for (;;) {
        parser.curr_token = scan_next_token();

        // Check for error tokens
        if (parser.curr_token.type == TOKEN_ERROR) {
            error_curr_token(parser.curr_token.start);
        }
        else {
            break;
        }
    }
}

// Check the type of the next token and return true
// if it matches, otherwise return false.
static bool check_next_token(TokenType type) {
    return parser.curr_token.type == type;
}

// Check and consume the next token if it matches
// the passed TokenType.
static bool match_next_token(TokenType type) {
    if (!check_next_token(type)) return false;

    next_token(); // Matched --> Consume
    return true;
}

// Check and consume a token of the passed TokenType,
// and report an error if not found.
static void consume_mandatory(TokenType type, const char* error_msg) {
    if (parser.curr_token.type == type) {
        next_token();
        return;
    }

    // Expected token not found
    error_curr_token(error_msg);
}

// Create a synthetic token that doesn't exist in the source code.
// (The token type and line number don't matter and are ignored).
static Token synthetic_token(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

//-------------------------------------
//    BYTECODE COMPILING FUNCTIONS
//-------------------------------------

// Return a pointer to the currently being-compiled chunk.
static CodeChunk* current_chunk() {
    // The purpose of this function is to encapsulate
    // the notion of the "current chunk" inside this
    // function.
    return &curr_compiler->function->chunk;
}

// Add a byte to the currently being-compiled chunk.
static void emit_byte(uint8_t byte) {
    append_chunk(current_chunk(), byte, parser.prev_token.line_num);
}

// Add two bytes to the currently being-compiled chunk.
static void emit_two_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

// Add OP_RETURN to the currently being-compiled chunk.
static void emit_op_return() {
    // Implicit null return
    emit_byte(OP_NULL);
    emit_byte(OP_RETURN);
}

// Add a constant to the contant pool of the current chunk
// and return the corresponding constant index.
static uint8_t add_constant_to_pool(IcoValue val) {
    // Add the constant to the pool and get the constant index
    int constant_idx = add_constant(current_chunk(), val);

    // Check if the pool has enough spaces
    if (constant_idx > UINT8_MAX) {
        error_prev_token("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant_idx;
}

// Add OP_CONSTANT to the currently being-compiled chunk
// and add the value to the the constant pool of the chunk.
static void emit_constant(IcoValue val) {
    // Add OP_CONSTANT and the constant index to the chunk
    emit_two_bytes(OP_CONSTANT, add_constant_to_pool(val));
}

// Emit a jump instruction and 2 placeholder operand bytes,
// then return the chunk offset right after the jump opcode.
static int emit_jump(uint8_t opcode) {
    emit_byte(opcode);

    // Placeholder for jump offset
    emit_byte(0xff);
    emit_byte(0xff);

    return current_chunk()->size - 2;
}

// Patch a jump instruction in the chunk with the correct jump distance/offset,
// which is from the jump opcode to the most recently added bytecode.
static void patch_jump(int offset) {
    // -2 to adjust for the 2 bytes of the jump distance itself
    int dist = current_chunk()->size - offset - 2;

    if (dist > UINT16_MAX) {
        error_prev_token("Too much bytecode to jump over.");
    }

    // Convert the jump distance to an unsigned short
    // byte-by-byte and put it in the chunk
    current_chunk()->chunk[offset] = (dist >> 8) & 0xff;
    current_chunk()->chunk[offset + 1] = dist & 0xff;
}

// Emit an OP_LOOP that takes the instruction pointer
// back to the passed offset (loop_start).
static void emit_loop(int loop_start) {
    emit_byte(OP_LOOP);

    // Calculate the offset. +2 to account for the 2-byte offset.
    int offset = current_chunk()->size + 2 - loop_start;
    if (offset > UINT16_MAX) error_prev_token("Loop body too large.");

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

// Initialize a new compiler struct, and set the current one
// to be its parent ("enclosing").
static void init_compiler(Compiler* compiler, FunctionType type, const char* name, int length) {
    compiler->enclosing = curr_compiler;
    compiler->function = NULL;
    compiler->func_type = type;
    compiler->local_var_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function_obj(); // Immediately reassign bc GC stuff
    curr_compiler = compiler;

    if (type != TYPE_TOP_LEVEL) {
        curr_compiler->function->name = copy_and_create_str_obj(name, length);
    }

    // Use the first slot in the call frame for the ObjFunction
    // or for "this" in case of methods
    LocalVar* local_vars = &curr_compiler->local_vars[curr_compiler->local_var_count++];
    local_vars[0].depth = 0;
    local_vars[0].is_captured = false;
    if (type == TYPE_FUNCTION) {
        local_vars[0].var_name.start = "\\/"; // For recursion in anonymous function
        local_vars[0].var_name.length = 2;
    }
    else {
        local_vars[0].var_name.start = "";
        local_vars[0].var_name.length = 0;
    }

    n_nested_compiler++;
    if (n_nested_compiler > MAX_NESTED_FUNCTIONS) {
        error_prev_token("Too many nested functions.");
    }
}

// Finish compiling the current chunk (and optionally
// print bytecode dumps if in debug mode).
static ObjFunction* end_compiler() {
    emit_op_return();

    // Get the compiling "function"
    ObjFunction* function = curr_compiler->function;

#ifdef DEBUG_PRINT_BYTECODE
    if (!parser.had_error) {
        disass_chunk(current_chunk(),
        function->name != NULL? function->name->chars : "<top level script>");
    }
#endif

    // Restore the enclosing compiler struct as the current one
    curr_compiler = curr_compiler->enclosing;
    n_nested_compiler--;

    return function;
}

//-------------------------------
//       PARSE FUNCTIONS
//-------------------------------

// Return the ParseRule, which is a row in the function pointer
// table of Pratt's parser, for the given token type.
static ParseRule* get_rule(TokenType token_type) {
    return &parse_rules[token_type];
}

// Parse and compile an expression with precedence
// higher than or equal to the passed precedence.
static void parse_expr_with_precedence(Precedence precedence) {
    // Advance to the next token
    next_token();

    // Parse a prefix expression first. Note that constants
    // and variables are also considered prefix expressions.
    ParseFn parse_prefix = get_rule(parser.prev_token.type)->parse_prefix;

    // No prefix parse function found in the table for
    // the current token --> Syntax error
    if (parse_prefix == NULL) {
        error_prev_token("Expect expression.");
        return;
    }

    // Check if current INFIX precedence allow variable assignment
    // (The only case when this is true is when we have assignment
    // chain, like a = b = 5).
    bool can_assign = precedence <= PREC_ASSIGNMENT;

    // The current token DOES have a prefix parse function
    parse_prefix(can_assign);

    // Check and parse an infix expression if present. The
    // prefix expression we just parsed will be used as the
    // left operand.
    //
    // If the current token doesn't have an infix rule, it
    // will also have the LOWEST precedence, which stops the
    // loop.
    while (precedence <= get_rule(parser.curr_token.type)->infix_precedence) {
        next_token();
        ParseFn parse_infix = get_rule(parser.prev_token.type)->parse_infix;
        parse_infix(can_assign);
    }


    // can_assign is always true at top-level due to parse_expression()
    // calling this function with PREC_ASSIGNMENT. If can_assign is false,
    // the "=" will not be consumed, so when control returns to the
    // top level call of this function, we will have can_assign=true
    // and the remaining "=", which will be catched as follows.

    if (can_assign && match_next_token(TOKEN_EQUAL)) {
        error_prev_token("Invalid assignment target.");
    }
}

// Parse and compile a single expression.
static void parse_expression() {
    // Assignment is the lowest precedence level for an expression
    parse_expr_with_precedence(PREC_ASSIGNMENT);
}

// Parse and compile an int literal.
static void parse_int_literal(bool can_assign) {
    long i = strtol(parser.prev_token.start, NULL, 10);
    emit_constant(INT_VAL(i));
}

// Parse and compile a float literal
static void parse_float_literal(bool can_assign) {
    double f = strtod(parser.prev_token.start, NULL);
    emit_constant(FLOAT_VAL(f));
}

// Parse and compile a boolean or nil literal.
static void parse_literal(bool can_assign) {
    switch (parser.prev_token.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NULL:  emit_byte(OP_NULL); break;
        case TOKEN_TRUE:  emit_byte(OP_TRUE); break;

        default: return; // Unreachable
    }
}

// Parse and compile a grouping, ie. "()".
static void parse_grouping(bool can_assign) {
    // The opening '(' has been consumed.
    // Proceed to consume the inner expression.
    parse_expression();

    consume_mandatory(TOKEN_RIGHT_PAREN, "Expect closing ')'.");
}

// Parse and compile a unary expression.
static void parse_unary(bool can_assign) {
    // Assume the operator has been consumed
    TokenType operator_type = parser.prev_token.type;

    // Parse the operand. Use unary precedence level
    // to allow things like "--5" or "!!isEmpty".
    parse_expr_with_precedence(PREC_UNARY);

    // Emit the corresponding opcode
    switch (operator_type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        case TOKEN_BANG:  emit_byte(OP_NOT); break;

        default:          return;// Unreachable
    }
}

// Parse and compile a binary expression.
static void parse_binary(bool can_assign) {
    // Assume the operator has been consumed and
    // the left operand has been compiled.
    // (See parse_expr_with_precedence().)
    TokenType operator_type = parser.prev_token.type;

    // Get the function pointer table row for the operator
    ParseRule* rule = get_rule(operator_type);

    // Parse the right operand with one precedence level higher
    // than the precedence level of the current operator
    parse_expr_with_precedence((Precedence)(rule->infix_precedence + 1));

    switch (operator_type) {
        // Arithmetic operations
        case TOKEN_PLUS:    emit_byte(OP_ADD); break;
        case TOKEN_MINUS:   emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:    emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH:   emit_byte(OP_DIVIDE); break;
        case TOKEN_PERCENT: emit_byte(OP_MODULO); break;

        // Comparison operations
        case TOKEN_BANG_EQUAL:    emit_two_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER:       emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_two_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emit_two_bytes(OP_GREATER, OP_NOT); break;

        default:  /// Unreachable
            return;
    }
}

static void parse_power(bool can_assign) {
    // Parse the right operand with one precedence level higher
    parse_expr_with_precedence(PREC_POW);
    emit_byte(OP_POWER);
}

// Parse and compile a string literal.
static void parse_string_literal(bool can_assign) {
    // "+ 1" and "- 2" are to trim the double-quotes of
    // the string literal lexeme.
    ObjString* obj_str = copy_and_create_str_obj(
        parser.prev_token.start + 1,
        parser.prev_token.length - 2
    );
    emit_constant(OBJ_VAL(obj_str));
}

// Synchronize the parser to a new statement when there
// is an error and the parser is in panic mode.
static void synchronize() {
    parser.panicking = false;

    while (parser.curr_token.type != TOKEN_EOF) {
        // Reset at new statement
        if (parser.prev_token.type == TOKEN_SEMICOLON) {
            return;
        }

        // Also detect new statement with some tokens
        switch (parser.curr_token.type) {
            case TOKEN_LOOP:
            case TOKEN_BACK_SLASH:
            case TOKEN_2_GREATER:
            case TOKEN_3_GREATER:
            case TOKEN_RETURN:
            case TOKEN_VAR:
                return;

            default:
                ; // Do nothing.
        }

        next_token();
    }
}

// Add an identifier name to the constant pool from its
// lexeme in the source code, then return the constant index.
static uint8_t identifier_constant_index(Token* token) {
    return add_constant_to_pool(
        OBJ_VAL(copy_and_create_str_obj(token->start, token->length))
    );
}

// Return two if two identifiers are the same
static bool identifiers_equal(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// Add a local variable to the array of local variables
// of the compiler struct (for resolving purpose).
static void add_local_var(Token var_name) {
    // If not enough stack space
    if (curr_compiler->local_var_count == UINT8_COUNT) {
        error_prev_token("Too many local variables in function.");
        return;
    }

    LocalVar* local_var = &curr_compiler->local_vars[curr_compiler->local_var_count++];
    local_var->var_name = var_name;
    local_var->depth = -1; // Declare but don't initialize yet
    local_var->is_captured = false;
}

// Mark the most recently added (aka. on top of the VM stack)
// local variable as initialized.
static void mark_initialized() {
    if (curr_compiler->scope_depth == 0) return; // skip global vars
    curr_compiler->local_vars[curr_compiler->local_var_count - 1].depth
        = curr_compiler->scope_depth;
}

// Declare a local variable (ie. add it to the list of locals).
// Skip if it is a global var.
static void declare_variable() {
    // Skip if it is a global var
    if (curr_compiler->scope_depth == 0) return;

    // Check for any declared var with the same name
    Token* var_name = &parser.prev_token;
    for (int i = curr_compiler->local_var_count - 1; i >= 0; i--) {
        LocalVar* curr_var = &curr_compiler->local_vars[i];

        // Going out of current scope -> Break
        if (curr_var->depth != -1 && curr_var->depth < curr_compiler->scope_depth) {
            break;
        }

        // Found a declared var with the same name
        if (identifiers_equal(var_name, &curr_var->var_name)) {
            error_prev_token("Already a variable with this name in this scope.");
        }
    }

    // Record the var to the array of local vars of struct Compiler
    add_local_var(*var_name);
}

// Parse the name of a global or local variable and declare it.
// Return the constant index in the constant pool of the name,
// or return 0 if it is a local variable.
static uint8_t parse_var_name(const char* error_msg) {
    consume_mandatory(TOKEN_IDENTIFIER, error_msg);

    // Declare and return if this is a local variable
    declare_variable();
    if (curr_compiler->scope_depth > 0) return 0;

    // Return the constant index of the var name if this is a global variable.
    return identifier_constant_index(&parser.prev_token);
}

// Emit bytecode instruction for global variable initialization,
// or mark a local variable as initialized.
static void define_variable(uint8_t var_name_const_idx) {
    // Don't need to emit any bytecode for local vars
    if (curr_compiler->scope_depth > 0) {
        mark_initialized();
        return;
    }

    // Bytecode for defining global variable
    emit_two_bytes(OP_DEFINE_GLOBAL, var_name_const_idx);
}

// Resolve a local variable and return its index on the
// VM stack, or return -1 if it's not a local variable
static int resolve_local(Compiler* compiler, Token* var_name) {
    // Linear search the array of local vars.
    // Walk backward to find the latest declared variable,
    // which allows shadowing to work.
    for (int i = compiler->local_var_count - 1; i >= 0; i--) {
        LocalVar* curr_var = &compiler->local_vars[i];
        if (identifiers_equal(var_name, &curr_var->var_name)) {
            // Error if variable has not been initialized
            if (curr_var->depth == -1) {
                error_prev_token("Can't read local variable in its own initializer.");
            }

            // Else return its stack index
            return i;
        }
    }

    // Not found -> Global var
    return -1;
}

// Add either a local variable or an upvalue of the immediately enclosing
// function (depending on is_local) as an upvalue of the current compiler
// and return the corresponding upvalue index.
static int add_upvalue(Compiler* compiler, uint8_t idx, bool is_local) {
    int upvalue_count = compiler->function->upvalue_count;

    // Check for existing upvalue for the same variable
    for (int i = 0; i < upvalue_count; i++) {
        Upvalue* curr_upvalue = &compiler->upvalues[i];
        if (curr_upvalue->index == idx && curr_upvalue->is_local == is_local) {
            // Found one
            return i;
        }
    }

    // Check for too many upvalues
    if (upvalue_count == UINT8_COUNT) {
        error_prev_token("Too many closure variables in this function.");
        return 0;
    }

    // Otherwise, add a new upvalue
    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = idx;
    return compiler->function->upvalue_count++;
}

// Resolve an upvalue and return its upvalue index in the upvalue
// array, or return -1 if it's not found.
static int resolve_upvalue(Compiler* compiler, Token* var_name) {
    // No enclosing function (aka top-level code)
    if (compiler->enclosing == NULL) return -1;

    // Try to resolve the upvalue as a local var of the immediately
    // enclosing function.
    int upper_local = resolve_local(compiler->enclosing, var_name);
    if (upper_local != -1) {
        compiler->enclosing->local_vars[upper_local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)upper_local, true);
    }

    // Otherwise, try to resolve as an upvalue of the immediately
    // enclosing function (aka. transitive upvalue).
    int upper_upvalue = resolve_upvalue(compiler->enclosing, var_name);
    if (upper_upvalue != -1) {
        // The transitive upvalue will be added to ALL intermediate functions
        return add_upvalue(compiler, (uint8_t)upper_upvalue, false);
    }

    return -1;
}

// Helper function for parse_variable. Parse and compile a variable
// usage with the correct variable type (local, upvalue, or global).
static void named_variable(Token name, bool can_assign) {
    // Deciding between global and local variable
    uint8_t get_op, set_op;
    int arg = resolve_local(curr_compiler, &name); // Scope depth if local
    if (arg != -1) { // Local
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else if ((arg = resolve_upvalue(curr_compiler, &name)) != -1) { // Upvalue
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    }
    else { // Global
        arg = identifier_constant_index(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    // Set or Get?
    if (can_assign && match_next_token(TOKEN_EQUAL)) { // Set
        // Compile the right-hand-side expression
        parse_expression();
        emit_two_bytes(set_op, (uint8_t)arg);
    }
    else { // Get
        emit_two_bytes(get_op, (uint8_t)arg);
    }
}

// Parse and compile a variable access expression.
static void parse_variable(bool can_assign) {
    named_variable(parser.prev_token, can_assign);
}

// Parse and compile a print statement.
static void parse_print_stmt(bool is_println) {
    // Assume the print token has been consumed
    parse_expression();

    // ";" at the end
    consume_mandatory(TOKEN_SEMICOLON, "Expect ';' at the end of statement.");
    emit_byte(is_println ? OP_PRINTLN : OP_PRINT);
}

// Parse and compile a block statement. Assume the left '{' has been
// consumed. Will consume the right '}'.
static void parse_block() {
    while (!check_next_token(TOKEN_RIGHT_BRACE) && !check_next_token(TOKEN_EOF)) {
        parse_declaration();
    }
    consume_mandatory(TOKEN_RIGHT_BRACE, "Expect '}' after a block.");
}

// Begin a new scope level
static void begin_scope() {
    // Increment the scope depth
    curr_compiler->scope_depth++;
}

// End a new scope level
static void end_scope() {
    // Decrement the scope depth
    curr_compiler->scope_depth--;

    // "Deallocate" all local variables of the current scope
    // (by adding OP_POPs and decrementing count)
    while (curr_compiler->local_var_count > 0 &&
           curr_compiler->local_vars[curr_compiler->local_var_count - 1].depth
                > curr_compiler->scope_depth) {
        // Emit OP_POP for normal local vars and OP_CLOSE_UPVALUE
        // for captured local vars.
        if (curr_compiler->local_vars[curr_compiler->local_var_count - 1].is_captured) {
            emit_byte(OP_CLOSE_UPVALUE);
        }
        else {
            emit_byte(OP_POP);
        }

        curr_compiler->local_var_count--;
    }
}

// Parse and compile an expression statement.
static void parse_expression_stmt() {
    parse_expression();
    consume_mandatory(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

// Parse and compile an if statement.
// Grammar: if -> "\ " expr "?" stmt (":" stmt)? ;
static void parse_if_stmt() {
    // The condition
    parse_expression();
    consume_mandatory(TOKEN_QUESTION, "Expect '?' after condition.");

    // Parse the then branch
    int then_jump_offset = emit_jump(OP_JUMP_IF_FALSE); // to jump through then
    emit_byte(OP_POP); // to pop the condition expr in the then branch
    parse_statement();
    int else_jump_offset = emit_jump(OP_JUMP); // to jump through else

    // Need to patch AFTER emitting the above OP_JUMP
    patch_jump(then_jump_offset);

    // Optionally parse the else branch
    emit_byte(OP_POP); // to pop the condition expr in the else branch
    if (match_next_token(TOKEN_COLON)) parse_statement();
    patch_jump(else_jump_offset);
}

// Parse and compile an and operation.
static void parse_and(bool can_assign) {
    int end_jump_offset = emit_jump(OP_JUMP_IF_FALSE);

    // If left operand is true --> No short circuit
    emit_byte(OP_POP);
    parse_expr_with_precedence(PREC_AND); // right operand

    // Patch jump after compiling the right operand
    patch_jump(end_jump_offset);
}

// Parse and compile an or operation.
static void parse_or(bool can_assign) {
    // If left operand is falsey, make a tiny jump to
    // evaluate the right operand
    int falsey_jump_offset = emit_jump(OP_JUMP_IF_FALSE);

    // Else (left operand is truthy), jump through to the end
    int end_jump_offset = emit_jump(OP_JUMP);

    patch_jump(falsey_jump_offset);
    emit_byte(OP_POP); // discard left operand

    parse_expr_with_precedence(PREC_OR); // right operand
    patch_jump(end_jump_offset);
}

// Parse and compile a ternary expression
static void parse_ternary(bool can_assign) {
    // The condition expression and the '?' is already parsed

    int then_jump_offset = emit_jump(OP_JUMP_IF_FALSE);

    // Then branch
    emit_byte(OP_POP); // Pop the condition
    parse_expr_with_precedence(PREC_OR);
    int else_jump_offset = emit_jump(OP_JUMP); // Skip the else branch

    // Else branch
    patch_jump(then_jump_offset);
    consume_mandatory(TOKEN_COLON, "Expect ':' in ternary expression.");
    emit_byte(OP_POP);
    parse_expr_with_precedence(PREC_OR);
    patch_jump(else_jump_offset);
}

// Parse and compile a while loop.
// Grammar: loop -> "@" expr ":" stmt ;
static void parse_while_stmt() {
    // The position in the bytecode that is the start of the loop
    int loop_start = current_chunk()->size;

    // The loop condition
    parse_expression();
    consume_mandatory(TOKEN_COLON, "Expect ':' after loop condition.");

    // Jump to exit the loop when the condition is false
    int exit_jump_offset = emit_jump(OP_JUMP_IF_FALSE);

    // Loop body
    emit_byte(OP_POP); // pop the loop condition
    parse_statement();
    emit_loop(loop_start);

    // For exitting the loop
    patch_jump(exit_jump_offset);
    emit_byte(OP_POP); // pop the loop condition
}

// Parse and compile a return statement
static void parse_return_stmt() {
    // Can't return from top-level code
    if (curr_compiler->func_type == TYPE_TOP_LEVEL) {
        error_prev_token("Can't return from top-level code.");
    }

    if (match_next_token(TOKEN_SEMICOLON)) {
        // No return value
        emit_op_return();
    }
    else {
        parse_expression(); // the return value expr
        consume_mandatory(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
}

// Parse and compile a statement-level statement.
static void parse_statement() {
    if (match_next_token(TOKEN_2_GREATER)) {
        parse_print_stmt(false);
    }
    else if (match_next_token(TOKEN_3_GREATER)) {
        parse_print_stmt(true);
    }
    else if (match_next_token(TOKEN_BACK_SLASH)) {
        parse_if_stmt();
    }
    else if (match_next_token(TOKEN_LOOP)) {
        parse_while_stmt();
    }
    else if (match_next_token(TOKEN_RETURN)) {
        parse_return_stmt();
    }
    else if (match_next_token(TOKEN_LEFT_BRACE)) {
        begin_scope();
        parse_block();
        end_scope();
    }
    else {
        parse_expression_stmt();
    }
}

// Compile a function literal into a bytecode chunk and store
// the resulting ObjFunction in the constant pool.
static void compile_function(FunctionType type, const char* name, int length) {
    // Start a new compiler struct for the function being compiled
    Compiler func_compiler;
    init_compiler(&func_compiler, type, name, length);
    begin_scope();

    // Compile the parameters
    if (!check_next_token(TOKEN_ARROW)) {
        do {
            // Increment the function's arity
            curr_compiler->function->arity++;
            if (curr_compiler->function->arity > 255) {
                error_curr_token("Can't have more than 255 parameters.");
            }

            // Parse the parameter name
            uint8_t constant = parse_var_name("Expect parameter name.");
            define_variable(constant);
        }
        while (match_next_token(TOKEN_COMMA));
    }
    consume_mandatory(TOKEN_ARROW, "Expect '->' after function parameters.");

    // Compile the function body
    if (match_next_token(TOKEN_LEFT_BRACE)) { // Block as body
        parse_block();
    }
    else { // Expression as body
        parse_expression();
        emit_byte(OP_RETURN);
    }

    // Store the resulting ObjFunction in the constant pool of the surrounding function
    ObjFunction* result_func = end_compiler();
    emit_two_bytes(OP_CLOSURE, add_constant_to_pool(OBJ_VAL(result_func)));

    // Emit 2 bytes [is_local][index] for each upvalue used
    for (int i = 0; i < result_func->upvalue_count; i++) {
        emit_byte(func_compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(func_compiler.upvalues[i].index);
    }
}

// Parse and compile a function literal
// Grammar: function -> "/\ " IDENTIFIER* "->" (expr | block);
static void parse_func_literal(bool can_assign) {
    // The "/\" is already consumed.
    compile_function(TYPE_FUNCTION, "/\\", 2);
}

// Parse and compile a '\/' symbol
static void parse_down_triangle(bool can_assign) {
    // '\/' is treated exactly like a local variable. Note that the name '\/'
    // only exists during scanning and compiling phases for resolving purpose.
    parse_variable(false);
}

// Parse a list of arguments for a function call
// and return the number of arguments.
static uint8_t parse_arg_list() {
    // Assume the left '(' is already consumed
    uint8_t arg_count = 0;
    if (!check_next_token(TOKEN_RIGHT_PAREN)) {
        do {
            // Parse one argument and increment the count
            parse_expression();
            if (arg_count == 255)
                error_prev_token("Can't have more than 255 arguments.");
            arg_count++;
        } while (match_next_token(TOKEN_COMMA));
    }
    consume_mandatory(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

    return arg_count;
}

// Parse and compile a function call
static void parse_call(bool can_assign) {
    uint8_t arg_count = parse_arg_list();
    emit_two_bytes(OP_CALL, arg_count);
}
/*
// Parse and compile a dot-notation for property access
// of a Lox instance.
static void parse_dot(bool can_assign) {
    // Parse the property's name
    consume_mandatory(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name_const_idx = identifier_constant_index(&parser.prev_token);

    if (can_assign && match_next_token(TOKEN_EQUAL)) {
        parse_expression();
        emit_two_bytes(OP_SET_PROPERTY, name_const_idx);
    }
    else if (match_next_token(TOKEN_LEFT_PAREN)) {
        // Switch to faster invocation
        uint8_t arg_count = parse_arg_list();
        emit_two_bytes(OP_INVOKE, name_const_idx);
        emit_byte(arg_count);
        // The bytecode should be like:
        // [code to push "this"][code to push arg list][op_invoke] <- stack top
    }
    else {
        emit_two_bytes(OP_GET_PROPERTY, name_const_idx);
    }
}
*/

// Parse and compile a variable declaration.
static void parse_var_decl() {
    uint8_t arg = parse_var_name("Expect variable name.");
    Token var_name = parser.prev_token;

    // Prepare the initialization value (or nil if not available)
    if (match_next_token(TOKEN_EQUAL)) {
        if (match_next_token(TOKEN_UP_TRIANGLE)) { // curr token is '/\'
            // Special case: create a function with the same name
            compile_function(TYPE_FUNCTION, var_name.start, var_name.length);
        }
        else {
            parse_expression();
        }
    }
    else {
        emit_byte(OP_NULL);
    }

    consume_mandatory(TOKEN_SEMICOLON, "Expect ';' after variable declaration;");
    define_variable(arg); // OP_DEFINE_GLOBAL or mark initialized local
}

// Parse and compile a declaration-level statement.
// Grammar: decl -> var_decl | stmt;
static void parse_declaration() {
    if (match_next_token(TOKEN_VAR)) {
        parse_var_decl();
    }
    else {
        parse_statement();
    }

    // To synchronize when there is a compile error
    if (parser.panicking) synchronize();
}

//----------------------------------
//     THE ONE HEADER FUNCTION
//----------------------------------

ObjFunction* compile(const char *source_code) {
    // Initialize the scanner, which will be used by the parser
    init_scanner(source_code);

    // Initialize the parser and compiler
    Compiler compiler;
    init_compiler(&compiler, TYPE_TOP_LEVEL, NULL, 0);
    parser.panicking = false;
    parser.had_error = false;

    // Make the parser get the first token
    next_token();

    // Parsing declaration-level statements until EOF
    while (!match_next_token(TOKEN_EOF)) {
        parse_declaration();
    }

    // End of the compiling process
    ObjFunction* result_func = end_compiler();
    return parser.had_error ? NULL : result_func;
}

// void mark_compiler_roots() {
//     Compiler* compiler = curr_compiler;

//     // Mark the ObjFunction of all linked compiler structs
//     while (compiler != NULL) {
//         mark_object((Obj*)compiler->function);
//         compiler = compiler->enclosing;
//     }
// }