#include "pp_parser.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>  // malloc
#include <string.h>

#include "lexer.h"
#include "macro.h"
#include "table.h"
#include "util.h"

static PpResult pp_prim(void);
static PpResult pp_cast_expr(void);

//

static Stream *pp_stream;

Stream *set_pp_stream(Stream *stream) {
  Stream *old = pp_stream;
  pp_stream = stream;
  return old;
}

static void pp_parse_error_valist(const Token *token, const char *fmt, va_list ap) {
  if (fmt != NULL) {
    if (token == NULL)
      token = fetch_token();
    if (token != NULL && token->line != NULL) {
      fprintf(stderr, "%s(%d): ", token->line->filename, token->line->lineno);
    }

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
  }

  if (token != NULL && token->line != NULL && token->begin != NULL)
    show_error_line(token->line->buf, token->begin, token->end - token->begin);
}

void pp_parse_error(const Token *token, const char *fmt, ...) {
  ++compile_error_count;

  va_list ap;
  va_start(ap, fmt);
  pp_parse_error_valist(token, fmt, ap);
  va_end(ap);

  exit(1);
}

Token *pp_match(enum TokenKind kind) {
  const char *p = get_lex_p();
  if (p != NULL) {
    for (;;) {
      const char *q = block_comment_start(p);
      if (q == NULL)
        break;

      const char *comment_start = q;
      for (;;) {
        q = block_comment_end(q);
        if (q != NULL) {
          set_source_string(q, pp_stream->filename, pp_stream->lineno);
          break;
        }

        char *line = NULL;
        size_t capa = 0;
        ssize_t len = getline_cont(&line, &capa, pp_stream->fp, &pp_stream->lineno);
        if (len == -1) {
          lex_error(comment_start, "Block comment not closed");
        }
        q = line;
      }
      p = q;
    }
  }

  return match(kind);
}

Token *pp_consume(enum TokenKind kind, const char *error) {
  Token *tok = pp_match(kind);
  if (tok == NULL)
    pp_parse_error(tok, error);
  return tok;
}

static PpResult expand_ident(const Token *ident) {
  Macro *macro = macro_get(ident->ident);
  if (macro == NULL) {
    //parse_error(ident, "`%.s' not defined", ident->ident->bytes, ident->ident->chars);
    return 0;
  }

  Vector *args = NULL;
  if (macro->params != NULL)
    args = pp_funargs();

  StringBuffer sb;
  sb_init(&sb);
  expand_macro(macro, ident, args, ident->ident, &sb);

  const char *left = get_lex_p();
  if (left != NULL)
    sb_append(&sb, left, NULL);
  char *expanded = sb_to_string(&sb);

  set_source_string(expanded, NULL, -1);

  return pp_prim();
}

static PpResult parse_defined(void) {
  bool lpar = pp_match(TK_LPAR) != NULL;
  Token *ident = pp_consume(TK_IDENT, "Ident expected");
  if (lpar)
    pp_consume(TK_RPAR, "No close paren");

  return macro_get(ident->ident) != NULL;
}

static PpResult pp_prim(void) {
  Token *tok;
  if ((tok = pp_match(TK_LPAR)) != NULL) {
    PpResult result = pp_expr();
    pp_consume(TK_RPAR, "No close paren");
    return result;
  }

  if ((tok = pp_match(TK_CHARLIT)) != NULL ||
      (tok = pp_match(TK_INTLIT)) != NULL ||
      (tok = pp_match(TK_LONGLIT)) != NULL ||
      (tok = pp_match(TK_LLONGLIT)) != NULL ||
      (tok = pp_match(TK_UCHARLIT)) != NULL ||
      (tok = pp_match(TK_UINTLIT)) != NULL ||
      (tok = pp_match(TK_ULONGLIT)) != NULL ||
      (tok = pp_match(TK_ULLONGLIT)) != NULL) {
    return tok->fixnum;
  }
  //if ((tok = pp_match(TK_STR)) != NULL)
  //  return new_expr_str(tok, tok->str.buf, tok->str.size);

  Token *ident = pp_consume(TK_IDENT, "Number or Ident or open paren expected");
  if (equal_name(ident->ident, alloc_name("defined", NULL, false))) {
    return parse_defined();
  } else {
    return expand_ident(ident);
  }
}

static PpResult pp_postfix(void) {
  PpResult result = pp_prim();

  //for (;;) {
    //Token *tok;
    //if (pp_match(TK_LPAR))
    //  expr = parse_funcall(expr);
    //else if ((tok = pp_match(TK_LBRACKET)) != NULL)
    //  expr = parse_array_index(tok, expr);
    //else if ((tok = pp_match(TK_INC)) != NULL)
    //  expr = new_expr_unary(EX_POSTINC, NULL, tok, expr);
    //else if ((tok = pp_match(TK_DEC)) != NULL)
    //  expr = new_expr_unary(EX_POSTDEC, NULL, tok, expr);
    //else
      return result;
  //}
}

static PpResult pp_unary(void) {
  Token *tok;
  if ((tok = pp_match(TK_ADD)) != NULL) {
    return pp_cast_expr();
  }

  if ((tok = pp_match(TK_SUB)) != NULL) {
    PpResult result = pp_cast_expr();
    return -result;
  }

  if ((tok = pp_match(TK_NOT)) != NULL) {
    PpResult result = pp_cast_expr();
    return result ? 0 : 1;
  }

  if ((tok = pp_match(TK_TILDA)) != NULL) {
    PpResult result = pp_cast_expr();
    return ~result;
  }

  //if ((tok = pp_match(TK_AND)) != NULL) {
  //  PpExpr *expr = pp_cast_expr();
  //  return new_expr_unary(EX_REF, NULL, tok, expr);
  //}

  //if ((tok = pp_match(TK_MUL)) != NULL) {
  //  PpExpr *expr = pp_cast_expr();
  //  return new_expr_unary(EX_DEREF, NULL, tok, expr);
  //}

  //if ((tok = pp_match(TK_INC)) != NULL) {
  //  PpExpr *expr = pp_unary();
  //  return new_expr_unary(EX_PREINC, NULL, tok, expr);
  //}

  //if ((tok = pp_match(TK_DEC)) != NULL) {
  //  PpExpr *expr = pp_unary();
  //  return new_expr_unary(EX_PREDEC, NULL, tok, expr);
  //}

  return pp_postfix();
}

static PpResult pp_cast_expr(void) {
  return pp_unary();
}

static PpResult pp_mul(void) {
  PpResult result = pp_cast_expr();
  for (;;) {
    Token *tok;
    if (!(((tok = pp_match(TK_MUL)) != NULL) ||
          ((tok = pp_match(TK_DIV)) != NULL) ||
          ((tok = pp_match(TK_MOD)) != NULL)))
      return result;

    PpResult rhs = pp_cast_expr();
    switch (tok->kind) {
    case TK_MUL:  result *= rhs; break;
    case TK_DIV:  result /= rhs; break;
    case TK_MOD:  result %= rhs; break;
    default:  assert(false); break;
    }
  }
}

static PpResult pp_add(void) {
  PpResult result = pp_mul();
  for (;;) {
    Token *tok;
    if (!(((tok = pp_match(TK_ADD)) != NULL) ||
          ((tok = pp_match(TK_SUB)) != NULL)))
      return result;

    PpResult rhs = pp_mul();
    if (tok->kind == TK_ADD)
      result += rhs;
    else
      result -= rhs;
  }
}

static PpResult pp_shift(void) {
  PpResult result = pp_add();
  for (;;) {
    Token *tok;
    if (!(((tok = pp_match(TK_LSHIFT)) != NULL) ||
          ((tok = pp_match(TK_RSHIFT)) != NULL)))
      return result;

    PpResult lhs = result, rhs = pp_add();
    if (tok->kind == TK_LSHIFT)
      result = lhs << rhs;
    else
      result = lhs >> rhs;
  }
}

static PpResult pp_cmp(void) {
  PpResult result = pp_shift();
  for (;;) {
    Token *tok;
    if (!(((tok = pp_match(TK_LT)) != NULL) ||
          ((tok = pp_match(TK_GT)) != NULL) ||
          ((tok = pp_match(TK_LE)) != NULL) ||
          ((tok = pp_match(TK_GE)) != NULL)))
      return result;

    PpResult lhs = result, rhs = pp_shift();
    switch (tok->kind) {
    case TK_LT:  result = lhs <  rhs ? 1 : 0; break;
    case TK_LE:  result = lhs <= rhs ? 1 : 0; break;
    case TK_GE:  result = lhs >= rhs ? 1 : 0; break;
    case TK_GT:  result = lhs >  rhs ? 1 : 0; break;
    default:  assert(false); break;
    }
  }
}

static PpResult pp_eq(void) {
  PpResult result = pp_cmp();
  for (;;) {
    Token *tok;
    if (!(((tok = pp_match(TK_EQ)) != NULL) ||
          ((tok = pp_match(TK_NE)) != NULL)))
      return result;

    PpResult lhs = result, rhs = pp_cmp();
    result = lhs == rhs ? 1 : 0;
    if (tok->kind != TK_EQ)
      result = 1 - result;
  }
}

static PpResult pp_and(void) {
  PpResult result = pp_eq();
  for (;;) {
    Token *tok;
    if ((tok = pp_match(TK_AND)) != NULL) {
      PpResult lhs = result, rhs = pp_eq();
      result = lhs & rhs;
    } else
      return result;
  }
}

static PpResult pp_xor(void) {
  PpResult result = pp_and();
  for (;;) {
    Token *tok;
    if ((tok = pp_match(TK_HAT)) != NULL) {
      PpResult lhs = result, rhs = pp_and();
      result = lhs ^ rhs;
    } else
      return result;
  }
}

static PpResult pp_or(void) {
  PpResult result = pp_xor();
  for (;;) {
    Token *tok;
    if ((tok = pp_match(TK_OR)) != NULL) {
      PpResult lhs = result, rhs = pp_xor();
      result = lhs | rhs;
    } else
      return result;
  }
}

static PpResult pp_logand(void) {
  PpResult result = pp_or();
  for (;;) {
    Token *tok;
    if ((tok = pp_match(TK_LOGAND)) != NULL) {
      PpResult rhs = pp_logand();
      result = result && rhs;
    } else
      return result;
  }
}

static PpResult pp_logior(void) {
  PpResult result = pp_logand();
  for (;;) {
    Token *tok;
    if ((tok = pp_match(TK_LOGIOR)) != NULL) {
      PpResult rhs = pp_logand();
      result = result || rhs;
    } else
      return result;
  }
}

static PpResult pp_conditional(void) {
  PpResult result = pp_logior();
  for (;;) {
    const Token *tok;
    if ((tok = pp_match(TK_QUESTION)) == NULL)
      return result;
    PpResult tval = pp_expr();
    pp_consume(TK_COLON, "`:' expected");
    PpResult fval = pp_conditional();

    result = result ? tval : fval;
  }
}

static PpResult pp_assign(void) {
  return pp_conditional();
}

PpResult pp_expr(void) {
  PpResult result = pp_assign();
  const Token *tok;
  while ((tok = pp_match(TK_COMMA)) != NULL) {
    PpResult next_result = pp_assign();
    result = next_result;
  }
  return result;
}

static Token *match2(enum TokenKind kind) {
  while (pp_match(TK_EOF)) {
    char *line = NULL;
    size_t capa = 0;
    ssize_t len = getline(&line, &capa, pp_stream->fp);
    if (len == -1)
      return NULL;
    ++pp_stream->lineno;
    set_source_string(line, pp_stream->filename, pp_stream->lineno);
  }
  return pp_match(kind);
}

Vector *pp_funargs(void) {
  Vector *args = NULL;
  if (match2(TK_LPAR)) {
    args = new_vector();
    if (!match2(TK_RPAR)) {
      StringBuffer sb;
      sb_init(&sb);
      const char *start = NULL;
      const char *end = NULL;
      int paren = 0;
      for (;;) {
        Token *tok;
        for (;;) {
          tok = pp_match(-1);
          if (tok->kind != TK_EOF)
            break;

          if (start != end)
            sb_append(&sb, start, end);
          if (!sb_empty(&sb))
            sb_append(&sb, "\n", NULL);
          start = end = NULL;

          ssize_t len = -1;
          char *line = NULL;
          if (pp_stream != NULL) {
            size_t capa = 0;
            len = getline_cont(&line, &capa, pp_stream->fp, &pp_stream->lineno);
          }
          if (len == -1) {
            pp_parse_error(NULL, "`)' expected");
            return NULL;
          }
          set_source_string(line, pp_stream->filename, pp_stream->lineno);
        }

        if (tok->kind == TK_COMMA || tok->kind == TK_RPAR) {
          if (paren <= 0) {
            if (sb_empty(&sb)) {
              if (start == end)
                pp_parse_error(tok, "expression expected");
              vec_push(args, strndup_(start, end - start));
            } else {
              if (start != end)
                sb_append(&sb, start, end);
              vec_push(args, sb_to_string(&sb));
              sb_clear(&sb);
            }
            start = end = NULL;

            if (tok->kind == TK_RPAR)
              break;
            else
              continue;
          }

          if (tok->kind == TK_RPAR)
            --paren;
        } else if (tok->kind == TK_LPAR) {
          ++paren;
        }
        if (start == NULL)
          start = tok->begin;
        end = tok->end;
      }
    }
  }
  return args;
}
