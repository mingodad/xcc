#pragma once

#include <stdbool.h>
#include <stddef.h>  // size_t
#include <stdint.h>  // intptr_t
#include <stdio.h>  // FILE

#define MAX_LEX_LOOKAHEAD  (2)

typedef struct Name Name;

// Line

typedef struct {
  const char *filename;
  int lineno;
  const char *buf;
} Line;

// Token

// Token kind
enum TokenKind {
  TK_EOF,            // Represent input end
  TK_ADD,            // +
  TK_SUB,            // -
  TK_MUL,            // *
  TK_DIV,            // /
  TK_MOD,            // %
  TK_AND,            // &
  TK_OR,             // |
  TK_HAT,            // ^
  TK_LT,             // <
  TK_GT,             // >
  TK_NOT,            // !
  TK_LPAR,           // (
  TK_RPAR,           // )
  TK_LBRACE,         // {
  TK_RBRACE,         // }
  TK_LBRACKET,       // [
  TK_RBRACKET,       // ]
  TK_ASSIGN,         // =
  TK_COLON,          // :
  TK_SEMICOL,        // ;
  TK_COMMA,          // ,
  TK_DOT,            // .
  TK_QUESTION,       // ?
  TK_TILDA,          // ~
  TK_INTLIT,         // int literal
  TK_CHARLIT,        // char literal
  TK_LONGLIT,        // long literal
  TK_LLONGLIT,       // long long literal
  TK_UINTLIT,        // unsigned int literal
  TK_UCHARLIT,       // unsigned char literal
  TK_ULONGLIT,       // unsigned long literal
  TK_ULLONGLIT,      // unsigned long long literal
  TK_STR,            // String literal
  TK_IDENT,          // Identifier
  TK_LSHIFT,         // <<
  TK_RSHIFT,         // >>
  TK_EQ,             // ==
  TK_NE,             // !=
  TK_LE,             // <=
  TK_GE,             // >=
  TK_LOGAND,         // &&
  TK_LOGIOR,         // ||
  TK_ARROW,          // ->
  TK_ADD_ASSIGN,     // +=
  TK_SUB_ASSIGN,     // -=
  TK_MUL_ASSIGN,     // *=
  TK_DIV_ASSIGN,     // /=
  TK_MOD_ASSIGN,     // %=
  TK_AND_ASSIGN,     // &=
  TK_OR_ASSIGN,      // |=
  TK_HAT_ASSIGN,     // ^=
  TK_LSHIFT_ASSIGN,  // <<=
  TK_RSHIFT_ASSIGN,  // >>=
  TK_INC,            // ++
  TK_DEC,            // --
  TK_IF,
  TK_ELSE,
  TK_SWITCH,
  TK_CASE,
  TK_DEFAULT,
  TK_DO,
  TK_WHILE,
  TK_FOR,
  TK_BREAK,
  TK_CONTINUE,
  TK_GOTO,
  TK_RETURN,
  TK_VOID,
  TK_CHAR,
  TK_SHORT,
  TK_INT,
  TK_LONG,
  TK_UNSIGNED,
  TK_SIGNED,
  TK_CONST,
  TK_STATIC,
  TK_INLINE,
  TK_EXTERN,
  TK_VOLATILE,
  TK_STRUCT,
  TK_UNION,
  TK_ENUM,
  TK_SIZEOF,
  TK_ALIGNOF,
  TK_TYPEDEF,
  TK_ELLIPSIS,       // ...
  TK_ASM,

#ifndef __NO_FLONUM
  TK_FLOAT,
  TK_FLOATLIT,   // float literal
  TK_DOUBLE,
  TK_DOUBLELIT,  // double literal
#endif

  // For preprocessor.
  PPTK_CONCAT,       // ##
  PPTK_STRINGIFY,    // #
};

// Token
typedef struct Token {
  enum TokenKind kind;
  Line *line;
  const char *begin;
  const char *end;
  union {
    const Name *ident;
    struct {
      const char *buf;
      size_t size;  // Include last '\0'.
    } str;
    intptr_t fixnum;
#ifndef __NO_FLONUM
    double flonum;
#endif
  };
} Token;

typedef struct {
  FILE *fp;
  const char *filename;
  Line *line;
  const char *p;
  Token *fetched[MAX_LEX_LOOKAHEAD];
  int idx;
  int lineno;
} Lexer;

void init_lexer(void);
void set_source_file(FILE *fp, const char *filename);
void set_source_string(const char *line, const char *filename, int lineno);
Token *fetch_token(void);
Token *match(enum TokenKind kind);
void unget_token(Token *token);
const char *read_ident(const char *p);
Token *alloc_dummy_ident(void);
const char *get_lex_p(void);
void lex_error(const char *p, const char *fmt, ...);

const char *block_comment_start(const char *p);
const char *block_comment_end(const char *p);

typedef bool (*LexEofCallback)(void);
LexEofCallback set_lex_eof_callback(LexEofCallback callback);
bool lex_eof_continue(void);
