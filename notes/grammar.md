## Syntax Grammar

### Declarations & Statements

```r
# Program
program -> decl* EOF;

# Declaration
decl -> var_decl | stmt;

var_decl -> "$" IDENTIFIER ("=" expr)? ;

# Statements
stmt -> exprStmt | loop | if | print | return | block ;

exprStmt -> expr ";" ; # Will print in REPL with sth like OP_PRINT

loop -> "@" expr ":" stmt ;

if -> "\ " expr "?" stmt (":" stmt)? ; # to mirror the ternary expr

print -> (">>" | ">>>") expr ";" ;

return -> "<~" expr? ";" ;

block -> "{" decl* "}" ;
```

### Expressions

```r
expr -> assignment;

assignment -> (call ("." | "[" expr "]"))? IDENTIFIER "=" assignment | ternary ;

ternary -> logical_or "?" logical_or ":" logical_or | logical_or ;

logical_or -> logical_and ("|" logical_and)* ;

logical_and -> equality ( "&" equality )* ;

equality -> comparison (("!=" | "==") comparison)* ;

comparison -> term ( ( ">" | ">=" | "<" | "<=" ) term )* ;

term -> factor ( ( "-" | "+" ) factor )* ;

factor -> unary ( ( "/" | "*" | "%" ) unary )* ;

unary -> ( "!" | "-" ) unary | power ;

power -> call ("^" unary)? ; # Because unary can itself be power

call -> primary ( "(" arguments? ")" | "." IDENTIFIER | "[" expr "]")* ;

primary -> bool | null | INT | FLOAT | STRING | IDENTIFIER | function | "(" expr ")" | read | "\/" | list | table ;

bool -> ":)" | ":(" ;

null -> "#" ;

function -> "/\ " IDENTIFIER* "->" (expr | block);

read -> "<<" | "<?" | "<#" ;

list -> "[" (expr ("," expr)* )? "]" ;

table -> "[#]" ;
```

## Lexical Grammar

```r
INT -> DIGIT+ ;
FLOAT -> INT"."INT ;
STRING -> "\"" <any char except '"'>* "\"" ;
IDENTIFIER -> ALPHA (ALPHA | DIGIT)* ;
ALPHA -> [a..zA..Z_]
DIGIT -> [0..9]
```

## Token types

```js
TOKEN_VAR : '$'
TOKEN_LOOP : '@'
TOKEN_BACK_SLASH : '\ '
TOKEN_QUESTION : '?'
TOKEN_COLON : ':'
TOKEN_SEMICOLON : ';'
TOKEN_EQUAL : '='
TOKEN_EQUAL_EQUAL : '=='
TOKEN_NOT_EQUAL : '!='
TOKEN_2_GREATER : '>>'
TOKEN_3_GREATER : '>>>'
TOKEN_RETURN : '<~'
TOKEN_LEFT_BRACE : '{'
TOKEN_RIGHT_BRACE : '}'
TOKEN_LEFT_PAREN : '('
TOKEN_RIGHT_PAREN : ')'
TOKEN_LEFT_SQUARE : '['
TOKEN_RIGHT_SQUARE : ']'
TOKEN_DOT : '.'
TOKEN_COMMA : ','
TOKEN_OR : '|'
TOKEN_AND : '&'
TOKEN_XOR : '^'
TOKEN_BANG : '!'
TOKEN_GREATER : '>'
TOKEN_LESS : '<'
TOKEN_GREATER_EQUAL : '>='
TOKEN_LESS_EQUAL : '<='
TOKEN_PLUS : '+'
TOKEN_MINUS : '-'
TOKEN_SLASH : '/'
TOKEN_STAR : '*'
TOKEN_PERCENT : '%'
TOKEN_TRUE : ':)'
TOKEN_FALSE : ':('
TOKEN_NULL : '#'
TOKEN_UP_TRIANGLE : '/\ '
TOKEN_DOWN_TRIANGLE : '\/'
TOKEN_ARROW : '->'
TOKEN_READ : '<<'
TOKEN_READ_BOOL : '<?'
TOKEN_READ_NUM : '<#'
TOKEN_TABLE : '[#]'
```

## Notes:

- Var declarations are evaluated to null.

- The REPL will add an implicit `;` to every input line.

- Comments are "// ..." (until end of line).

- IO expr:
  + Print: `>> expr` (evaluated to the printed value)
  + Println: `>>> expr`
  + Read string: `<<` (treat as a value)
  + Read bool: `<?` (accept ":)" or ":(")
  + Read number: `<#`

- Function literal will require implementation of anonymous functions as well.

- There are 2 types of table copy: shallow and deep. --> Implemented as native function.