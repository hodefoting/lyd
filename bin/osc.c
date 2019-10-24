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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_OSC
#ifdef HAVE_OSC
#include <lyd/lyd.h>
#include <lo/lo.h>
#include <stdlib.h>
#include <stdio.h>

static LydVoice   *voices[1024]={NULL,};

static void
osc_error (int         num,
           const char *msg,
           const char *path)
{
  printf ("liblo server error %d in path %s: %s\n", num, path, msg);
}

#define OSC_ARGS const char  *path, const char  *types, lo_arg     **argv,\
                 int          argc, void        *data,  void        *lyd

static int osc_log (OSC_ARGS)
{
    int i;

    printf ("OSC %s ", path);
    for (i=0; i<argc; i++)
      {
	printf (" ");
	lo_arg_pp(types[i], argv[i]);
      }
    printf("\n");
    return 1; /* message not handled */
}

static int osc_compile (OSC_ARGS)
{
  int codeslot = argv[0]->i;
  const char *code = &argv[1]->s;
  printf ("compile %d %s\n", codeslot, code);

  lyd_set_patch (lyd, codeslot, code);
  return 0;
}

static int osc_invoke (OSC_ARGS)
{
  int slot = argv[0]->i;
  int codeslot = argv[1]->i;
  printf ("invoke: %d %d\n", slot, codeslot);
  voices[slot] = lyd_note (lyd, codeslot, 61, 1.0, 100.0);
  return 0;
}

static int osc_run (OSC_ARGS)
{
  LydProgram *program;
  int slot = argv[0]->i;
  const char *code = &argv[1]->s;
  printf ("run %d %s\n", slot, code);

  program = lyd_compile (lyd, code);
  if (!program)
    return 0;
  voices[slot] = lyd_voice_new (lyd, program, 0, 0);

  lyd_program_free (program);
  return 0;
}

static int osc_release (OSC_ARGS)
{
  int slot = argv[0]->i;
  if (voices[slot])
    {
      lyd_voice_release (voices[slot]);
    }
  return 0;
}

static int osc_kill (OSC_ARGS)
{
  int slot = argv[0]->i;
  if (voices[slot])
    {
      lyd_voice_kill (voices[slot]);
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
    lyd_voice_set_param (voices[slot], param, value);
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
#endif
