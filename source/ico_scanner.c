#include <stdio.h>
#include <string.h>

#include "ico_common.h"
#include "ico_scanner.h"

typedef struct {
    const char* start; // Pointer to start of the current lexeme
    const char* current; // Pointer to current character being looked at
    int line_num; // Line number
} Scanner;

// Global "singleton" variable, similar to the VM
Scanner sc;

//------------------------------
//      STATIC FUNCTIONS
//------------------------------

// Return true if c is a digit
static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// Return true if c is a letter or the underscore character
static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

// Return true if at end of file (EOF)
static bool is_at_end() {
    // Check the char pointed to by sc.current
    return *sc.current == '\0';
}

// Create a Token struct of the passed type from the current lexeme in the Scanner
static Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = sc.start;
    token.length = (int)(sc.current - sc.start);
    token.line_num = sc.line_num;
    return token;
}

// Advance 1 character and make a token like make_token()
static Token advance_and_make_token(TokenType type) {
    // Advance
    sc.current++;

    // Make token
    Token token;
    token.type = type;
    token.start = sc.start;
    token.length = (int)(sc.current - sc.start);
    token.line_num = sc.line_num;
    return token;
}

// Create and return an error token from the passed message
static Token error_token(const char* error_msg) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = error_msg;
    token.length = (int)strlen(error_msg);
    token.line_num = sc.line_num;
    return token;
}

// Return the next char in the source code and advance the scanner
static char advance_to_next_char() {
    sc.current++;
    return *(sc.current - 1);
}

// Consume the next char and return true if it matches the target
static bool match_next(char target) {
    // End of file
    if (is_at_end()) {
        return false;
    }

    // Doesn't match the expected target
    if (*sc.current != target) {
        return false;
    }

    // Otherwise: does match
    sc.current++;
    return true;
}

// Return the next char without advancing the scanner
static char peek_next_char() {
    return *sc.current;
}

// Return the NEXT NEXT char without advancing the scanner
static char peek_next_next() {
    if (is_at_end()) {
        return '\0';
    }
    return *(sc.current + 1);
}

// Skip the scanner through the whitespaces and comments at the current position
static void skip_whitespace_comment() {
    for (;;) {
        char c = peek_next_char();

        switch (c) {
            // Skip spaces, tabs, and carriage returns
            case ' ': case '\r': case '\t':
                advance_to_next_char();
                break;

            // New line -> increment line number
            case '\n':
                sc.line_num++;
                advance_to_next_char();
                break;

            // Skip through comments too
            case '/':
                if (peek_next_next() == '/') {
                    // One-line comment only.
                    // Also: check for '\n' but don't consume it
                    // -> To let the above case handle it (including
                    // line number increment)
                    while (peek_next_char() != '\n' && !is_at_end()) {
                        advance_to_next_char();
                    }
                }
                else {
                    // Not a comment, but just a '/' character
                    return;
                }
                break;

            default:
                return;
        }
    }
}

// Parse a string literal token in the source code, with the left " already consumed
static Token parse_string() {
    // Consume up to the right "
    while (peek_next_char() != '"' && !is_at_end()) {
        // Allow multiline strings
        if (peek_next_char() == '\n') {
            sc.line_num++;
        }
        advance_to_next_char();
    }

    // End of file before the closing "
    if (is_at_end()) {
        return error_token("Unterminated string.");
    }

    // Consume the closing "
    advance_to_next_char();

    return make_token(TOKEN_STRING);
}

// Parse an int or a float literal token in the source code
static Token parse_number() {
    bool is_float = false;

    // Consume all next digits
    while (is_digit(peek_next_char())) {
        advance_to_next_char();
    }

    // Check for the optional decimal part --> Float
    if (peek_next_char() == '.' && is_digit(peek_next_next())) {
        is_float = true;

        // Consume the '.'
        advance_to_next_char();

        // Consume the digits after the decimal point
        while (is_digit(peek_next_char())) {
            advance_to_next_char();
        }
    }

    return make_token(is_float ? TOKEN_FLOAT : TOKEN_INT);
}

// Parse a keyword or an identifier
static Token parse_identifier() {
    // Consume alphanumeric chars as far as possible
    while (is_alpha(peek_next_char()) || is_digit(peek_next_char())) {
        advance_to_next_char();
    }
    return make_token(TOKEN_IDENTIFIER);
}

//------------------------------
//       HEADER FUNCTIONS
//------------------------------

void init_scanner(const char *source_code) {
    sc.start = source_code;
    sc.current = source_code;
    sc.line_num = 1;
}

Token scan_next_token() {
    // Skip all whitespaces before the next token
    skip_whitespace_comment();

    // Move to the next lexeme
    sc.start = sc.current;

    // Check for end of file
    if (is_at_end()) {
        return make_token(TOKEN_EOF);
    }

    // Get the next char and advance sc.current
    char c = advance_to_next_char();

    // Check for keywords and identifiers first
    if (is_alpha(c)) {
        return parse_identifier();
    }

    // Check for number literals
    if (is_digit(c)) {
        return parse_number();
    }

    switch (c) {
        // Single-character tokens
        case '$': return make_token(TOKEN_VAR);
        case '@': return make_token(TOKEN_LOOP);
        case '?': return make_token(TOKEN_QUESTION);
        case ';': return make_token(TOKEN_SEMICOLON);
        case '{': return make_token(TOKEN_LEFT_BRACE);
        case '}': return make_token(TOKEN_RIGHT_BRACE);
        case '(': return make_token(TOKEN_LEFT_PAREN);
        case ')': return make_token(TOKEN_RIGHT_PAREN);
        case ']': return make_token(TOKEN_RIGHT_SQUARE);
        case '.': return make_token(TOKEN_DOT);
        case ',': return make_token(TOKEN_COMMA);
        case '|': return make_token(TOKEN_PIPE);
        case '&': return make_token(TOKEN_AND);
        case '^': return make_token(TOKEN_CARET);
        case '+': return make_token(TOKEN_PLUS);
        case '*': return make_token(TOKEN_STAR);
        case '%': return make_token(TOKEN_PERCENT);
        case '#': return make_token(TOKEN_NULL);

        // Two-character tokens
        case '!': return make_token(match_next('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return make_token(match_next('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '/': return make_token(match_next('\\') ? TOKEN_UP_TRIANGLE : TOKEN_SLASH);
        case '\\': return make_token(match_next('/') ? TOKEN_DOWN_TRIANGLE : TOKEN_BACK_SLASH);
        case '-': return make_token(match_next('>') ? TOKEN_ARROW : TOKEN_MINUS);

        case ':':
            switch (peek_next_char()) {
                case ')': return advance_and_make_token(TOKEN_TRUE);
                case '(': return advance_and_make_token(TOKEN_FALSE);
                default:  return make_token(TOKEN_COLON);
            }

        case '<':
            switch (peek_next_char()) {
                case '=': return advance_and_make_token(TOKEN_LESS_EQUAL);
                case '~': return advance_and_make_token(TOKEN_RETURN);
                case '<': return advance_and_make_token(TOKEN_READ);
                case '?': return advance_and_make_token(TOKEN_READ_BOOL);
                case '#': return advance_and_make_token(TOKEN_READ_NUM);
                default:  return make_token(TOKEN_LESS);
            }

        // Three-character tokens
        case '>':
            switch (peek_next_char()) {
                case '=': return advance_and_make_token(TOKEN_GREATER_EQUAL);
                case '>':
                    advance_to_next_char();
                    return make_token(match_next('>') ? TOKEN_3_GREATER : TOKEN_2_GREATER);
                default: return make_token(TOKEN_GREATER);
            }

        case '[': {
            char c1 = peek_next_char();
            char c2 = peek_next_next();
            if (c1 == '#' && c2 == ']') {
                advance_to_next_char();
                advance_to_next_char();
                return make_token(TOKEN_TABLE);
            }
            else {
                return make_token(TOKEN_LEFT_SQUARE);
            }
        }

        // String literals
        case '"': return parse_string();
    }

    // Invalid token found
    return error_token("Unexpected character.");
}

#ifdef DEBUG_PRINT_TOKEN

const char* token_names[] = {
    [TOKEN_VAR] = "TOKEN_VAR",
    [TOKEN_LOOP] = "TOKEN_LOOP",
    [TOKEN_QUESTION] = "TOKEN_QUESTION",
    [TOKEN_SEMICOLON] = "TOKEN_SEMICOLON",
    [TOKEN_LEFT_BRACE] = "TOKEN_LEFT_BRACE",
    [TOKEN_RIGHT_BRACE] = "TOKEN_RIGHT_BRACE",
    [TOKEN_LEFT_PAREN] = "TOKEN_LEFT_PAREN",
    [TOKEN_RIGHT_PAREN] = "TOKEN_RIGHT_PAREN",
    [TOKEN_RIGHT_SQUARE] = "TOKEN_RIGHT_SQUARE",
    [TOKEN_DOT] = "TOKEN_DOT",
    [TOKEN_COMMA] = "TOKEN_COMMA",
    [TOKEN_OR] = "TOKEN_OR",
    [TOKEN_AND] = "TOKEN_AND",
    [TOKEN_XOR] = "TOKEN_XOR",
    [TOKEN_PLUS] = "TOKEN_PLUS",
    [TOKEN_STAR] = "TOKEN_STAR",
    [TOKEN_PERCENT] = "TOKEN_PERCENT",
    [TOKEN_NULL] = "TOKEN_NULL",
    [TOKEN_EQUAL] = "TOKEN_EQUAL",
    [TOKEN_EQUAL_EQUAL] = "TOKEN_EQUAL_EQUAL",
    [TOKEN_BANG] = "TOKEN_BANG",
    [TOKEN_BANG_EQUAL] = "TOKEN_NOT_EQUAL",
    [TOKEN_COLON] = "TOKEN_COLON",
    [TOKEN_TRUE] = "TOKEN_TRUE",
    [TOKEN_FALSE] = "TOKEN_FALSE",
    [TOKEN_LESS] = "TOKEN_LESS",
    [TOKEN_LESS_EQUAL] = "TOKEN_LESS_EQUAL",
    [TOKEN_RETURN] = "TOKEN_RETURN",
    [TOKEN_READ] = "TOKEN_READ",
    [TOKEN_READ_BOOL] = "TOKEN_READ_BOOL",
    [TOKEN_READ_NUM] = "TOKEN_READ_NUM",
    [TOKEN_SLASH] = "TOKEN_SLASH",
    [TOKEN_UP_TRIANGLE] = "TOKEN_UP_TRIANGLE",
    [TOKEN_BACK_SLASH] = "TOKEN_BACK_SLASH",
    [TOKEN_DOWN_TRIANGLE] = "TOKEN_DOWN_TRIANGLE",
    [TOKEN_MINUS] = "TOKEN_MINUS",
    [TOKEN_ARROW] = "TOKEN_ARROW",
    [TOKEN_GREATER] = "TOKEN_GREATER",
    [TOKEN_GREATER_EQUAL] = "TOKEN_GREATER_EQUAL",
    [TOKEN_2_GREATER] = "TOKEN_2_GREATER",
    [TOKEN_3_GREATER] = "TOKEN_3_GREATER",
    [TOKEN_LEFT_SQUARE] = "TOKEN_LEFT_SQUARE",
    [TOKEN_TABLE] = "TOKEN_TABLE",
    [TOKEN_IDENTIFIER] = "TOKEN_IDENTIFIER",
    [TOKEN_INT] = "TOKEN_INT",
    [TOKEN_FLOAT] = "TOKEN_FLOAT",
    [TOKEN_STRING] = "TOKEN_STRING",
    [TOKEN_ERROR] = "TOKEN_ERROR",
    [TOKEN_EOF] = "TOKEN_EOF",
};

void print_token(Token token) {
    fputc('"', stderr);
    for (int i = 0; i < token.length; i++) {
        fputc(*(token.start + i), stderr);
    }
    fprintf(stderr, "\" %s\n", token_names[token.type]);
}

#endif