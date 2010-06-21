#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "lyd.h"
#include "lyd-private.h"

/**** XXX: leaking, what seems to be ) tokens, since they do not become
 * part of the tree
 */

static float str2float (const char *str);

typedef struct _StrConstant {
  char *str;
  float  value;
} StrConstant;

static StrConstant constant_lexicon[] = {
  {"pi",     3.141592653589793},
  {"phi",    1.61803399},
  {"iphi",   0.61803399},
};

typedef struct _StrOp {
  char *str;
  LydOp  op;
} StrOp;

static StrOp op_lexicon[] = {
  {"none",       LYD_NONE},
  {"nop",        LYD_NOP},
  {"+",          LYD_ADD},
  {"-",          LYD_SUB},
  {"*",          LYD_MUL},
  {"/",          LYD_DIV},
  {"%",          LYD_MOD},
  {"abs",        LYD_ABS},
  {"pow",        LYD_POW},
  {"sqrt",       LYD_SQRT},
  {"neg",        LYD_NEG},
  {"mix",        LYD_MIX},
  {"mix3",       LYD_MIX3},
  {"mix4",       LYD_MIX4},

  {"sin",        LYD_SIN},
  {"saw",        LYD_SAW},
  {"ramp",       LYD_RAMP},
  {"square",     LYD_SQUARE},
  {"pulse",      LYD_PULSE},
  {"noise",      LYD_NOISE},

  {"adsr",       LYD_ADSR},
  {"reverb",     LYD_REVERB},

  {"low_pass",   LYD_LOW_PASS},
  {"high_pass",  LYD_HIGH_PASS},
  {"band_pass",  LYD_BAND_PASS},
  {"notch",      LYD_NOTCH},
  {"peak_eq",    LYD_PEAK_EQ},
  {"low_shelf",  LYD_LOW_SHELF},
  {"high_shelf", LYD_HIGH_SELF}
};

typedef enum {
  name,
  literal,
  operator,
  function,
  variable,
  binary,
  unary,
  args,
} Type;

typedef struct _Token Token;
typedef struct _Parser Parser;

struct _Token { /* Token  is used both to describe the grammar and */
  char  *str;    /* to build the syntax tree */
  Type   type;
  int    left_binding_power;
  Token *(*nud) (Parser *parser, Token *this);
  int    right_binding_power;
  Token *(*led) (Parser *parser, Token *this, Token *left);
  /* the following could be in a union to save 3 * 32 bit/token|node */
  float  value; /* this token interpreted as a floating point number
                 * (for literals)
                 */
  int    command_no;
  Token *first;
  Token *second;
  Token *args[4]; /* 4 arguments seems to be sufficient for now */
};

struct _Parser  {
  Token      *token;
  const char *buf;
  const char *p;
  char       *error;
  int         variables;
  float       variable[8]; /* max 8 variables for now */
  float       var_default[8];
  Token      *tree;
};

/* forward declarations */
static Token *nud_default (Parser *parser, Token *this);
static Token *nud_itself  (Parser *parser, Token *this);
static Token *nud_prefix  (Parser *parser, Token *this);
static Token *nud_lparen  (Parser *parser, Token *this);
static Token *led_default (Parser *parser, Token *this, Token *left);
static Token *led_infix   (Parser *parser, Token *this, Token *left);
static Token *led_lparen  (Parser *parser, Token *this, Token *left);

static Token symbols[]= {
  {"(end)",      operator, 0,  nud_default, 0,  led_default},
  {"(function)", function, 10, nud_itself,  0,  led_default},
  {"(variable)", variable, 0,  nud_itself,  0,  led_default},
  {"(literal)",  literal,  0,  nud_itself,  0,  led_default},

  {";",          operator, 0,  nud_default, 0, led_default},
  {",",          operator, 0,  nud_default, 0, led_default},
  {")",          operator, 0,  nud_default, 0, led_default},

  {"-",          operator, 50, nud_prefix,  50, led_infix},
  {"+",          operator, 50, nud_default, 50, led_infix},
  {"*",          operator, 60, nud_default, 60, led_infix},
  {"/",          operator, 60, nud_default, 60, led_infix},
  {"(",          operator, 80, nud_lparen,  0,  led_lparen},

};

/* forward declarations */
static Token * parser_expression   (Parser *parser, int right_binding_power);
static Token * parser_advance      (Parser *parser, const char *str);
static void token_free (Token *token);

static Token *nud_default (Parser *parser, Token *this)
{
  parser->error = "parse error1";
  return NULL;
}

static Token *nud_itself (Parser *parser, Token *this)
{
  return this;
}

static Token *led_default (Parser *parser, Token *this, Token *left)
{
  parser->error = "parse error2";
  return NULL;
}

static Token *led_infix (Parser *parser, Token *this, Token *left)
{
  this->first = left;
  this->second = parser_expression(parser, this->right_binding_power);
  this->type = binary;
  return this;
}

static Token *led_lparen (Parser *parser, Token *this, Token *left)
{
  this->type = args;
  this->first = left;
  if (left->type != function)
    parser->error = "lparen parse error";
  if (parser->error)
    return NULL;
  if (strcmp (parser->token->str, ")"))
    {
      int i;
      for (i=0; i<4; i++)
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

static Token *nud_lparen (Parser *parser, Token *this)
{
  Token *e = parser_expression(parser, 0);
  parser_advance (parser, ")");
  token_free (this);
  return e;
}

static Token *nud_prefix (Parser *parser, Token *this)
{
  this->first = parser_expression (parser, 70);
  this->type = unary;
  return this;
}

#define N_ELEMENTS(a)  (sizeof(a)/sizeof(a[0]))

static Token *parser_lookup (Parser *parser,
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



static LydOp str2op (const char *str)
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
#define is_op(str)       (str2op (str)!=0)
#define is_constant(str) (str2constant (str)!=0.0)

static int oneof (char needle, char *haystack)
{
  char *p;
  for (p=haystack; *p; p++)
    if (needle == *p)
      return 1;
  return 0;
}

static Token *parser_scanner_next (Parser *parser)
{
  char word[40]="";
  int   wpos=0;
  int   incomment=0;
  char *whitespace = "\n ";
  char *numerals   = "0123456789.";
  char *operators  = "+-/*(),";

  Token *tok = g_new0 (Token, 1);
  
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

  if (oneof (*parser->p, numerals))
    {
      while (*parser->p && oneof (*parser->p, numerals) && wpos<40)
        word[wpos++]=*parser->p++;
      tok->type = literal;
    }
  else if (oneof (*parser->p, operators))
    word[wpos++]=*parser->p++;
  else
    {
      while (*parser->p
             && !oneof (*parser->p, whitespace)
             && !oneof (*parser->p, operators)
             && wpos<40)
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

static void token_free (Token *token)
{
  if (token->str)
    g_free (token->str);
  g_free (token);
}

static int
parser_error_pos (Parser *parser)
{
  return parser->p - parser->buf;
}

static Token *
parser_advance (Parser     *parser,
                const char *expected)
{
  Token *newtok;
  Token *t;

  if (expected && strcmp (parser->token->str, expected))
    {
      char msg[256];
      snprintf (msg, 256, "expected %s got %s", expected, parser->token->str);
      parser->error = strdup(msg);
      return NULL;
    }
  if (!(t = parser_scanner_next (parser)))
    {
      return parser_lookup(parser, "(end)");
    }

  newtok = g_malloc0(sizeof(Token));
  if (t->type == literal)
    { 
      *newtok = *parser_lookup(parser, "(literal)");
      newtok->str = g_strdup (t->str);
      newtok->value = g_ascii_strtod (t->str, NULL);
    }
  else 
    {
      Token *orig;
      /* check if the token matches one of the primitives */
      if ((orig = parser_lookup (parser, t->str)))
        {
          *newtok = *orig;
          newtok->str = g_strdup (t->str);
        }
      else if (is_constant (t->str)) /* or a constant */
        {
          *newtok = *parser_lookup(parser, "(literal)");
          newtok->str = g_strdup (t->str);
          newtok->value = str2constant (t->str);
        }
      else if (is_op (t->str))
        {
          *newtok = *parser_lookup (parser, "(function)");
          newtok->str = g_strdup (t->str);
        }
      else
        {
          int j;
          char *value;
          *newtok = *parser_lookup (parser, "(variable)");
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

static Token *parser_expression (Parser *parser, int right_binding_power)
{
  Token *left;
  Token *t = parser->token;
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
          parser->error = "unexpected end of input";
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

static Token *parser_parse (Parser *this)
{
  parser_advance (this, NULL);
  this->tree = parser_expression (this, 0);
  if (this->error)
    return NULL;
  return this->tree;
}

static Parser *parser_new (const char *string)
{
  Parser *this = g_new0 (Parser, 1);
  this->buf = this->p = string;
  this->error = NULL;
  this->token = NULL;
  return this;
}

static const char *
parser_error (Parser *parser)
{
  return parser->error;
}


static int tcount (Parser *parser, Token *t, int *cnt)
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
      case function:
        break;
      case args:
        t->command_no = *cnt;

        (*cnt)++;
        for (i=0;i<4;i++)
          if (t->args[i])
            tcount (parser, t->args[i], cnt);
        break;
      case unary:
        t->command_no = *cnt;
        (*cnt)++;
        tcount (parser, t->first, cnt);
        break;
      default:
        printf ("`unhandled tcount %i %s`", t->type, t->str);
    }
  return *cnt;
}

static void tfree (Token *t)
{
  switch (t->type)
    {
      int i;
      case binary:
        tfree (t->first);
        tfree (t->second);
        break;
      case literal:
      case variable:
      case function:
        break;
      case args:
        for (i=0;i<4;i++)
          if (t->args[i])
            tfree (t->args[i]);
        /* fallthrough */
      case unary:
        tfree (t->first);
        break;
      default:
        printf ("%s!!!!%i %s`", __FUNCTION__, t->type, t->str);
    }
  token_free (t);
}

static void parser_free (Parser *parser)
{
#if 0
  if (parser->error)
    g_free (parser->error);
#endif
  if (parser->tree)
    tfree (parser->tree);
  g_free (parser);
}

/*************/

/* print s-expression */
static void sexp (Token *t)
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
        for (i=0;i<4;i++)
          if (t->args[i]) sexp (t->args[i]);
        printf (")");
        break;
      case unary:
        printf ("(%s", t->str); sexp (t->first); printf (")");
        break;
      default:
        printf ("%s!!!!!%i %s!!!!!", __FUNCTION__, t->type, t->str);
    }
}

static void compile (Parser     *parser,
                     Token      *t,
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
          program->commands[POS(t)].op = str2op (t->str);
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
          program->commands[POS(t)].op = str2op (t->first->str);
          for (i=0;i<4;i++)
            if (t->args[i])
              compile (parser, t->args[i], program, totcmds, POS(t), i);
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
          program->commands[POS(t)].op = str2op ("neg");
          compile (parser, t->first, program, totcmds, POS(t), 0);
        }
        break;
      default:
        printf ("%s!!!!!%i %s!!!!!", __FUNCTION__, t->type, t->str);
    }
}


static void print_program (LydProgram *program)
{
  int i;
  printf ("LydProgram program = {\"noname\", \n");
  for (i=0;program->commands[i].op;i++)
    {
    printf ("{%i, {%2.2f, %2.2f, %2.2f, %2.2f}}\n",
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
  Parser *parser = parser_new (source);
  LydProgram *program;
  int commands;
  Token *t;
  int i;

  t = parser_parse (parser);
  if (!t)
    {
      printf ("%d: %s\n",
               parser_error_pos (parser),
               parser_error (parser));
      parser_free (parser);
        
      return NULL;
    }
  program = g_new0 (LydProgram, 1);
  commands = tcount (parser, parser->tree, NULL);
  compile (parser, parser->tree, program, commands + parser->variables,
           commands+parser->variables-1, 0);
  for (i=0; i<parser->variables; i++)
    {
      program->commands[i].op = str2op ("nop");
      program->commands[i].arg[0] = parser->var_default[i];
      program->commands[i].arg[1] = parser->variable[i];
    }

  if (1)
    {
      printf ("%s\n", source);
      sexp (parser->tree);
      print_program (program);
    }
  parser_free (parser);
  return program;
}
