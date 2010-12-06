/*
 * Copyright (c) 2010 Øyvind Kolås <pippin@gimp.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "lyd-private.h"

/* This code is heavily inspired by:
 *    http://javascript.crockford.com/tdop/tdop.html
 * Even though it is claimed that the approach works best for dynamic languages
 * I found that it also maps quite well to C.
 */

/* XXX: leaking, what seems to be ) tokens, since they do not become part of
 * the tree.
 */

typedef struct {
  char     *str;
  LydOpCode op;
} LydOpCodeMap;

/* table mapping keywords of the language to op-codes */
static LydOpCodeMap op_lexicon[] = {
  {"none",       LYD_NONE},
#define LYD_OP(NAME, OP_CODE, ARGS, CODE, DOC, ARG_DOC)  {NAME, LYD_##OP_CODE},
#include "lyd-ops.inc"
#undef LYD_OP
};

typedef struct {
  char *str;
  float value;
} LydConstantMap;

static LydConstantMap constant_lexicon[] = {
  {"pi",   3.141592653589793},
  {"phi",  1.61803399},
  {"iphi", 0.61803399},
};

typedef enum {
  name, string, literal, operator, function, variable, binary, unary, args
} LydTokenType;

typedef struct _LydToken  LydToken;
typedef struct _LydParser LydParser;

/* Tokens are used both to describe the grammar and construct the syntax tree.
 */
struct _LydToken {
  char         *str;   
  LydTokenType  type;
  int           left_binding_power;
  LydToken      *(*nud) (LydParser *parser, LydToken *this);
  int           right_binding_power;
  LydToken      *(*led) (LydParser *parser, LydToken *this, LydToken *left);
  /****************/
  float          value; /* this token interpreted as a floating point number
                         * (for literals) */
  int            command_no;
  LydToken      *first;
  LydToken      *second;
  LydToken      *args[LYD_MAX_ARGS]; 
};

struct _LydParser  {
  Lyd        *lyd;
  LydToken   *token;
  const char *buf;
  const char *p;
  char       *error;
  int         variables;
  float       variable[LYD_MAX_VARIABLES];
  float       var_default[LYD_MAX_VARIABLES];
  LydToken   *tree;
};

/* forward declarations */
static LydToken *nud_default (LydParser *parser, LydToken *this);
static LydToken *nud_itself  (LydParser *parser, LydToken *this);
static LydToken *nud_prefix  (LydParser *parser, LydToken *this);
static LydToken *nud_lparen  (LydParser *parser, LydToken *this);
static LydToken *led_default (LydParser *parser, LydToken *this, LydToken *left);
static LydToken *led_infix   (LydParser *parser, LydToken *this, LydToken *left);
static LydToken *led_lparen  (LydParser *parser, LydToken *this, LydToken *left);

/* When used as a symbol only the first 6 fields of the token are used. */
static LydToken symbols[]= {
  {"(end)", operator, 0,  nud_default, 0,  led_default},
  {"(fun)", function, 10, nud_itself,  0,  led_default},
  {"(var)", variable, 0,  nud_itself,  0,  led_default},
  {"(lit)", literal,  0,  nud_itself,  0,  led_default},
  {";",     operator, 0,  nud_default, 0,  led_default},
  {",",     operator, 0,  nud_default, 0,  led_default},
  {")",     operator, 0,  nud_default, 0,  led_default},
  {"-",     operator, 50, nud_prefix,  50, led_infix},
  {"+",     operator, 50, nud_default, 50, led_infix},
  {"*",     operator, 60, nud_default, 60, led_infix},
  {"/",     operator, 60, nud_default, 60, led_infix},
  {"%",     operator, 60, nud_default, 60, led_infix},
  {"^",     operator, 70, nud_default, 70, led_infix},
  {"(",     operator, 80, nud_lparen,  0,  led_lparen},
};

/* forward declarations */
static LydToken * parser_expression (LydParser *parser, int right_binding_power);
static LydToken * parser_advance    (LydParser *parser, const char *str);
static void token_free (LydToken *token);

static LydToken *nud_default (LydParser *parser, LydToken *this)
{
  parser->error = "nud_default";
  return NULL;
}

static LydToken *nud_itself (LydParser *parser, LydToken *this)
{
  return this;
}

static LydToken *led_default (LydParser *parser, LydToken *this, LydToken *left)
{
  parser->error = "led_default";
  return NULL;
}

static LydToken *led_infix (LydParser *parser, LydToken *this, LydToken *left)
{
  this->first = left;
  this->second = parser_expression(parser, this->right_binding_power);
  this->type = binary;
  return this;
}

static LydToken *led_lparen (LydParser *parser, LydToken *this, LydToken *left)
{
  this->type = args;
  this->first = left;
  if (left->type != function)
    parser->error = "( error";
  if (parser->error)
    return NULL;
  if (strcmp (parser->token->str, ")"))
    {
      int i;
      for (i=0; i< LYD_MAX_ARGS; i++)
        {
          this->args[i] = parser_expression (parser, 0);
          if (strcmp (parser->token->str, ","))
            break;
          left = parser->token;
          parser_advance (parser, ",");
          token_free (left);
        }
    }
  parser_advance(parser, ")");
  return this;
}

static LydToken *nud_lparen (LydParser *parser, LydToken *this)
{
  LydToken *e = parser_expression(parser, 0);
  parser_advance (parser, ")");
  token_free (this);
  return e;
}

static LydToken *nud_prefix (LydParser *parser, LydToken *this)
{
  this->first = parser_expression (parser, 70);
  this->type = unary;
  return this;
}

#define N_ELEMENTS(a)  (sizeof(a)/sizeof(a[0]))

static LydToken *parser_lookup (LydParser *parser,
                                char   *str)
{
  int i;
  for (i=0; i<N_ELEMENTS (symbols); i++)
    {
      if (!strcmp (symbols[i].str, str))
        return &symbols[i];
    }
  return NULL;
}

static LydOpCode str2opcode (const char *str)
{
  int i;
  for (i=0 ; i < N_ELEMENTS (op_lexicon); i++)
    if (!strcmp(str, op_lexicon[i].str))
      return op_lexicon[i].op;
  return 0;
}

static float str2constant (const char *str)
{
  int i;
  for (i=0 ; i < N_ELEMENTS (constant_lexicon); i++)
    if (!strcmp(str, constant_lexicon[i].str))
      return constant_lexicon[i].value;
  return 0.0;
}
#define is_op(str)       (str2opcode (str)!=0)
#define is_constant(str) (str2constant (str)!=0.0)

static int oneof (char needle, char *haystack)
{
  char *p;
  for (p=haystack; *p; p++)
    if (needle == *p)
      return 1;
  return 0;
}

#define MAX_TOK_LEN 512

static LydToken *parser_scanner_next (LydParser *parser)
{
  char word[MAX_TOK_LEN]="";
  int   wpos=0;
  int   incomment=0;
  char *whitespace = "\n ";
  char *numerals   = "0123456789.";
  char *operators  = "+-/*%(),";

  LydToken *tok = g_new0 (LydToken, 1);
  
  /* swallow whitespace */
  while (*parser->p
       && (oneof (*parser->p, whitespace)
           || *parser->p == '#'
           || incomment))
    {
      if (*parser->p == '#')
        incomment = TRUE;
      else if (incomment && *parser->p == '\n')
        incomment = FALSE;
      parser->p++;
    }
  wpos = 0;

  if (*parser->p == '"')
    {
      parser->p++;
      while (*parser->p && *parser->p !='"')
        word[wpos++]=*parser->p++;
      if (wpos>=MAX_TOK_LEN)
        wpos=MAX_TOK_LEN-1;
      word[wpos]='\0';
      if (*parser->p=='"')
        parser->p++;
      tok->type = string;
    }
  else if (*parser->p == '\'')
    {
      parser->p++;
      while (*parser->p && *parser->p !='\'')
        word[wpos++]=*parser->p++;
      if (wpos>=MAX_TOK_LEN)
        wpos=MAX_TOK_LEN-1;
      word[wpos]='\0';
      if (*parser->p=='\'')
        parser->p++;
      tok->type = string;
    }
  else if (oneof (*parser->p, numerals))
    {
      while (*parser->p && oneof (*parser->p, numerals) && wpos<MAX_TOK_LEN)
        {
          word[wpos++]=*parser->p++;
          if (wpos>=MAX_TOK_LEN)
            wpos=MAX_TOK_LEN-1;
          word[wpos]='\0';
        }
      tok->type = literal;
    }
  else if (oneof (*parser->p, operators))
    word[wpos++]=*parser->p++;
  else
    {
      while (*parser->p
             && !oneof (*parser->p, whitespace)
             && !oneof (*parser->p, operators)
             && wpos<MAX_TOK_LEN)
        {
          word[wpos++]=*parser->p++;
          if (word[wpos-1]=='=' && &parser->p) /* directly swallow next char
                                                  after = to allow negative values
                                                */
            word[wpos++]=*parser->p++;
        }
      tok->type = name;
    }
  word[wpos]=0;

  if (!wpos)
    {
      g_free (tok->str);
      g_free (tok);
      return NULL;
    }
  tok->str = g_strdup (word);
  return tok;
}

static void token_free (LydToken *token)
{
  if (token->str)
    g_free (token->str);
  g_free (token);
}

static int
parser_error_pos (LydParser *parser)
{
  return parser->p - parser->buf;
}

static int
lyd_find_wave (Lyd *lyd, const char *name)
{
  int    i;
  for (i = 0; i < LYD_MAX_WAVE; i++)
    {
      LydWave *wave = lyd->wave[i];
      if (wave && !strcmp (wave->name, name))
        return i;
    }

  if (lyd->wave_handler)
    {
      if (lyd->wave_handler (lyd, name, lyd->wave_handler_data))
        printf ("wave loader returned error\n");
      for (i = 0; i < LYD_MAX_WAVE; i++)
        {
          LydWave *wave = lyd->wave[i];
          if (wave && !strcmp (wave->name, name))
            return i;
        }
      fprintf (stderr, "Failed loading wave data \"%s\"\n", name);
    }

  else
    fprintf (stderr, "Failed loading wave data \"%s\" no wave_handler\n", name);
  return 0;
}

static LydToken *
parser_advance (LydParser *parser, const char *expected)
{
  LydToken *newtok;
  LydToken *t;

  if (expected && strcmp (parser->token->str, expected))
    {
      static char msg[256];
      snprintf (msg, 256, "expected %s got %s", expected, parser->token->str);
      parser->error = msg;
      return NULL;
    }
  if (!(t = parser_scanner_next (parser)))
    {
      return parser_lookup(parser, "(end)");
    }

  newtok = g_malloc0 (sizeof (LydToken));
  if (t->type == literal)
    { 
      *newtok = *parser_lookup (parser, "(lit)");
      newtok->str = g_strdup (t->str);
      newtok->value = g_ascii_strtod (t->str, NULL);
    }
  else if (t->type == string)
    { 
      *newtok = *parser_lookup (parser, "(lit)");
      newtok->str = g_strdup (t->str);
      newtok->value = lyd_find_wave (parser->lyd, t->str);
    }
  else 
    {
      LydToken *orig;
      /* check if the token matches one of the primitives */
      if ((orig = parser_lookup (parser, t->str)))
        {
          *newtok = *orig;
          newtok->str = g_strdup (t->str);
        }
      else if (is_constant (t->str)) /* or a constant */
        {
          *newtok = *parser_lookup(parser, "(lit)");
          newtok->str = g_strdup (t->str);
          newtok->value = str2constant (t->str);
        }
      else if (is_op (t->str))
        {
          *newtok = *parser_lookup (parser, "(fun)");
          newtok->str = g_strdup (t->str);
        }
      else
        {
          int j;
          char *value;
          *newtok = *parser_lookup (parser, "(var)");
          newtok->str = g_strdup (t->str);
          value = strchr (newtok->str, '=');
          if (value)
            {
              *value = '\0';
            }
          newtok->value = str2float (newtok->str);
          for (j = 0; j < parser->variables; j++)
            {
              if (parser->variable[j] == newtok->value)
                goto found;
            }
          if (value)
            parser->var_default[parser->variables] = g_ascii_strtod (value + 1, NULL);
          parser->variable[parser->variables++] = newtok->value;
          found:
          ;
        }
    }
  token_free (t);
  parser->token = newtok;
  return newtok;
}

static LydToken *parser_expression (LydParser *parser, int right_binding_power)
{
  LydToken *left;
  LydToken *t = parser->token;
  if ((parser_advance (parser, NULL) == parser_lookup (parser, "(end)")))
    return t;
  left = t->nud (parser, t);
  if (parser->error)
    return NULL;
  while (right_binding_power < parser->token->left_binding_power)
    {
      t = parser->token;
      if (parser_advance (parser, NULL) == parser_lookup(parser, "(end)"))
        {
          parser->error = "unexpected EOD";
          return NULL;
        }
      left = t->led(parser, t, left);
      if (parser->error)
        return NULL;
    }
  if (parser->error)
    return NULL;
  return left;
}

static LydToken *parser_parse (LydParser *this)
{
  parser_advance (this, NULL);
  this->tree = parser_expression (this, 0);
  if (this->error)
    return NULL;
  return this->tree;
}

static LydParser *parser_new (Lyd *lyd, const char *string)
{
  LydParser *this = g_new0 (LydParser, 1);
  this->buf = this->p = string;
  this->lyd = lyd;
  this->error = NULL;
  this->token = NULL;
  return this;
}

static const char *
parser_error (LydParser *parser)
{
  return parser->error;
}


static int tcount (LydParser *parser, LydToken *t, int *cnt)
{
  int rootcnt = 0;
  if (!cnt)
    cnt = &rootcnt;
  switch (t->type)
    {
      int i;
      case binary:
        t->command_no = *cnt;

        (*cnt)++;
        tcount (parser, t->first, cnt);
        tcount (parser, t->second, cnt);
        break;
      case variable:
      case literal:
      case string:
      case function:
        break;
      case args:
        t->command_no = *cnt;

        (*cnt)++;
        for (i=0;i< LYD_MAX_ARGS;i++)
          if (t->args[i])
            tcount (parser, t->args[i], cnt);
        break;
      case unary:
        t->command_no = *cnt;
        (*cnt)++;
        tcount (parser, t->first, cnt);
        break;
      default:
        printf ("%s:%d ` %i %s`", __FILE__, __LINE__, t->type, t->str);
    }
  return *cnt;
}

static void tfree (LydToken *t)
{
  switch (t->type)
    {
      int i;
      case binary:
        tfree (t->first);
        tfree (t->second);
        break;
      case literal:
      case string:
      case variable:
      case function:
        break;
      case args:
        for (i=0;i< LYD_MAX_ARGS;i++)
          if (t->args[i])
            tfree (t->args[i]);
        /* fallthrough */
      case unary:
        tfree (t->first);
        break;
      default:
        printf ("%s:%s!!!%i %s`\n", __FILE__, __FUNCTION__, t->type, t->str);
    }
  token_free (t);
}

static void parser_free (LydParser *parser)
{
  if (parser->tree)
    tfree (parser->tree);
  g_free (parser);
}

/*************/

/* print s-expression */
static void sexp (LydToken *t)
{
  if (!t)
    {
      printf ("sexp passed NULL\n");
      return;
    }
  switch (t->type)
    {
      int i;
      case binary:
        printf ("(%s", t->str); sexp (t->first);sexp (t->second);printf (")");
        break;
      case literal:
        printf (" %2.2f", t->value);
        break;
      case variable:
        printf (" %2.2f", t->value * 100);
        break;
      case args:
        printf ("(%s", t->first->str);
        for (i=0;i< LYD_MAX_ARGS;i++)
          if (t->args[i]) sexp (t->args[i]);
        printf (")");
        break;
      case unary:
        printf ("(%s", t->str); sexp (t->first); printf (")");
        break;
      default:
        printf ("%s:%s!!%i %s\n", __FILE__, __FUNCTION__, t->type, t->str);
    }
}

static void compile (LydParser  *parser,
                     LydToken   *t,
                     LydProgram *program,
                     int         totcmds,
                     int         command,
                     int         argno)
{
  if (!t)
    {
      printf ("assemble passed NULL");
      return;
    }
  switch (t->type)
    {
      int i;
      case binary:
        {
#define POS(t) (totcmds-1-t->command_no)
          if (command != POS(t))
            {
              /* write a pointer to ourselves into the
               * referenced command
               */
              program->commands[command].arg[argno] = POS(t) - command;
            }
          /* set the type of oursevles */
          program->commands[POS(t)].op = str2opcode (t->str);
          compile (parser, t->first, program, totcmds, POS(t), 0);
          compile (parser, t->second, program, totcmds, POS(t), 1);
        }
        break;
      case literal:
        program->commands[command].arg[argno] = t->value;
        break;
      case variable:
        {
          int j;
          for (j = 0; j < parser->variables; j++)
            if (parser->variable[j]==t->value)
              break;
          program->commands[command].arg[argno] = j - command;
        }
        break;
      case args:
        {
          if (command != POS(t))
            {
              /* write a pointer to ourselves into the
               * referenced command
               */
              program->commands[command].arg[argno] = POS(t) - command;
            }

          /* set the type of the function oursevles */
          program->commands[POS(t)].op = str2opcode (t->first->str);
          for (i=0;i< LYD_MAX_ARGS;i++)
            if (t->args[i])
              {
                program->commands[POS(t)].argc++;
                compile (parser, t->args[i], program, totcmds, POS(t), i);
              }
        }
        break;
      case unary: 
        {
#define POS(t) (totcmds-1-t->command_no)
          if (command != POS(t))
            {
              /* write a pointer to ourselves into the
               * referenced command
               */
              program->commands[command].arg[argno] = POS(t) - command;
            }
          /* set the type of oursevles */
          program->commands[POS(t)].op = str2opcode ("neg");
          compile (parser, t->first, program, totcmds, POS(t), 0);
        }
        break;
      default:
        printf ("%s:%s!!!%i %s!!\n", __FILE__, __FUNCTION__, t->type, t->str);
    }
}

static void print_program (LydProgram *program)
{
  int i;
  printf ("LydProgram program = {\"noname\", \n");
  for (i=0;program->commands[i].op;i++)
    {
    printf ("{%i, {%2.2f, %2.15f, %2.2f, %2.2f}}\n",
         program->commands[i].op,
         program->commands[i].arg[0],
         program->commands[i].arg[1],
         program->commands[i].arg[2],
         program->commands[i].arg[3]);
    }
  printf ("},\n");
}

void
lyd_program_free (LydProgram *program)
{
  g_free (program);
}

LydProgram *lyd_compile (Lyd *lyd, const char *source)
{
  LydParser *parser = parser_new (lyd, source);
  LydProgram *program;
  int commands;
  LydToken *t;
  int i;

  t = parser_parse (parser);
  if (!t)
    {
      printf ("%d: %s\n",
               parser_error_pos (parser),
               parser_error (parser)?parser_error(parser):"unknown error");
      parser_free (parser);
        
      return NULL;
    }
  program = g_new0 (LydProgram, 1);
  commands = tcount (parser, parser->tree, NULL);
  compile (parser, parser->tree, program, commands + parser->variables,
           commands+parser->variables-1, 0);
  for (i=0; i<parser->variables; i++)
    {
      program->commands[i].op = str2opcode ("nop");
      program->commands[i].arg[0] = parser->var_default[i];
      program->commands[i].arg[1] = parser->variable[i];
    }

  if (0)
    {
      printf ("%s\n", source);
      sexp (parser->tree);
      printf ("\n");
      print_program (program);
    }
  parser_free (parser);
  return program;
}
