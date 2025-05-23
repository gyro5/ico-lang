#ifndef ICO_SCANNER_H
#define ICO_SCANNER_H

typedef enum {
    // Single-character tokens
    TOKEN_VAR, // "$"
    TOKEN_LOOP, // "@"
    TOKEN_QUESTION, // "?"
    TOKEN_SEMICOLON, // ";"
    TOKEN_LEFT_BRACE, // "{"
    TOKEN_RIGHT_BRACE, // "}"
    TOKEN_LEFT_PAREN, // "("
    TOKEN_RIGHT_PAREN, // ")"
    TOKEN_RIGHT_SQUARE, // "]"
    TOKEN_DOT, // "."
    TOKEN_COMMA, // ","
    TOKEN_OR, // "|"
    TOKEN_AND, // "&"
    TOKEN_XOR, // "^"
    TOKEN_PLUS, // "+"
    TOKEN_STAR, // "*"
    TOKEN_PERCENT, // "%"
    TOKEN_NULL, // "#"

    // 2-character tokens
    TOKEN_EQUAL, // "="
    TOKEN_EQUAL_EQUAL, // "=="

    TOKEN_BANG, // "!"
    TOKEN_NOT_EQUAL, // "!="

    TOKEN_COLON, // ":"
    TOKEN_TRUE, // ":)"
    TOKEN_FALSE, // ":("

    TOKEN_LESS, // "<"
    TOKEN_LESS_EQUAL, // "<="
    TOKEN_RETURN, // "<~"
    TOKEN_READ, // "<<"
    TOKEN_READ_BOOL, // "<?"
    TOKEN_READ_NUM, // "<#"

    TOKEN_SLASH, // "/"
    TOKEN_UP_TRIANGLE, // "/\"

    TOKEN_BACK_SLASH, // "\"
    TOKEN_DOWN_TRIANGLE, // "\/"

    TOKEN_MINUS, // "-"
    TOKEN_ARROW, // "->"

    // 3-character tokens
    TOKEN_GREATER, // ">"
    TOKEN_GREATER_EQUAL, // ">="
    TOKEN_2_GREATER, // ">>"
    TOKEN_3_GREATER, // ">>>"

    TOKEN_LEFT_SQUARE, // "["
    TOKEN_TABLE, // "[#]"

    // Literals and identifiers
    TOKEN_IDENTIFIER,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,

    TOKEN_ERROR, TOKEN_EOF,
} TokenType;

// Struct to represent a token in source code
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
