#include <stdio.h>
#include <string.h>

#include "clox_common.h"
#include "clox_scanner.h"

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

// Create a LoxToken struct of the passed type
// from the current lexeme in the Scanner
static LoxToken make_token(TokenType type) {
    LoxToken token;
    token.type = type;
    token.start = sc.start;
    token.length = (int)(sc.current - sc.start);
    token.line_num = sc.line_num;
    return token;
}

// Create and return an error token from the passed message
static LoxToken error_token(const char* error_msg) {
    LoxToken token;
    token.type = TOKEN_ERROR;
    token.start = error_msg;
    token.length = (int)strlen(error_msg);
    token.line_num = sc.line_num;
    return token;
}

// Return the next char in the source code
static char next_char() {
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

// Skip the scanner through the whitespaces and
// comments at the current position
static void skip_whitespace_comment() {
    for (;;) {
        char c = peek_next_char();

        switch (c) {
            // Skip spaces, tabs, and carriage returns
            case ' ': case '\r': case '\t':
                next_char();
                break;

            // New line -> increment line number
            case '\n':
                sc.line_num++;
                next_char();
                break;

            // Skip through comments too
            case '/':
                if (peek_next_next() == '/') {
                    // One-line comment only.
                    // Also: check for '\n' but don't consume it
                    // -> To let the above case handle it (including
                    // line number increment)
                    while (peek_next_char() != '\n' && !is_at_end()) {
                        next_char();
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

// Parse a string literal token in the source code,
// with the left " already consumed
static LoxToken parse_string() {
    // Consume up to the right "
    while (peek_next_char() != '"' && !is_at_end()) {
        // Allow multiline strings
        if (peek_next_char() == '\n') {
            sc.line_num++;
        }
        next_char();
    }

    // End of file before the closing "
    if (is_at_end()) {
        return error_token("Unterminated string.");
    }

    // Consume the closing "
    next_char();

    return make_token(TOKEN_STRING);
}

// Parse a number literal token in the source code
static LoxToken parse_number() {
    // Consume all next digits
    while (is_digit(peek_next_char())) {
        next_char();
    }

    // Check for the optional decimal part
    if (peek_next_char() == '.' && is_digit(peek_next_next())) {
        // Consume the '.'
        next_char();

        // Consume the digits after the decimal point
        while (is_digit(peek_next_char())) {
            next_char();
        }
    }

    return make_token(TOKEN_NUMBER);
}

// Check if the rest of the current lexeme match the keyword token type
static TokenType check_keyword(int rest_start, int rest_length,
    const char* rest, TokenType type) {
    /*
    Parameters:
    - rest_start: start index in the keyword string of the remaining part (rest)
    - rest_length: length of rest
    - rest: the remaining part to-be-checked of the keyword
    - type: the target token type to verify against
    */

    int lexeme_length = sc.current - sc.start;
    int keyword_length = rest_start + rest_length;

    if (lexeme_length == keyword_length 
    && memcmp(sc.start + rest_start, rest, rest_length) == 0) {
    return type;
    }

    // The rest of the current lexeme does NOT match
    return TOKEN_IDENTIFIER;
}

// Check and return the approriate keyword or identifier token type
static TokenType keyword_identifier_type() {
    switch (*(sc.start)) {
        // Cases with only one possible keyword
        case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);

        // Cases with multiple possible keywords
        case 'f':
            // If lexeme contains 2 or more chars
            if (sc.current - sc.start > 1) {
                switch (*(sc.start + 1)) {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        
        case 't':
            // If lexeme contains 2 or more chars
            if (sc.current - sc.start > 1) {
                switch (*(sc.start + 1)) {
                    case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
    }
    return TOKEN_IDENTIFIER;
}

// Parse a keyword or an identifier
static LoxToken parse_keyword_identifier() {
    // Consume alphanumeric chars as far as possible
    while (is_alpha(peek_next_char()) || is_digit(peek_next_char())) {
        next_char();
    }
    return make_token(keyword_identifier_type());
}

//------------------------------
//       HEADER FUNCTIONS
//------------------------------

void init_scanner(const char *source_code) {
    sc.start = source_code;
    sc.current = source_code;
    sc.line_num = 1;
}

LoxToken scan_token() {
    // Skip all whitespaces before the next token
    skip_whitespace_comment();

    // Move to the next lexeme
    sc.start = sc.current;

    // Check for end of file
    if (is_at_end()) {
        return make_token(TOKEN_EOF);
    }

    // Get the next char
    char c = next_char();

    // Check for keywords and identifiers first
    if (is_alpha(c)) {
        return parse_keyword_identifier();
    }

    // Check for number literals
    if (is_digit(c)) {
        return parse_number();
    }

    switch (c) {
        // Single-character tokens
        case '(': return make_token(TOKEN_LEFT_PAREN);
        case ')': return make_token(TOKEN_RIGHT_PAREN);
        case '{': return make_token(TOKEN_LEFT_BRACE);
        case '}': return make_token(TOKEN_RIGHT_BRACE);
        case ';': return make_token(TOKEN_SEMICOLON);
        case ',': return make_token(TOKEN_COMMA);
        case '.': return make_token(TOKEN_DOT);
        case '-': return make_token(TOKEN_MINUS);
        case '+': return make_token(TOKEN_PLUS);
        case '/': return make_token(TOKEN_SLASH);
        case '*': return make_token(TOKEN_STAR);

        // Two-character tokens
        case '!':
            return make_token(match_next('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return make_token(match_next('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return make_token(match_next('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return make_token(match_next('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

        // String literals
        case '"': return parse_string();
    }

    // Invalid token found
    return error_token("Unexpected character.");
}