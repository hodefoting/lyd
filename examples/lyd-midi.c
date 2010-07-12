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

#include <lyd/lyd.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <glib.h>

int main (int    argc,
          char **argv)
{
  Lyd    *lyd;
  gsize   length;
  char   *mididata;
  GError *error = NULL;

  g_thread_init (NULL );
  lyd = lyd_new ();

  if (!argv[1])
    {
      g_print ("\nUsage: %s <file.mid>\n\n", argv[0]);
      return 0;
    }

  printf ("initing sound\n");
  if (!lyd_audio_init (lyd, "auto"))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      return -1;
    }
  if (!g_file_get_contents (argv[1], &mididata, &length, &error))
    {
      printf ("Failed to read midi data from %s\n", argv[1]);
      return -1;
    }
  else
    {
      printf ("Loaded %i bytes midi data from %s\n", length, argv[1]);
    }
  lyd_midi_load (lyd, mididata, length);
  lyd_midi_play (lyd);
  // lyd_midi_set_repeat (Lyd *lyd, float start, float end)
  /*
  sleep (4);
  lyd_midi_pause (lyd);
  sleep (2);
  lyd_midi_play (lyd);
  sleep (2);
  lyd_free (lyd);
  */
  g_main_loop_run (g_main_loop_new (NULL, FALSE));

  return 0;
}

#undef NOTE
