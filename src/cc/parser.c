#include "../config.h"
#include "parser.h"

#include <assert.h>
#include <inttypes.h>  // PRIdPTR
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>  // malloc

#include "ast.h"
#include "lexer.h"
#include "table.h"
#include "type.h"
#include "util.h"
#include "var.h"

#define MAX_ERROR_COUNT  (25)

const int LF_BREAK = 1 << 0;
const int LF_CONTINUE = 1 << 0;

Function *curfunc;
static int curloopflag;
Stmt *curswitch;

int compile_error_count;

static Stmt *parse_stmt(void);

VarInfo *add_var_to_scope(Scope *scope, const Token *ident, Type *type, int storage) {
  assert(ident != NULL);
  const Name *name = ident->ident;
  assert(name != NULL);
  if (scope->vars != NULL) {
    int idx = var_find(scope->vars, name);
    if (idx >= 0) {
      VarInfo *varinfo = scope->vars->data[idx];
      if (!(varinfo->storage & VS_EXTERN || storage & VS_EXTERN)) {
        parse_error_nofatal(ident, "`%.*s' already defined", name->bytes, name->chars);
        return varinfo;
      }
    }
  }
  return scope_add(scope, name, type, storage);
}

static void parse_error_valist(const Token *token, const char *fmt, va_list ap) {
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

void parse_error_nofatal(const Token *token, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  parse_error_valist(token, fmt, ap);
  va_end(ap);

  ++compile_error_count;
  if (compile_error_count >= MAX_ERROR_COUNT)
    exit(1);
}

void parse_error(const Token *token, const char *fmt, ...) {
  ++compile_error_count;

  va_list ap;
  va_start(ap, fmt);
  parse_error_valist(token, fmt, ap);
  va_end(ap);

  exit(1);
}

Token *consume(/*enum TokenKind*/int kind, const char *error) {
  Token *tok = match(kind);
  if (tok == NULL)
    parse_error(tok, error);
  return tok;
}

Type *fix_array_size(Type *type, Initializer *init) {
  assert(init != NULL);
  assert(type->kind == TY_ARRAY);

  bool is_str = (is_char_type(type->pa.ptrof) &&
                 init->kind == IK_SINGLE &&
                 init->single->kind == EX_STR);
  if (!is_str && init->kind != IK_MULTI) {
    // Error will be reported in another place.
    return type;
  }

  ssize_t arr_len = type->pa.length;
  if (arr_len == -1) {
    if (is_str) {
      arr_len = init->single->str.size;
    } else {
      ssize_t index = 0;
      ssize_t max_index = 0;
      for (ssize_t i = 0; i < init->multi->len; ++i) {
        Initializer *init_elem = init->multi->data[i];
        if (init_elem->kind == IK_ARR) {
          assert(init_elem->arr.index->kind == EX_FIXNUM);
          index = init_elem->arr.index->fixnum;
        }
        ++index;
        if (max_index < index)
          max_index = index;
      }
      arr_len = max_index;
    }
    Type *cloned = clone_type(type);
    cloned->pa.length = arr_len;
    return cloned;
  } else {
    assert(arr_len > 0);
    assert(!is_str || init->single->kind == EX_STR);
    ssize_t init_len = is_str ? (ssize_t)init->single->str.size : (ssize_t)init->multi->len;
    if (init_len > arr_len && (!is_str || init_len - 1 > arr_len))  // Allow non-nul string.
      parse_error(NULL, "Initializer more than array size");
    return type;
  }
}

static Stmt *build_memcpy(Expr *dst, Expr *src, size_t size) {
  assert(!is_global_scope(curscope));
  Type *charptr_type = ptrof(&tyChar);
  VarInfo *dstvar = add_var_to_scope(curscope, alloc_dummy_ident(), charptr_type, 0);
  VarInfo *srcvar = add_var_to_scope(curscope, alloc_dummy_ident(), charptr_type, 0);
  VarInfo *sizevar = add_var_to_scope(curscope, alloc_dummy_ident(), &tySize, 0);
  Expr *dstexpr = new_expr_variable(dstvar->name, dstvar->type, NULL, curscope);
  Expr *srcexpr = new_expr_variable(srcvar->name, srcvar->type, NULL, curscope);
  Expr *sizeexpr = new_expr_variable(sizevar->name, sizevar->type, NULL, curscope);

  Fixnum size_num_lit = size;
  Expr *size_num = new_expr_fixlit(&tySize, NULL, size_num_lit);

  Fixnum zero = 0;
  Expr *zeroexpr = new_expr_fixlit(&tySize, NULL, zero);

  Vector *stmts = new_vector();
  vec_push(stmts, new_stmt_expr(new_expr_bop(EX_ASSIGN, charptr_type, NULL, dstexpr, dst)));
  vec_push(stmts, new_stmt_expr(new_expr_bop(EX_ASSIGN, charptr_type, NULL, srcexpr, src)));
  vec_push(stmts, new_stmt_for(
      NULL,
      new_expr_bop(EX_ASSIGN, &tySize, NULL, sizeexpr, size_num),    // for (_size = size;
      new_expr_bop(EX_GT, &tyBool, NULL, sizeexpr, zeroexpr),        //      _size > 0;
      new_expr_unary(EX_PREDEC, &tySize, NULL, sizeexpr),            //      --_size)
      new_stmt_expr(                                                 //   *_dst++ = *_src++;
          new_expr_bop(EX_ASSIGN, &tyChar, NULL,
                       new_expr_unary(EX_DEREF, &tyChar, NULL,
                                      new_expr_unary(EX_POSTINC, charptr_type, NULL, dstexpr)),
                       new_expr_unary(EX_DEREF, &tyChar, NULL,
                                      new_expr_unary(EX_POSTINC, charptr_type, NULL, srcexpr))))));
  return new_stmt_block(NULL, stmts, NULL);
}

// Convert string literal to global char-array variable reference.
static Initializer *convert_str_to_ptr_initializer(Scope *scope, Type *type, Initializer *init) {
  assert(type->kind == TY_ARRAY && is_char_type(type->pa.ptrof));
  VarInfo *varinfo = str_to_char_array(scope, type, init, toplevel);
  VarInfo *gvarinfo = is_global_scope(scope) ? varinfo : varinfo->static_.gvar;
  Initializer *init2 = malloc(sizeof(*init2));
  init2->kind = IK_SINGLE;
  init2->single = new_expr_variable(gvarinfo->name, type, NULL, global_scope);
  init2->token = init->token;
  return init2;
}

static Stmt *init_char_array_by_string(Expr *dst, Initializer *src) {
  // Initialize char[] with string literal (char s[] = "foo";).
  assert(src->kind == IK_SINGLE);
  const Expr *str = src->single;
  assert(str->kind == EX_STR);
  assert(dst->type->kind == TY_ARRAY && is_char_type(dst->type->pa.ptrof));

  ssize_t size = str->str.size;
  ssize_t dstsize = dst->type->pa.length;
  if (dstsize == -1) {
    dst->type->pa.length = dstsize = size;
  } else {
    if (dstsize < size - 1)
      parse_error(NULL, "Buffer is shorter than string: %d for \"%s\"", (int)dstsize, str);
  }

  Type *strtype = dst->type;
  VarInfo *varinfo = str_to_char_array(curscope, strtype, src, toplevel);
  Expr *var = new_expr_variable(varinfo->name, strtype, NULL, curscope);
  return build_memcpy(dst, var, size);
}

static int compare_desig_start(const void *a, const void *b) {
  const ssize_t *pa = *(ssize_t**)a;
  const ssize_t *pb = *(ssize_t**)b;
  ssize_t d = *pa - *pb;
  return d > 0 ? 1 : d < 0 ? -1 : 0;
}

static Initializer *flatten_array_initializer(Initializer *init) {
  // Check whether IK_DOT or IK_ARR exists.
  int i = 0, len = init->multi->len;
  for (; i < len; ++i) {
    Initializer *init_elem = init->multi->data[i];
    if (init_elem->kind == IK_DOT)
      parse_error(NULL, "dot initializer for array");
    if (init_elem->kind == IK_ARR)
      break;
  }
  if (i >= len)  // IK_ARR not exits.
    return init;

  // Enumerate designated initializer.
  Vector *ranges = new_vector();  // <(start, count)>
  size_t lastStartIndex = 0;
  size_t lastStart = 0;
  size_t index = i;
  for (; i <= len; ++i, ++index) {  // '+1' is for last range.
    Initializer *init_elem = NULL;
    if (i >= len || (init_elem = init->multi->data[i])->kind == IK_ARR) {
      if (i < len && init_elem->arr.index->kind != EX_FIXNUM)
        parse_error(NULL, "Constant value expected");
      if ((size_t)i > lastStartIndex) {
        size_t *range = malloc(sizeof(size_t) * 3);
        range[0] = lastStart;
        range[1] = lastStartIndex;
        range[2] = index - lastStart;
        vec_push(ranges, range);
      }
      if (i >= len)
        break;
      lastStart = index = init_elem->arr.index->fixnum;
      lastStartIndex = i;
    } else if (init_elem->kind == IK_DOT)
      parse_error(NULL, "dot initializer for array");
  }

  // Sort
  myqsort(ranges->data, ranges->len, sizeof(size_t *), compare_desig_start);

  // Reorder
  Vector *reordered = new_vector();
  index = 0;
  for (int i = 0; i < ranges->len; ++i) {
    size_t *p = ranges->data[i];
    size_t start = p[0];
    size_t index = p[1];
    size_t count = p[2];
    if (i > 0) {
      size_t *q = ranges->data[i - 1];
      if (start < q[0] + q[2])
        parse_error(NULL, "Initializer for array overlapped");
    }
    for (size_t j = 0; j < count; ++j) {
      Initializer *elem = init->multi->data[index + j];
      if (j == 0 && index != start && elem->kind != IK_ARR) {
        Initializer *arr = malloc(sizeof(*arr));
        arr->kind = IK_ARR;
        Fixnum n = start;
        arr->arr.index = new_expr_fixlit(&tyInt, NULL, n);
        arr->arr.value = elem;
        elem = arr;
      }
      vec_push(reordered, elem);
    }
  }

  Initializer *init2 = malloc(sizeof(*init2));
  init2->kind = IK_MULTI;
  init2->multi = reordered;
  return init2;
}

static Initializer *flatten_initializer(Type *type, Initializer *init) {
  if (init == NULL)
    return NULL;

  switch (type->kind) {
  case TY_STRUCT:
    if (init->kind == IK_MULTI) {
      const StructInfo *sinfo = type->struct_.info;
      int n = sinfo->members->len;
      int m = init->multi->len;
      if (n <= 0) {
        if (m > 0)
          parse_error_nofatal(init->token, "Initializer for empty struct");
        return init;
      }
      if (sinfo->is_union && m > 1)
        parse_error(((Initializer*)init->multi->data[1])->token, "Initializer for union more than 1");

      Initializer **values = malloc(sizeof(Initializer*) * n);
      for (int i = 0; i < n; ++i)
        values[i] = NULL;

      int index = 0;
      for (int i = 0; i < m; ++i) {
        Initializer *value = init->multi->data[i];
        if (value->kind == IK_ARR)
          parse_error(NULL, "indexed initializer for struct");

        if (value->kind == IK_DOT) {
          const Name *name = value->dot.name;
          index = var_find(sinfo->members, name);
          if (index >= 0) {
            value = value->dot.value;
          } else {
            Vector *stack = new_vector();
            if (search_from_anonymous(type, name, NULL, stack) == NULL) {
              parse_error_nofatal(value->token, "`%.*s' is not member of struct", name->bytes, name->chars);
              continue;
            }

            index = (intptr_t)stack->data[0];
            Vector *multi = new_vector();
            vec_push(multi, value);
            Initializer *init2 = malloc(sizeof(*init2));
            init2->kind = IK_MULTI;
            init2->multi = multi;
            value = init2;
          }
        }
        if (index >= n)
          parse_error(NULL, "Too many init values");

        // Allocate string literal for char* as a char array.
        if (value->kind == IK_SINGLE && value->single->kind == EX_STR) {
          const VarInfo *member = sinfo->members->data[index];
          if (member->type->kind == TY_PTR &&
              is_char_type(member->type->pa.ptrof)) {
            value = convert_str_to_ptr_initializer(curscope, value->single->type, value);
          }
        }

        values[index++] = value;
      }

      Initializer *flat = malloc(sizeof(*flat));
      flat->kind = IK_MULTI;
      Vector *v = malloc(sizeof(*v));
      v->len = v->capacity = n;
      v->data = (void**)values;
      flat->multi = v;

      return flat;
    }
    break;
  case TY_ARRAY:
    switch (init->kind) {
    case IK_MULTI:
      init = flatten_array_initializer(init);
      break;
    case IK_SINGLE:
      // Special handling for string (char[]), and accept length difference.
      if (init->single->type->kind == TY_ARRAY &&
          can_cast(type->pa.ptrof, init->single->type->pa.ptrof, is_zero(init->single), false))
        break;
      // Fallthrough
    default:
      // Error will be reported in another place.
      break;
    }
    break;
  case TY_PTR:
    {
      Initializer *p = init;
      if (p->kind == IK_ARR)
        p = p->arr.value;
      if (p->kind != IK_SINGLE) {
        parse_error_nofatal(init->token, "Initializer type error");
        break;
      }

      Expr *value = p->single;
      check_cast(type, value->type, is_zero(value), false, init->token);
    }
    break;
  default:
    break;
  }
  return init;
}

static Expr *check_global_initializer_fixnum(Expr *value, bool *isconst) {
  switch (value->kind) {
  case EX_FIXNUM:
#ifndef __NO_FLONUM
  case EX_FLONUM:
#endif
    *isconst = true;
    break;
  case EX_STR:
    // Create string and point to it.
    value = str_to_char_array_var(curscope, value, toplevel);
    *isconst = true;
    break;
  case EX_VAR:
    {
      Scope *scope;
      VarInfo *varinfo = scope_find(value->var.scope, value->var.name, &scope);
      assert(varinfo != NULL);
      if (!is_global_scope(scope) && !(varinfo->storage & VS_STATIC))
        parse_error(value->token, "Allowed global reference only");
      *isconst = value->type->kind == TY_ARRAY || value->type->kind == TY_FUNC ||
                 (value->type->kind == TY_PTR && value->type->pa.ptrof->kind == TY_FUNC);
    }
    break;
  case EX_ADD:
  case EX_SUB:
    {
      bool lhs_const = false, rhs_const = false;
      value->bop.lhs = check_global_initializer_fixnum(value->bop.lhs, &lhs_const);
      value->bop.rhs = check_global_initializer_fixnum(value->bop.rhs, &rhs_const);
      *isconst = lhs_const && rhs_const;
    }
    break;
  case EX_REF:
    value->unary.sub = check_global_initializer_fixnum(value->unary.sub, isconst);
    *isconst = true;
    break;
  case EX_DEREF:
  case EX_CAST:
    value->unary.sub = check_global_initializer_fixnum(value->unary.sub, isconst);
    break;
  case EX_MEMBER:
    value->member.target = check_global_initializer_fixnum(value->member.target, isconst);
    if (value->token->kind != TK_DOT)
      parse_error(value->token, "Allowed global reference only");
    *isconst = value->type->kind == TY_ARRAY;
    break;
  default:
    *isconst = false;
    break;
  }
  return value;
}

static Initializer *check_global_initializer(Type *type, Initializer *init) {
  if (init == NULL)
    return NULL;

  init = flatten_initializer(type, init);

  switch (type->kind) {
#ifndef __NO_FLONUM
  case TY_FLONUM:
    if (init->kind == IK_SINGLE) {
      switch (init->single->kind) {
      case EX_FIXNUM:
        {
          Fixnum fixnum = init->single->fixnum;
          init->single = new_expr_flolit(type, init->single->token, fixnum);
        }
        // Fallthrough
      case EX_FLONUM:
        return init;
      default:
        parse_error_nofatal(init->single->token, "Constant expression expected");
        break;
      }
    }
    break;
#endif
  case TY_FIXNUM:
  case TY_PTR:
    {
      assert(init->kind == IK_SINGLE);
      bool isconst = false;
      Expr *value = check_global_initializer_fixnum(init->single, &isconst);
      init->single = make_cast(type, init->single->token, value, false);
      if (!isconst) {
        parse_error_nofatal(init->single->token, "Initializer must be constant");
      }
    }
    break;
  case TY_ARRAY:
    switch (init->kind) {
    case IK_MULTI:
      {
        Type *elemtype = type->pa.ptrof;
        Vector *multi = init->multi;
        for (int i = 0, len = multi->len; i < len; ++i) {
          Initializer *eleminit = multi->data[i];
          if (eleminit->kind == IK_ARR) {
            eleminit->arr.value = check_global_initializer(elemtype, eleminit->arr.value);
          } else {
            multi->data[i] = check_global_initializer(elemtype, eleminit);
          }
        }
      }
      break;
    case IK_SINGLE:
      if (is_char_type(type->pa.ptrof)) {
        Expr *e = strip_cast(init->single);
        if (e->kind == EX_STR) {
          assert(type->pa.length > 0);
          if ((ssize_t)e->str.size - 1 > type->pa.length) {  // Allow non-nul string.
            parse_error_nofatal(init->single->token, "Array size shorter than initializer");
          }
          break;
        }
      }
      // Fallthrough
    case IK_DOT:
    default:
      parse_error_nofatal(init->token, "Array initializer requires `{'");
      break;
    }
    break;
  case TY_STRUCT:
    {
      if (init->kind == IK_SINGLE) {
        Expr *e = init->single;
        if (e->kind != EX_COMPLIT || !can_cast(type, e->type, false, false)) {
          parse_error_nofatal(init->token, "Struct initializer requires `{'");
          break;
        }
        init = flatten_initializer(type, e->complit.original_init);
        Expr *var = e->complit.var;
        VarInfo *varinfo = scope_find(var->var.scope, var->var.name, NULL);
        assert(varinfo != NULL);
        assert(is_global_scope(var->var.scope));
        varinfo->global.init = init;
      }
      assert(init->kind == IK_MULTI);
      const StructInfo *sinfo = type->struct_.info;
      for (int i = 0, n = sinfo->members->len; i < n; ++i) {
        const VarInfo *member = sinfo->members->data[i];
        Initializer *init_elem = init->multi->data[i];
        if (init_elem != NULL)
          init->multi->data[i] = check_global_initializer(member->type, init_elem);
      }
    }
    break;
  default:
    parse_error_nofatal(NULL, "Global initial value for type %d not implemented (yet)\n", type->kind);
    break;
  }
  return init;
}

Vector *assign_initial_value(Expr *expr, Initializer *init, Vector *inits) {
  if (init == NULL)
    return inits;

  if (inits == NULL)
    inits = new_vector();

  Initializer *org_init = init;
  init = flatten_initializer(expr->type, init);

  switch (expr->type->kind) {
  case TY_ARRAY:
    switch (init->kind) {
    case IK_MULTI:
      {
        ssize_t arr_len = expr->type->pa.length;
        assert(arr_len > 0);
        if (init->multi->len > arr_len)
          parse_error(init->token, "Initializer more than array size");

        assert(!is_global_scope(curscope));
        Type *ptr_type = array_to_ptr(expr->type);
        VarInfo *ptr_varinfo = add_var_to_scope(curscope, alloc_dummy_ident(), ptr_type, 0);
        Expr *ptr_var = new_expr_variable(ptr_varinfo->name, ptr_type, NULL, curscope);
        vec_push(inits, new_stmt_expr(new_expr_bop(EX_ASSIGN, ptr_type, NULL, ptr_var, expr)));

        const size_t len = init->multi->len;
        const size_t elem_size = type_size(expr->type->pa.ptrof);
        size_t prev_index = 0, index = 0;
        for (size_t i = 0; i < len; ++i) {
          Initializer *init_elem = init->multi->data[i];
          if (init_elem->kind == IK_ARR) {
            Expr *ind = init_elem->arr.index;
            if (ind->kind != EX_FIXNUM)
              parse_error(init_elem->token, "Number required");
            index = ind->fixnum;
            init_elem = init_elem->arr.value;
          }

          size_t add = index - prev_index;
          if (add > 0) {
            const Fixnum n = add * elem_size;
            vec_push(inits, new_stmt_expr(
                new_expr_unary(EX_MODIFY, ptr_type, NULL,
                               new_expr_bop(EX_ADD, ptr_type, NULL, ptr_var,
                                            new_expr_fixlit(&tySize, NULL, n)))));
          }

          assign_initial_value(new_expr_deref(NULL, ptr_var), init_elem, inits);
          prev_index = index++;
        }
      }
      break;
    case IK_SINGLE:
      // Special handling for string (char[]).
      if (is_char_type(expr->type->pa.ptrof) &&
          init->single->kind == EX_STR) {
        vec_push(inits, init_char_array_by_string(expr, init));
        break;
      }
      // Fallthrough
    default:
      parse_error_nofatal(init->token, "Array initializer requires `{'");
      break;
    }
    break;
  case TY_STRUCT:
    {
      if (init->kind == IK_SINGLE) {
        Expr *e = init->single;
        if (can_cast(expr->type, e->type, false, false)) {
          vec_push(inits, new_stmt_expr(new_expr_bop(EX_ASSIGN, expr->type, init->token, expr, e)));
          break;
        }
      }
      if (init->kind != IK_MULTI) {
        parse_error_nofatal(init->token, "Struct initializer requires `{'");
        break;
      }

      const StructInfo *sinfo = expr->type->struct_.info;
      if (!sinfo->is_union) {
        for (int i = 0, n = sinfo->members->len; i < n; ++i) {
          const VarInfo *member = sinfo->members->data[i];
          Expr *mem = new_expr_member(NULL, member->type, expr, NULL, i);
          Initializer *init_elem = init->multi->data[i];
          if (init_elem != NULL)
            assign_initial_value(mem, init_elem, inits);
        }
      } else {
        int n = sinfo->members->len;
        int m = init->multi->len;
        if (n <= 0 && m > 0)
          parse_error(init->token, "Initializer for empty union");
        if (org_init->multi->len > 1)
          parse_error(init->token, "More than one initializer for union");

        for (int i = 0; i < n; ++i) {
          Initializer *init_elem = init->multi->data[i];
          if (init_elem == NULL)
            continue;
          const VarInfo *member = sinfo->members->data[i];
          Expr *mem = new_expr_member(NULL, member->type, expr, NULL, i);
          assign_initial_value(mem, init_elem, inits);
          break;
        }
      }
    }
    break;
  default:
    switch (init->kind) {
    case IK_MULTI:
      if (init->multi->len != 1 || ((Initializer*)init->multi->data[0])->kind != IK_SINGLE) {
        parse_error_nofatal(init->token, "Requires scaler");
        break;
      }
      init = init->multi->data[0];
      // Fallthrough
    case IK_SINGLE:
      {
        Expr *value = str_to_char_array_var(curscope, init->single, toplevel);
        vec_push(inits,
                 new_stmt_expr(new_expr_bop(EX_ASSIGN, expr->type, init->token, expr,
                                            make_cast(expr->type, init->token, value, false))));
      }
      break;
    default:
      parse_error(init->token, "Error initializer");
      break;
    }
    break;
  }

  return inits;
}

Vector *construct_initializing_stmts(Vector *decls) {
  Vector *inits = NULL;
  for (int i = 0; i < decls->len; ++i) {
    VarDecl *decl = decls->data[i];
    if (decl->storage & VS_STATIC)
      continue;
    Expr *var = new_expr_variable(decl->ident->ident, decl->type, NULL, curscope);
    inits = assign_initial_value(var, decl->init, inits);
  }
  return inits;
}

static Initializer *check_vardecl(Type **ptype, const Token *ident, int storage, Initializer *init) {
  Type *type = *ptype;
  if (type->kind == TY_ARRAY && init != NULL)
    *ptype = type = fix_array_size(type, init);
  if (!(storage & VS_EXTERN))
    ensure_struct(type, ident, curscope);

  if (curfunc != NULL) {
    VarInfo *varinfo = scope_find(curscope, ident->ident, NULL);
    varinfo->type = type;

    // TODO: Check `init` can be cast to `type`.
    if (storage & VS_STATIC) {
      VarInfo *gvarinfo = varinfo->static_.gvar;
      assert(gvarinfo != NULL);
      gvarinfo->global.init = init = check_global_initializer(type, init);
      gvarinfo->type = type;
      // static variable initializer is handled in codegen, same as global variable.
    }
  } else {
    //intptr_t eval;
    //if (find_enum_value(ident->ident, &eval))
    //  parse_error(ident, "`%.*s' is already defined", ident->ident->bytes, ident->ident->chars);
    if (storage & VS_EXTERN && init != NULL) {
      parse_error_nofatal(init->token, "extern with initializer");
      return NULL;
    }
    // Toplevel
    VarInfo *gvarinfo = scope_find(global_scope, ident->ident, NULL);
    assert(gvarinfo != NULL);
    gvarinfo->global.init = init = check_global_initializer(type, init);
    gvarinfo->type = type;
  }
  return init;
}

static void add_func_label(const Token *label) {
  assert(curfunc != NULL);
  Table *table = curfunc->label_table;
  if (table == NULL) {
    curfunc->label_table = table = alloc_table();
  }
  if (!table_put(table, label->ident, (void*)-1))  // Put dummy value.
    parse_error_nofatal(label, "Label `%.*s' already defined", label->ident->bytes, label->ident->chars);
}

static void add_func_goto(Stmt *stmt) {
  assert(curfunc != NULL);
  if (curfunc->gotos == NULL)
    curfunc->gotos = new_vector();
  vec_push(curfunc->gotos, stmt);
}

// Scope

static Scope *enter_scope(Function *func, Vector *vars) {
  Scope *scope = new_scope(curscope, vars);
  curscope = scope;
  vec_push(func->scopes, scope);
  return scope;
}

static void exit_scope(void) {
  assert(!is_global_scope(curscope));
  curscope = curscope->parent;
}

// Initializer

Initializer *parse_initializer(void) {
  Initializer *result = malloc(sizeof(*result));
  const Token *lblace_tok;
  if ((lblace_tok = match(TK_LBRACE)) != NULL) {
    Vector *multi = new_vector();
    if (!match(TK_RBRACE)) {
      for (;;) {
        Initializer *init;
        const Token *tok;
        if (match(TK_DOT)) {  // .member=value
          Token *ident = consume(TK_IDENT, "`ident' expected for dotted initializer");
          consume(TK_ASSIGN, "`=' expected for dotted initializer");
          Initializer *value = parse_initializer();
          init = malloc(sizeof(*init));
          init->kind = IK_DOT;
          init->token = ident;
          init->dot.name = ident->ident;
          init->dot.value = value;
        } else if ((tok = match(TK_LBRACKET)) != NULL) {
          Expr *index = parse_const();
          consume(TK_RBRACKET, "`]' expected");
          match(TK_ASSIGN);  // both accepted: `[1] = 2`, and `[1] 2`
          Initializer *value = parse_initializer();
          init = malloc(sizeof(*init));
          init->kind = IK_ARR;
          init->token = tok;
          init->arr.index = index;
          init->arr.value = value;
        } else {
          init = parse_initializer();
        }
        vec_push(multi, init);

        if (match(TK_COMMA)) {
          if (match(TK_RBRACE))
            break;
        } else {
          consume(TK_RBRACE, "`}' or `,' expected");
          break;
        }
      }
    }
    result->kind = IK_MULTI;
    result->token = lblace_tok;
    result->multi = multi;
  } else {
    result->kind = IK_SINGLE;
    result->single = parse_assign();
    result->token = result->single->token;
  }
  return result;
}

static bool def_type(Type *type, Token *ident) {
  const Name *name = ident->ident;
  Scope *scope;
  Type *conflict = find_typedef(curscope, name, &scope);
  if (conflict != NULL && scope == curscope) {
    if (!same_type(type, conflict))
      parse_error(ident, "Conflict typedef");
  } else {
    conflict = NULL;
  }

  if (conflict == NULL || (type->kind == TY_STRUCT && type->struct_.info != NULL)) {
    if (type->kind == TY_ARRAY) {
      ensure_struct(type, ident, curscope);
    }
    add_typedef(curscope, name, type);
    return true;
  } else {
    return false;
  }
}

static Vector *parse_vardecl_cont(Type *rawType, Type *type, int storage, Token *ident) {
  Vector *decls = NULL;
  bool first = true;
  do {
    int tmp_storage = storage;
    if (!first) {
      type = parse_var_def(&rawType, &tmp_storage, &ident);
      if (type == NULL || ident == NULL) {
        parse_error(NULL, "`ident' expected");
        return NULL;
      }
    }
    first = false;

    Initializer *init = NULL;
    if (match(TK_LPAR)) {  // Function prototype.
      bool vaargs;
      Vector *params = parse_funparams(&vaargs);
      Vector *param_types = extract_varinfo_types(params);
      type = new_func_type(type, params, param_types, vaargs);
    } else {
      not_void(type, NULL);
    }

    if (type->kind == TY_FUNC /* && !is_global_scope(curscope)*/) {
      // Must be prototype.
      tmp_storage |= VS_EXTERN;
    }

    assert(!is_global_scope(curscope));

    if (tmp_storage & VS_TYPEDEF) {
      def_type(type, ident);
      continue;
    }

    VarInfo *varinfo = add_var_to_scope(curscope, ident, type, tmp_storage);
    varinfo->type = type;  // type might be changed.
    if (type->kind != TY_FUNC && match(TK_ASSIGN))
      init = parse_initializer();
    init = check_vardecl(&type, ident, tmp_storage, init);
    VarDecl *decl = new_vardecl(type, ident, init, tmp_storage);
    if (decls == NULL)
      decls = new_vector();
    vec_push(decls, decl);
  } while (match(TK_COMMA));
  return decls;
}

static bool parse_vardecl(Stmt **pstmt) {
  Type *rawType = NULL;
  int storage;
  Token *ident;
  Type *type = parse_var_def(&rawType, &storage, &ident);
  if (type == NULL)
    return false;

  *pstmt = NULL;
  if (ident == NULL) {
    if ((type->kind == TY_STRUCT ||
         (type->kind == TY_FIXNUM && type->fixnum.kind == FX_ENUM)) &&
         match(TK_SEMICOL)) {
      // Just struct/union or enum definition.
    } else {
      parse_error(NULL, "Ident expected");
    }
  } else {
    Vector *decls = parse_vardecl_cont(rawType, type, storage, ident);
    consume(TK_SEMICOL, "`;' expected");
    if (decls != NULL) {
      Vector *inits = !is_global_scope(curscope) ? construct_initializing_stmts(decls) : NULL;
      *pstmt = new_stmt_vardecl(decls, inits);
    }
  }
  return true;
}

static Stmt *parse_if(const Token *tok) {
  consume(TK_LPAR, "`(' expected");
  Expr *cond = make_cond(parse_expr());
  consume(TK_RPAR, "`)' expected");
  Stmt *tblock = parse_stmt();
  Stmt *fblock = NULL;
  if (match(TK_ELSE)) {
    fblock = parse_stmt();
  }
  return new_stmt_if(tok, cond, tblock, fblock);
}

static Stmt *parse_switch(const Token *tok) {
  consume(TK_LPAR, "`(' expected");
  Expr *value = parse_expr();
  not_void(value->type, value->token);
  consume(TK_RPAR, "`)' expected");

  Stmt *swtch = new_stmt_switch(tok, value);
  Stmt *save_switch = curswitch;
  int save_flag = curloopflag;
  curloopflag |= LF_BREAK;
  curswitch = swtch;

  swtch->switch_.body = parse_stmt();

  curloopflag = save_flag;
  curswitch = save_switch;

  return swtch;
}

static Stmt *parse_case(const Token *tok) {
  Expr *value = parse_const();
  consume(TK_COLON, "`:' expected");
  assert(value->kind == EX_FIXNUM);

  Stmt *stmt = new_stmt_case(tok, value);
  if (curswitch == NULL) {
    parse_error(tok, "`case' cannot use outside of `switch`");
  } else {
    // Check duplication.
    Fixnum v = value->fixnum;
    Vector *cases = curswitch->switch_.cases;
    for (int i = 0, len = cases->len; i < len; ++i) {
      Stmt *c = cases->data[i];
      if (c->case_.value == NULL)
        continue;
      if (c->case_.value->fixnum == v)
        parse_error_nofatal(tok, "Case value `%" PRIdPTR "' already defined", v);
    }
    vec_push(cases, stmt);
  }
  return stmt;
}

static Stmt *parse_default(const Token *tok) {
  consume(TK_COLON, "`:' expected");

  Stmt *stmt = new_stmt_default(tok);
  if (curswitch == NULL) {
    parse_error_nofatal(tok, "`default' cannot use outside of `switch'");
  } else if (curswitch->switch_.default_ != NULL) {
    parse_error_nofatal(tok, "`default' already defined in `switch'");
  } else {
    curswitch->switch_.default_ = stmt;
    vec_push(curswitch->switch_.cases, stmt);
  }
  return stmt;
}

static Stmt *parse_while(const Token *tok) {
  consume(TK_LPAR, "`(' expected");
  Expr *cond = make_cond(parse_expr());
  consume(TK_RPAR, "`)' expected");

  int save_flag = curloopflag;
  curloopflag |= LF_BREAK | LF_CONTINUE;

  Stmt *body = parse_stmt();

  curloopflag = save_flag;

  return new_stmt_while(tok, cond, body);
}

static Stmt *parse_do_while(void) {
  int save_flag = curloopflag;
  curloopflag |= LF_BREAK | LF_CONTINUE;

  Stmt *body = parse_stmt();

  curloopflag = save_flag;

  const Token *tok = consume(TK_WHILE, "`while' expected");
  consume(TK_LPAR, "`(' expected");
  Expr *cond = make_cond(parse_expr());
  consume(TK_RPAR, "`)' expected");
  consume(TK_SEMICOL, "`;' expected");
  return new_stmt_do_while(body, tok, cond);
}

static Stmt *parse_for(const Token *tok) {
  consume(TK_LPAR, "`(' expected");
  Expr *pre = NULL;
  Vector *decls = NULL;
  Scope *scope = NULL;
  if (!match(TK_SEMICOL)) {
    Type *rawType = NULL;
    int storage;
    Token *ident;
    Type *type = parse_var_def(&rawType, &storage, &ident);
    if (type != NULL) {
      if (ident == NULL)
        parse_error(NULL, "Ident expected");
      scope = enter_scope(curfunc, NULL);
      decls = parse_vardecl_cont(rawType, type, storage, ident);
      consume(TK_SEMICOL, "`;' expected");
    } else {
      pre = parse_expr();
      consume(TK_SEMICOL, "`;' expected");
    }
  }

  Expr *cond = NULL;
  Expr *post = NULL;
  Stmt *body = NULL;
  if (!match(TK_SEMICOL)) {
    cond = make_cond(parse_expr());
    consume(TK_SEMICOL, "`;' expected");
  }
  if (!match(TK_RPAR)) {
    post = parse_expr();
    consume(TK_RPAR, "`)' expected");
  }

  int save_flag = curloopflag;
  curloopflag |= LF_BREAK | LF_CONTINUE;

  body = parse_stmt();

  Vector *stmts = new_vector();
  if (decls != NULL) {
    Vector *inits = construct_initializing_stmts(decls);
    vec_push(stmts, new_stmt_vardecl(decls, inits));
  }

  curloopflag = save_flag;

  if (scope != NULL)
    exit_scope();

  Stmt *stmt = new_stmt_for(tok, pre, cond, post, body);
  vec_push(stmts, stmt);
  return new_stmt_block(tok, stmts, scope);
}

static Stmt *parse_break_continue(enum StmtKind kind, const Token *tok) {
  consume(TK_SEMICOL, "`;' expected");
  if ((curloopflag & LF_BREAK) == 0) {
    const char *err;
    if (kind == ST_BREAK)
      err = "`break' cannot be used outside of loop";
    else
      err = "`continue' cannot be used outside of loop";
    parse_error_nofatal(tok, err);
  }
  return new_stmt(kind, tok);
}

static Stmt *parse_goto(const Token *tok) {
  Token *label = consume(TK_IDENT, "label for goto expected");
  consume(TK_SEMICOL, "`;' expected");

  Stmt *stmt = new_stmt_goto(tok, label);
  add_func_goto(stmt);
  return stmt;
}

static Stmt *parse_label(const Token *label) {
  Stmt *stmt = new_stmt_label(label, parse_stmt());
  add_func_label(label);
  return stmt;
}

static Stmt *parse_return(const Token *tok) {
  Expr *val = NULL;
  if (!match(TK_SEMICOL)) {
    val = parse_expr();
    consume(TK_SEMICOL, "`;' expected");
    val = str_to_char_array_var(curscope, val, toplevel);
  }

  assert(curfunc != NULL);
  Type *rettype = curfunc->type->func.ret;
  if (val == NULL) {
    if (rettype->kind != TY_VOID)
      parse_error_nofatal(tok, "`return' required a value");
  } else {
    if (rettype->kind == TY_VOID)
      parse_error_nofatal(val->token, "void function `return' a value");
    else
      val = make_cast(rettype, val->token, val, false);
  }

  return new_stmt_return(tok, val);
}

static Expr *parse_asm_arg(void) {
  /*const Token *str =*/ consume(TK_STR, "string literal expected");
  consume(TK_LPAR, "`(' expected");
  Expr *var = parse_expr();
  if (var == NULL || var->kind != EX_VAR) {
    parse_error(var != NULL ? var->token : NULL, "string literal expected");
  }
  consume(TK_RPAR, "`)' expected");
  return var;
}

static Stmt *parse_asm(const Token *tok) {
  consume(TK_LPAR, "`(' expected");

  Expr *str = parse_expr();
  if (str == NULL || str->kind != EX_STR) {
    parse_error(str != NULL ? str->token : NULL, "`__asm' expected string literal");
  }

  Expr *arg = NULL;
  if (match(TK_COLON)) {
    arg = parse_asm_arg();
  }

  consume(TK_RPAR, "`)' expected");
  consume(TK_SEMICOL, "`;' expected");
  return new_stmt_asm(tok, str, arg);
}

// Multiple stmt-s, also accept `case` and `default`.
static Vector *parse_stmts(void) {
  Vector *stmts = new_vector();
  for (;;) {
    Stmt *stmt;
    Token *tok;
    if (parse_vardecl(&stmt)) {
      if (stmt == NULL)
        continue;
    } else if ((tok = match(TK_CASE)) != NULL)
      stmt = parse_case(tok);
    else if ((tok = match(TK_DEFAULT)) != NULL)
      stmt = parse_default(tok);
    else
      stmt = parse_stmt();

    if (stmt == NULL) {
      if (match(TK_RBRACE))
        return stmts;
      parse_error(NULL, "`}' expected");
    }
    vec_push(stmts, stmt);
  }
}

Stmt *parse_block(const Token *tok) {
  Scope *scope = enter_scope(curfunc, NULL);
  Vector *stmts = parse_stmts();
  Stmt *stmt = new_stmt_block(tok, stmts, scope);
  exit_scope();
  return stmt;
}

static Stmt *parse_stmt(void) {
  Token *tok = match(-1);
  switch (tok->kind) {
  case TK_RBRACE:
  case TK_EOF:
    unget_token(tok);
    return NULL;
  case TK_IDENT:
    if (match(TK_COLON))
      return parse_label(tok);
    break;
  case TK_SEMICOL:
    return new_stmt_block(tok, NULL, NULL);
  case TK_LBRACE:
    return parse_block(tok);
  case TK_IF:
    return parse_if(tok);
  case TK_SWITCH:
    return parse_switch(tok);
  case TK_WHILE:
    return parse_while(tok);
  case TK_DO:
    return parse_do_while();
  case TK_FOR:
    return parse_for(tok);
  case TK_BREAK: case TK_CONTINUE:
    return parse_break_continue(tok->kind == TK_BREAK ? ST_BREAK : ST_CONTINUE, tok);
  case TK_GOTO:
    return parse_goto(tok);
  case TK_RETURN:
    return parse_return(tok);
  case TK_ASM:
    return parse_asm(tok);
  default:
    break;
  }

  unget_token(tok);

  // expression statement.
  Expr *val = parse_expr();
  consume(TK_SEMICOL, "`;' expected");
  return new_stmt_expr(str_to_char_array_var(curscope, val, toplevel));
}

static Declaration *parse_defun(Type *functype, int storage, Token *ident) {
  assert(functype->kind == TY_FUNC);

  bool prototype = match(TK_SEMICOL) != NULL;
  if (!prototype && functype->func.params == NULL) { // Old-style
    // Treat it as a zero-parameter function.
    functype->func.params = functype->func.param_types = new_vector();
    functype->func.vaargs = false;
  }

  Function *func = new_func(functype, ident->ident);
  VarInfo *varinfo = scope_find(global_scope, func->name, NULL);
  bool err = false;
  if (varinfo == NULL) {
    varinfo = add_var_to_scope(global_scope, ident, functype, storage);
  } else {
    if (varinfo->type->kind != TY_FUNC ||
        !same_type(varinfo->type->func.ret, functype->func.ret) ||
        (varinfo->type->func.params != NULL && !same_type(varinfo->type, functype))) {
      parse_error_nofatal(ident, "Definition conflict: `%.*s'", func->name->bytes, func->name->chars);
      err = true;
    } else {
      if (varinfo->global.func == NULL) {
        if (varinfo->type->func.params == NULL)  // Old-style prototype definition.
          varinfo->type = functype;  // Overwrite with actual function type.
      }
    }
  }

  if (prototype) {
    // Prototype declaration.
  } else {
    consume(TK_LBRACE, "`;' or `{' expected");

    if (!err && varinfo->global.func != NULL) {
      parse_error_nofatal(ident, "`%.*s' function already defined", func->name->bytes,
                          func->name->chars);
    } else {
      varinfo->global.func = func;
    }

    assert(curfunc == NULL);
    assert(is_global_scope(curscope));
    curfunc = func;
    Vector *top_vars = NULL;
    const Vector *params = func->type->func.params;
    if (params != NULL) {
      top_vars = new_vector();
      for (int i = 0; i < params->len; ++i)
        vec_push(top_vars, params->data[i]);
    }
    func->scopes = new_vector();
    enter_scope(func, top_vars);  // Scope for parameters.
    func->stmts = parse_stmts();
    exit_scope();
    assert(is_global_scope(curscope));

    // Check goto labels.
    if (func->gotos != NULL) {
      Vector *gotos = func->gotos;
      Table *label_table = func->label_table;
      for (int i = 0; i < gotos->len; ++i) {
        Stmt *stmt = gotos->data[i];
        if (label_table == NULL || !table_try_get(label_table, stmt->goto_.label->ident, NULL)) {
          const Name *name = stmt->goto_.label->ident;
          parse_error_nofatal(stmt->goto_.label, "`%.*s' not found", name->bytes, name->chars);
        }
      }
    }

    curfunc = NULL;
  }
  return new_decl_defun(func);
}

static Declaration *parse_global_var_decl(
    Type *rawtype, int storage, Type *type, Token *ident
) {
  Vector *decls = NULL;
  for (;;) {
    if (!(type->kind == TY_PTR && type->pa.ptrof->kind == TY_FUNC) &&
        type->kind != TY_VOID)
      type = parse_type_suffix(type);

    if (storage & VS_TYPEDEF) {
      def_type(type, ident);
    } else {
      if (type->kind == TY_VOID)
        parse_error(ident, "`void' not allowed");

      VarInfo *varinfo = add_var_to_scope(global_scope, ident, type, storage);

      Initializer *init = NULL;
      if (match(TK_ASSIGN) != NULL)
        init = parse_initializer();
      varinfo->global.init = init;

      init = check_vardecl(&type, ident, storage, init);
      varinfo->type = type;  // type might be changed.
      VarDecl *decl = new_vardecl(type, ident, init, storage);
      if (decls == NULL)
        decls = new_vector();
      vec_push(decls, decl);
    }

    if (!match(TK_COMMA))
      break;

    // Next declaration.
    type = parse_type_modifier(rawtype);
    ident = consume(TK_IDENT, "`ident' expected");
  }

  consume(TK_SEMICOL, "`;' or `,' expected");

  if (decls == NULL)
    return NULL;
  return new_decl_vardecl(decls);
}

static Declaration *parse_declaration(void) {
  Type *rawtype = NULL;
  int storage;
  Token *ident;
  Type *type = parse_var_def(&rawtype, &storage, &ident);
  if (type != NULL) {
    if (ident == NULL) {
      if ((type->kind == TY_STRUCT ||
           (type->kind == TY_FIXNUM && type->fixnum.kind == FX_ENUM)) &&
          match(TK_SEMICOL)) {
        // Just struct/union or enum definition.
      } else {
        parse_error(NULL, "Ident expected");
      }
      return NULL;
    }

    if (type->kind == TY_FUNC) {
      if (storage & VS_TYPEDEF) {
        consume(TK_SEMICOL, "`;' expected");
        assert(ident != NULL);
        def_type(type, ident);
        return NULL;
      }
      return parse_defun(type, storage, ident);
    }

    return parse_global_var_decl(rawtype, storage, type, ident);
  }
  parse_error(NULL, "Unexpected token");
  return NULL;
}

void parse(Vector *decls) {
  curscope = global_scope;

  while (!match(TK_EOF)) {
    Declaration *decl = parse_declaration();
    if (decl != NULL)
      vec_push(decls, decl);
  }
}
