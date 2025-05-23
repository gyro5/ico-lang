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

// Parse a number literal token in the source code
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

Token next_token() {
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
        return parse_number(); // TODO: fix this
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
        case '|': return make_token(TOKEN_OR);
        case '&': return make_token(TOKEN_AND);
        case '^': return make_token(TOKEN_XOR);
        case '+': return make_token(TOKEN_PLUS);
        case '*': return make_token(TOKEN_STAR);
        case '%': return make_token(TOKEN_PERCENT);
        case '#': return make_token(TOKEN_NULL);

        // Two-character tokens
        case '!':
            return make_token(match_next('=') ? TOKEN_NOT_EQUAL : TOKEN_BANG);
        case '=':
            return make_token(match_next('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case ':':
            switch (peek_next_char()) {
                case ')': advance_and_make_token(TOKEN_TRUE); break;
                case '(': advance_and_make_token(TOKEN_FALSE); break;
                default: make_token(TOKEN_COLON); break;
            }
            break;
        case '<':
            switch (peek_next_char()) {
                case '=': advance_and_make_token(TOKEN_LESS_EQUAL); break;
                case '~': advance_and_make_token(TOKEN_RETURN); break;
                case '<': advance_and_make_token(TOKEN_READ); break;
                case '?': advance_and_make_token(TOKEN_READ_BOOL); break;
                case '#': advance_and_make_token(TOKEN_READ_NUM); break;
                default: make_token(TOKEN_LESS); break;
            }
            break;
        case '/':
            return make_token(match_next('\\') ? TOKEN_UP_TRIANGLE : TOKEN_SLASH);
        case '\\':
            return make_token(match_next('/') ? TOKEN_DOWN_TRIANGLE : TOKEN_BACK_SLASH);
        case '-':
            return make_token(match_next('>') ? TOKEN_ARROW : TOKEN_MINUS);

        // Three-character tokens
        case '>':
            switch (peek_next_char()) {
                case '=': advance_and_make_token(TOKEN_GREATER_EQUAL); break;
                case '>':
                    advance_to_next_char();
                    make_token(match_next('>') ? TOKEN_3_GREATER : TOKEN_2_GREATER);
                    break;
                default: make_token(TOKEN_GREATER); break;
            }
            break;
        case '[': {
            char c1 = peek_next_char();
            char c2 = peek_next_next();
            if (c1 == '#' && c2 == ']') {
                advance_to_next_char();
                advance_to_next_char();
                make_token(TOKEN_TABLE);
            }
            else {
                make_token(TOKEN_LEFT_SQUARE);
            }
            break;
        }

        // String literals
        case '"': return parse_string();
    }

    // Invalid token found
    return error_token("Unexpected character.");
}