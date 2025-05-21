#ifndef SIMBOLO_SCANNER_H
#define SIMBOLO_SCANNER_H

typedef enum {
    TOKEN_VAR, // '$'
    TOKEN_LOOP, // '@'
    TOKEN_BACK_SLASH, // '\ '
    TOKEN_QUESTION, // '?'
    TOKEN_COLON, // ':'
    TOKEN_SEMICOLON, // ';'
    TOKEN_EQUAL, // '='
    TOKEN_EQUAL_EQUAL, // '=='
    TOKEN_NOT_EQUAL, // '!='
    TOKEN_2_GREATER, // '>>'
    TOKEN_3_GREATER, // '>>>'
    TOKEN_RETURN, // '<~'
    TOKEN_LEFT_BRACE, // '{'
    TOKEN_RIGHT_BRACE, // '}'
    TOKEN_LEFT_PAREN, // '('
    TOKEN_RIGHT_PAREN, // ')'
    TOKEN_LEFT_SQUARE, // '['
    TOKEN_RIGHT_SQUARE, // ']'
    TOKEN_DOT, // '.'
    TOKEN_COMMA, // ','
    TOKEN_OR, // '|'
    TOKEN_AND, // '&'
    TOKEN_XOR, // '^'
    TOKEN_BANG, // '!'
    TOKEN_GREATER, // '>'
    TOKEN_LESS, // '<'
    TOKEN_GREATER_EQUAL, // '>='
    TOKEN_LESS_EQUAL, // '<='
    TOKEN_PLUS, // '+'
    TOKEN_MINUS, // '-'
    TOKEN_SLASH, // '/'
    TOKEN_STAR, // '*'
    TOKEN_PERCENT, // '%'
    TOKEN_TRUE, // ':)'
    TOKEN_FALSE, // ':('
    TOKEN_NULL, // '#'
    TOKEN_UP_TRIANGLE, // '/\ '
    TOKEN_DOWN_TRIANGLE, // '\/'
    TOKEN_ARROW, // '->'
    TOKEN_READ, // '<<'
    TOKEN_READ_BOOL, // '<?'
    TOKEN_READ_NUM, // '<#'
    TOKEN_TABLE, // '[#]'

    TOKEN_ERROR, TOKEN_EOF,
} TokenType;

// Struct to represent a token in Lox source code
typedef struct {
    TokenType type;
    const char* start; // Start of the lexeme
    int length; // Length of the lexeme
    int line_num;
} Token;

// Initialise the source code Scanner struct
void init_scanner(const char* source_code);

// Scan the source code and return the next token
Token next_token();

#endif // !SIMBOLO_SCANNER_H
