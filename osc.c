#include "lyd.h"
#ifdef HAVE_OSC
#include <lo/lo.h>
#include <stdlib.h>
#include <glib.h>

static LydProgram *programs[256]={NULL,};
static LydVoice   *voices[1024]={NULL,};

static void
osc_error (int         num,
           const char *msg,
           const char *path)
{
  g_print ("liblo server error %d in path %s: %s\n", num, path, msg);
}

#define OSC_ARGS const char  *path,\
         const char  *types,\
         lo_arg     **argv,\
         int          argc,\
         void        *data,\
         void        *lyd

static int osc_log (OSC_ARGS)
{
    int i;

    g_print ("OSC %s ", path);
    for (i=0; i<argc; i++)
      {
	g_print (" ");
	lo_arg_pp(types[i], argv[i]);
      }
    g_print("\n");
    return 1; /* message not handled */
}

static int osc_compile (OSC_ARGS)
{
  LydProgram *program;
  int codeslot = argv[0]->i;
  const char *code = &argv[1]->s;
  g_print ("compile %d %s\n", codeslot, code);

  program = lyd_compile (lyd, code);
  if (!program)
    return 0;
  if (programs[codeslot])
    {
      lyd_program_free (programs[codeslot]);
    }
  programs[codeslot] = program;
  return 0;
}

static int osc_invoke (OSC_ARGS)
{
  int slot = argv[0]->i;
  int codeslot = argv[1]->i;
  if (!programs[codeslot])
    return 0;
  g_print ("invoke: %d %d\n", slot, codeslot);
  voices[slot] = lyd_new_voice (lyd, programs[codeslot], 0);
  return 0;
}

static int osc_run (OSC_ARGS)
{
  LydProgram *program;
  int slot = argv[0]->i;
  const char *code = &argv[1]->s;
  g_print ("run %d %s\n", slot, code);

  program = lyd_compile (lyd, code);
  if (!program)
    return 0;
  voices[slot] = lyd_new_voice (lyd, program, 0);

  lyd_program_free (program);
  return 0;
}

static int osc_release (OSC_ARGS)
{
  int slot = argv[0]->i;
  if (voices[slot])
    {
      lyd_voice_release (lyd, voices[slot]);
    }
  return 0;
}

static int osc_kill (OSC_ARGS)
{
  int slot = argv[0]->i;
  if (voices[slot])
    {
      lyd_voice_kill (lyd, voices[slot]);
      voices[slot]=NULL; 
    }
  return 0;
}

static int osc_patch (OSC_ARGS)
{
  return 0;
}

static int osc_program_set (OSC_ARGS)
{
  return 0;
}

static int osc_voice_set (OSC_ARGS)
{
  int slot = argv[0]->i;
  const char *param = &argv[1]->s;
  float value = argv[2]->f;
  if (voices[slot])
    lyd_voice_set_param (lyd, voices[slot], param, value);
  return 0;
}

/*
 
compile  <codeslot> "code"
pset     <codeslot> <param> <value>

invoke   <slot> <codeslot>
release  <slot>
kill     <slot> 

run      <slot> "code"
patch    <slot> "code"
set      <slot> <param> <value>

*/

void lyd_osc_init (Lyd *lyd)
{
  lo_server_thread st = lo_server_thread_new ("6150", osc_error);
  lo_server_thread_add_method (st, "/compile", "is", osc_compile, lyd);
  lo_server_thread_add_method (st, "/invoke", "ii", osc_invoke, lyd);
  lo_server_thread_add_method (st, "/run", "is", osc_run, lyd);
  lo_server_thread_add_method (st, "/release", "i", osc_release, lyd);
  lo_server_thread_add_method (st, "/kill", "i", osc_kill, lyd);
  lo_server_thread_add_method (st, "/patch", "is", osc_patch, lyd);
  lo_server_thread_add_method (st, "/pset", "isf", osc_program_set, lyd);
  lo_server_thread_add_method (st, "/set", "isf", osc_voice_set, lyd);
  lo_server_thread_add_method (st, NULL, NULL, osc_log, NULL);
  lo_server_thread_start (st);
}
#endif
