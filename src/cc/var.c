#include "var.h"

#include <assert.h>
#include <stdlib.h>  // malloc
#include <string.h>

#include "lexer.h"
#include "table.h"
#include "util.h"

int var_find(const Vector *vars, const Name *name) {
  for (int i = 0, len = vars->len; i < len; ++i) {
    VarInfo *info = vars->data[i];
    if (info->name != NULL && equal_name(info->name, name))
      return i;
  }
  return -1;
}

VarInfo *var_add(Vector *vars, const Name *name, const Type *type, int flag, const Token *ident) {
  const Name *label = NULL;
  VarInfo *ginfo = NULL;
  if (name != NULL) {
    int idx = var_find(vars, name);
    if (idx >= 0)
      parse_error(ident, "`%.*s' already defined", name->bytes, name->chars);
    if (flag & VF_STATIC) {
      label = alloc_label();
      ginfo = define_global(type, flag, NULL, label);
    }
  }

  VarInfo *info = malloc(sizeof(*info));
  info->name = name;
  info->type = type;
  info->flag = flag;
  info->local.label = label;
  info->reg = NULL;
  vec_push(vars, info);
  return ginfo != NULL ? ginfo : info;
}

// Global

static Table gvar_table;

VarInfo *find_global(const Name *name) {
  return table_get(&gvar_table, name);
}

VarInfo *define_global(const Type *type, int flag, const Token *ident, const Name *name) {
  if (name == NULL)
    name = ident->ident;
  VarInfo *varinfo = find_global(name);
  if (varinfo != NULL) {
    if (!(varinfo->flag & VF_EXTERN)) {
      if (!(flag & VF_EXTERN))
        parse_error(ident, "`%.*s' already defined", name->bytes, name->chars);
      return varinfo;
    }
  } else {
    varinfo = malloc(sizeof(*varinfo));
  }
  varinfo->name = name;
  varinfo->type = type;
  varinfo->flag = flag;
  varinfo->global.init = NULL;
  table_put(&gvar_table, name, varinfo);
  return varinfo;
}

// Scope

Scope *new_scope(Scope *parent, Vector *vars) {
  Scope *scope = malloc(sizeof(*scope));
  scope->parent = parent;
  scope->vars = vars;
  return scope;
}

VarInfo *scope_find(Scope *scope, const Name *name, Scope **pscope) {
  VarInfo *varinfo = NULL;
  for (;; scope = scope->parent) {
    if (scope == NULL)
      break;
    if (scope->vars != NULL) {
      int idx = var_find(scope->vars, name);
      if (idx >= 0) {
        varinfo = scope->vars->data[idx];
        break;
      }
    }
  }
  if (pscope != NULL)
    *pscope = scope;
  return varinfo;
}

VarInfo *scope_add(Scope *scope, const Token *ident, const Type *type, int flag) {
  if (scope->vars == NULL)
    scope->vars = new_vector();
  assert(ident != NULL);
  return var_add(scope->vars, ident->ident, type, flag, ident);
}
