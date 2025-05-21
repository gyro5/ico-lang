#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clox_common.h"
#include "clox_compiler.h"
#include "clox_scanner.h"
#include "clox_value.h"
#include "clox_object.h"
#include "clox_memory.h"

#ifdef DEBUG_PRINT_BYTECODE
#include "clox_debug.h"
#endif

// Note that C implicitly creates enum values
// from lowest to highest, which is what we want.
typedef enum {
    PREC_NONE,          // lowest precedence level
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // ! -
    PREC_CALL,          // . ()
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
    LoxToken curr_token;
    LoxToken prev_token;
    bool had_error;
    bool panicking;
} Parser;

typedef struct {
    LoxToken var_name;  // The local variable name
    int depth;          // The scope depth of the local variable
    bool is_captured;   // Whether the variable is captured by a closure
} LocalVar;

// Type of "function environment"
typedef enum {
    TYPE_FUNCTION,
    TYPE_TOP_LEVEL,
    TYPE_METHOD,
    TYPE_INIT, // To have special return bytecode for initializers
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

// Struct to track (nested) class declarations
typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool has_superclass;
} ClassCompiler;

// Singleton parser struct
Parser parser;

Compiler* curr_compiler = NULL;
ClassCompiler* curr_class_compiler = NULL;

// The currently being-compiled chunk
// NOTE: this will change later when we need to compile user-defined chunk
CodeChunk* compiling_chunk;

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
static void parse_variable(bool can_assign);
static void parse_string_literal(bool can_assign);
static void parse_number_literal(bool can_assign);
static void parse_literal(bool can_assign);
static void parse_and(bool can_assign);
static void parse_or(bool can_assign);
static void parse_call(bool can_assign);
static void parse_dot(bool can_assign);
static void parse_this(bool can_assign);
static void parse_super(bool can_assign);
static void parse_declaration();
static void parse_statement();

ParseRule parse_rules[] = {
    [TOKEN_LEFT_PAREN]    = {parse_grouping, parse_call, PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA]         = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT]           = {NULL, parse_dot, PREC_CALL},
    [TOKEN_MINUS]         = {parse_unary, parse_binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL, parse_binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH]         = {NULL, parse_binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL, parse_binary, PREC_FACTOR},
    [TOKEN_BANG]          = {parse_unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL, parse_binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL, parse_binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {parse_variable, NULL, PREC_NONE},
    [TOKEN_STRING]        = {parse_string_literal, NULL, PREC_NONE},
    [TOKEN_NUMBER]        = {parse_number_literal, NULL, PREC_NONE},
    [TOKEN_AND]           = {NULL, parse_and, PREC_AND},
    [TOKEN_CLASS]         = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE]          = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE]         = {parse_literal, NULL, PREC_NONE},
    [TOKEN_FOR]           = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN]           = {NULL, NULL, PREC_NONE},
    [TOKEN_IF]            = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL]           = {parse_literal, NULL, PREC_NONE},
    [TOKEN_OR]            = {NULL, parse_or, PREC_OR},
    [TOKEN_PRINT]         = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN]        = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER]         = {parse_super, NULL, PREC_NONE},
    [TOKEN_THIS]          = {parse_this, NULL, PREC_NONE},
    [TOKEN_TRUE]          = {parse_literal, NULL, PREC_NONE},
    [TOKEN_VAR]           = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE]         = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR]         = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF]           = {NULL, NULL, PREC_NONE},
};

//------------------------------
//      UTILITY FUNCTIONS
//------------------------------

// Report error at the passed token.
static void error_at(LoxToken* token, const char* msg) {
    // Don't report errors if already in panic mode
    if (parser.panicking) {
        return;
    }

    parser.panicking = true;

    // Print the line number of the token
    fprintf(stderr, "[Line %d] Error", token->line_num);

    // Check for token type (and optionally print "at end"
    // or the lexeme)
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
        parser.curr_token = scan_token();

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

    // Matched --> Consume
    next_token();
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
static LoxToken synthetic_token(const char* text) {
    LoxToken token;
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
    if (curr_compiler->func_type == TYPE_INIT) {
        // Always return "this" in initializers
        emit_two_bytes(OP_GET_LOCAL, 0); // "this" always in stack slot 0
    }
    else {
        // Implicit nil return in all other cases
        emit_byte(OP_NIL);
    }
    emit_byte(OP_RETURN);
}

// Add a constant to the contant pool of the current chunk
// and return the corresponding constant index.
static uint8_t add_constant_to_pool(Value val) {
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
static void emit_constant(Value val) {
    // Add OP_CONSTANT and the constant index to the chunk
    emit_two_bytes(OP_CONSTANT, add_constant_to_pool(val));
}

// Initialize a new compiler struct, and set the current one
// to be its parent ("enclosing").
static void init_compiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = curr_compiler;
    compiler->function = NULL;
    compiler->func_type = type;
    compiler->local_var_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function_obj(); // Immediately reassign bc garbage collection stuff
    curr_compiler = compiler;

    // parse_func_decl() parses the function name's token, then compile_function()
    //  will call this function to initialize a new compiler struct.
    // --> So we can set the function name here using the previous token.
    if (type != TYPE_TOP_LEVEL) {
        curr_compiler->function->name = copy_and_create_str_obj(
            parser.prev_token.start, parser.prev_token.length);
    }

    // Use the first slot in the call frame for the ObjFunction
    // or for "this" in case of methods
    LocalVar* local_vars = &curr_compiler->local_vars[curr_compiler->local_var_count++];
    local_vars[0].depth = 0;
    local_vars[0].is_captured = false;
    if (type == TYPE_FUNCTION) {
        local_vars[0].var_name.start = ""; // empty name so that user can't overwrite
        local_vars[0].var_name.length = 0;
    }
    else {
        local_vars[0].var_name.start = "this";
        local_vars[0].var_name.length = 4;
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

    // Check if current precedence allow variable assignment
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

    /*
    can_assign is always true at top-level due to parse_expression()
    calling this function with PREC_ASSIGNMENT. If can_assign is false,
    the "=" will not be consumed, so when control returns to the
    top level call of this function, we will have can_assign=true
    and the remaining "=", which will be catched as follows.
    */
    if (can_assign && match_next_token(TOKEN_EQUAL)) {
        error_prev_token("Invalid assignment target.");
    }
}

// Parse and compile a single expression.
static void parse_expression() {
    // Assignment is the lowest precedence level for an expression
    parse_expr_with_precedence(PREC_ASSIGNMENT);
}

// Parse and compile a number literal.
static void parse_number_literal(bool can_assign) {
    double val = strtod(parser.prev_token.start, NULL);
    emit_constant(number_val(val));
}

// Parse and compile a boolean or nil literal.
static void parse_literal(bool can_assign) {
    switch (parser.prev_token.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NIL: emit_byte(OP_NIL); break;
        case TOKEN_TRUE: emit_byte(OP_TRUE); break;

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
        case TOKEN_MINUS:
            emit_byte(OP_NEGATE);
            break;

        case TOKEN_BANG:
            emit_byte(OP_NOT);
            break;

        default:  // Unreachable
            return;
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

// Parse and compile a string literal.
static void parse_string_literal(bool can_assign) {
    // "+ 1" and "- 2" are to trim the double-quotes of
    // the string literal lexeme.
    ObjString* obj_str = copy_and_create_str_obj(
        parser.prev_token.start + 1,
        parser.prev_token.length - 2
    );
    emit_constant(obj_val(obj_str));
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
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ; // Do nothing.
        }

        next_token();
    }
}

// Add an identifier name to the constant pool from its
// lexeme in the source code, then return the constant index.
static uint8_t identifier_constant_index(LoxToken* token) {
    return add_constant_to_pool(
        obj_val(copy_and_create_str_obj(token->start, token->length))
    );
}

// Return two if two identifiers are the same
static bool identifiers_equal(LoxToken* a, LoxToken* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// Add a local variable to the array of local variables
// of the compiler struct (for resolving purpose).
static void add_local_var(LoxToken var_name) {
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
    // Skip if it is a global variable
    if (curr_compiler->scope_depth == 0) return;

    // Check for any declared variable with the same name
    LoxToken* var_name = &parser.prev_token;
    for (int i = curr_compiler->local_var_count - 1; i >= 0; i--) {
        LocalVar* curr_var = &curr_compiler->local_vars[i];

        // Going out of current scope -> Break
        if (curr_var->depth != -1 && curr_var->depth < curr_compiler->scope_depth) {
            break;
        }

        // Found a declared variable with the same name
        if (identifiers_equal(var_name, &curr_var->var_name)) {
            error_prev_token("Already a variable with this name in this scope.");
        }
    }

    // Record the variable to the array of local variables
    // of the compiler struct
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

    // Return the constant index of the var name if
    // this is a global variable.
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
static int resolve_local(Compiler* compiler, LoxToken* var_name) {
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
static int resolve_upvalue(Compiler* compiler, LoxToken* var_name) {
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
static void named_variable(LoxToken name, bool can_assign) {
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

    // Variable access or assignment?
    if (can_assign && match_next_token(TOKEN_EQUAL)) { // Assignment
        // Compile the right-hand-side expression
        parse_expression();

        emit_two_bytes(set_op, (uint8_t)arg);
    }
    else { // Access
        emit_two_bytes(get_op, (uint8_t)arg);
    }
}

// Parse and compile a variable access expression.
static void parse_variable(bool can_assign) {
    named_variable(parser.prev_token, can_assign);
}

// Parse and compile a variable declaration.
static void parse_var_decl() {
    uint8_t arg = parse_var_name("Expect variable name.");

    // Prepare the initialization value (or nil if not available)
    if (match_next_token(TOKEN_EQUAL)) {
        parse_expression();
    }
    else {
        emit_byte(OP_NIL);
    }

    consume_mandatory(TOKEN_SEMICOLON, "Expect ';' after variable declaration;");

    define_variable(arg);
}

// Parse and compile a print statement.
static void parse_print_stmt() {
    // Assume the "print" keyword has been consumed
    parse_expression();

    // ";" at the end
    consume_mandatory(TOKEN_SEMICOLON, "Expect ';' at the end of statement.");
    emit_byte(OP_PRINT);
}

// Parse and compile a block statement.
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

// Emit a jump instruction and 2 placeholder operand bytes,
// then return the chunk offset right after the jump opcode.
static int emit_jump(uint8_t opcode) {
    emit_byte(opcode);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->size - 2;
}

// Patch a jump instruction in the chunk with the correct
// jump distance/offset, which is from the jump opcode to
// the most recently added bytecode.
static void patch_jump(int offset) {
    // -2 to adjust for the 2 bytes of the jump distance itself
    int dist = current_chunk()->size - offset - 2;

    if (dist > UINT16_MAX) {
        error_prev_token("Too much bytecode to jump over.");
    }

    // Convert the jump distance to an unsigned short
    // byte-by-byte and put it in the chunk
    current_chunk()->code_chunk[offset] = (dist >> 8) & 0xff;
    current_chunk()->code_chunk[offset + 1] = dist & 0xff;
}

// Parse and compile an if statement.
static void parse_if_stmt() {
    // The condition
    consume_mandatory(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    parse_expression();
    consume_mandatory(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    // Parse the then branch
    int then_jump_offset = emit_jump(OP_JUMP_IF_FALSE); // to jump through then
    emit_byte(OP_POP); // to pop the condition expr in the then branch
    parse_statement();
    int else_jump_offset = emit_jump(OP_JUMP); // to jump through else

    // Need to patch AFTER emitting the above OP_JUMP
    patch_jump(then_jump_offset);

    // Optionally parse the else branch
    emit_byte(OP_POP); // to pop the condition expr in the else branch
    if (match_next_token(TOKEN_ELSE)) parse_statement();
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

// Parse and compile a while loop.
static void parse_while_stmt() {
    int loop_start = current_chunk()->size;

    // The loop condition
    consume_mandatory(TOKEN_LEFT_PAREN, "Expect'(' after 'while'.");
    parse_expression();
    consume_mandatory(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

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

// Parse and compile a for loop.
static void parse_for_stmt() {
    begin_scope(); // To scope the 3 clauses of 'for'.
    consume_mandatory(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    // Initializer clause
    if (match_next_token(TOKEN_SEMICOLON)) {
        // No initializer
    } // The following 2 cases will automatically consume the ';'.
    else if (match_next_token(TOKEN_VAR)) {
        parse_var_decl();
    }
    else {
        parse_expression_stmt();
    }

    // Loop condition
    int loop_start = current_chunk()->size;
    int exit_jump_offset = -1;
    if (!match_next_token(TOKEN_SEMICOLON)) {
        parse_expression();
        consume_mandatory(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exit_jump_offset = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP); // pop the condition before doing the loop body
    }

    // Increment clause
    if (!match_next_token(TOKEN_RIGHT_PAREN)) {
        // Jump through the increment first
        int body_jump_offset = emit_jump(OP_JUMP);
        int increment_start = current_chunk()->size;

        // Parse the increment clause
        parse_expression();
        emit_byte(OP_POP);
        consume_mandatory(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start); // go to the next iteration
        loop_start = increment_start; // change so that after body, jump to increment
        patch_jump(body_jump_offset); // after evaluating loop condition, skip increment clause
    }

    // Loop body
    parse_statement();
    emit_loop(loop_start); // jump to either next iteration or increment clause

    // Clean up
    if (exit_jump_offset != -1) {
        patch_jump(exit_jump_offset);
        emit_byte(OP_POP); // pop the condition after exitting the loop
    }
    end_scope();
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
        // Can't return another expression from initializer
        if (curr_compiler->func_type == TYPE_INIT) {
            error_prev_token("Can't return a value from an initializer.");
        }

        parse_expression(); // the return value expr
        consume_mandatory(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
}

// Parse and compile a statement-level statement.
// Grammar: statement -> exprStmt | forStmt | ifStmt
// | printStmt | returnStmt | whileStmt | block.
static void parse_statement() {
    if (match_next_token(TOKEN_PRINT)) {
        parse_print_stmt();
    }
    else if (match_next_token(TOKEN_IF)) {
        parse_if_stmt();
    }
    else if (match_next_token(TOKEN_WHILE)) {
        parse_while_stmt();
    }
    else if (match_next_token(TOKEN_FOR)) {
        parse_for_stmt();
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

// Compile a function into a bytecode chunk and store
// the resulting ObjFunction in the constant pool.
// (Because function definitions are literals.)
static void compile_function(FunctionType type) {
    // Start a new compiler struct for the function being compiled
    Compiler func_compiler;
    init_compiler(&func_compiler, type);
    begin_scope();

    // Compile the parameters
    consume_mandatory(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check_next_token(TOKEN_RIGHT_PAREN)) {
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
    consume_mandatory(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    // Compile the function body (as a block stmt)
    consume_mandatory(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    parse_block();

    // Store the resulting ObjFunction in the constant pool of the surrounding function
    ObjFunction* result_func = end_compiler();
    emit_two_bytes(OP_CLOSURE, add_constant_to_pool(obj_val(result_func)));

    // Emit 2 bytes [is_local][index] for each upvalue used
    for (int i = 0; i < result_func->upvalue_count; i++) {
        emit_byte(func_compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(func_compiler.upvalues[i].index);
    }
}

// Parse and compile a function declaration
static void parse_func_decl() {
    // Store in a global or local variable
    uint8_t global = parse_var_name("Expect function name.");
    mark_initialized();
    compile_function(TYPE_FUNCTION);
    define_variable(global);
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
            if (arg_count == 255) {
                error_prev_token("Can't have more than 255 arguments.");
            }
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

// Parse and compile a "this" keyword
static void parse_this(bool can_assign) {
    // Check for invalid uses
    if (curr_class_compiler == NULL) {
        error_prev_token("Can't use 'this' outside of a class.");
        return;
    }

    // "this" will be compiled as a local variable. It is already
    // added to the list of locals in init_compiler().
    // can_assign is false because users can't assign to "this".
    parse_variable(false);
}

// Parse and compile a "super" keyword
static void parse_super(bool can_assign) {
    // Check for invalid uses
    if (curr_class_compiler == NULL) {
        error_prev_token("Can't use 'super' outside of a class.");
    }
    else if (!curr_class_compiler->has_superclass) {
        error_prev_token("Can't use 'super' in a class with no superclass.");
    }

    // Super is always used for method access
    consume_mandatory(TOKEN_DOT, "Expect '.' after 'super'.");
    consume_mandatory(TOKEN_IDENTIFIER, "Expect superclass method name.");

    // Parse the method name
    uint8_t name_const_idx = identifier_constant_index(&parser.prev_token);

    // Load "this" and "super" to the stack for OP_GET_SUPER
    // (which at runtime will create a new bound method that binds
    // the superclass method with the receiver).
    named_variable(synthetic_token("this"), false);     // Will be a local var
    LoxToken syn_super = synthetic_token("super");

    // Faster invocation optimization (optional)
    if (match_next_token(TOKEN_LEFT_PAREN)) {
        uint8_t arg_count = parse_arg_list();
        named_variable(syn_super, false);               // Will be an upvalue
        emit_two_bytes(OP_SUPER_INVOKE, name_const_idx);
        emit_byte(arg_count);
    }
    else {
        named_variable(syn_super, false);               // Will be an upvalue
        emit_two_bytes(OP_GET_SUPER, name_const_idx);
    }
}

// Parse and compile a method, then emit bytecode (OP_METHOD)
// to attach it to the owner class.
static void parse_method() {
    // Parse the method's name
    consume_mandatory(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t name_const_idx = identifier_constant_index(&parser.prev_token);

    // Set the correct type based on method name
    // (also so that "this" is set to stack slot 0)
    FunctionType type = TYPE_METHOD;
    if (parser.prev_token.length == 4 &&
        memcmp(parser.prev_token.start, "init", 4) == 0) {
            type = TYPE_INIT;
        }

    // Parse the method's parameter list and body using compile_function()
    compile_function(type);

    emit_two_bytes(OP_METHOD, name_const_idx);
}

// Parse and compile a class declaration
static void parse_class_decl() {
    // Parse the class name (as an identifier constant)
    consume_mandatory(TOKEN_IDENTIFIER, "Expect class name.");
    LoxToken class_name = parser.prev_token;
    uint8_t name_constant = identifier_constant_index(&class_name);
    declare_variable(); // for local class decl

    // Bytecode to create the class at runtime and assign to
    // a variable (by "defining" the variable).
    emit_two_bytes(OP_CLASS, name_constant);
    define_variable(name_constant);

    // Add a new class compiler to the linked stack
    ClassCompiler new_class_compiler;
    new_class_compiler.has_superclass = false;
    new_class_compiler.enclosing = curr_class_compiler;
    curr_class_compiler = &new_class_compiler;

    // Optionally parse a superclass
    if (match_next_token(TOKEN_LESS)) {
        consume_mandatory(TOKEN_IDENTIFIER, "Expect superclass name.");

        // Emit bytecode to push the superclass to the stack
        parse_variable(false);

        // Check for "class A < A {}"
        if (identifiers_equal(&class_name, &parser.prev_token)) {
            error_prev_token("A class can't subclass itself.");
        }

        //  Begin a new scope for the hidden local var "super"
        begin_scope();
        add_local_var(synthetic_token("super"));
        define_variable(0); // 0 is not used because "super" is not a global var

        // Emit bytecode to push the current class to the stack
        named_variable(class_name, false);

        emit_byte(OP_INHERIT);
        new_class_compiler.has_superclass = true;
    }

    // Reload the class back to the stack so that we can inject
    // methods into it. (If present, OP_INHERIT still pops both
    // the superclass and the subclass, so we still need to push
    // again).
    named_variable(class_name, false);

    // Parse and compile the class decl body
    consume_mandatory(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check_next_token(TOKEN_RIGHT_BRACE) && !check_next_token(TOKEN_EOF)) {
        parse_method();
    }
    consume_mandatory(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emit_byte(OP_POP); // Pop the class after adding methods

    // Exit the scope that contains "super"
    if (new_class_compiler.has_superclass) {
        end_scope();
    }

    // Restore the enclosing class compiler
    curr_class_compiler = curr_class_compiler->enclosing;
}

// Parse and compile a declaration-level statement.
// Grammar: declaration -> classDecl | funDecl | var Decl | statement.
static void parse_declaration() {
    if (match_next_token(TOKEN_VAR)) {
        parse_var_decl();
    }
    else if (match_next_token(TOKEN_FUN)) {
        parse_func_decl();
    }
    else if (match_next_token(TOKEN_CLASS)) {
        parse_class_decl();
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
    init_compiler(&compiler, TYPE_TOP_LEVEL); // TODO one more line to remove
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

void mark_compiler_roots() {
    Compiler* compiler = curr_compiler;

    // Mark the ObjFunction of all linked compiler structs
    while (compiler != NULL) {
        mark_object((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}