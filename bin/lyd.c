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

#ifdef HAVE_GLIB
#include <glib.h>
#endif
#include <lyd/lyd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "welcome.h"

int lyd_audio_init   (Lyd        *lyd,
                      const char *driver);

#ifdef HAVE_OSC
void lyd_osc_init   (Lyd *lyd);
#endif

#ifdef HAVE_ALSA
void lyd_midi_init   (Lyd *lyd);
#endif

int main (int    argc,
          char **argv)
{
  Lyd *lyd;
#ifdef HAVE_GLIB
  g_thread_init (NULL);
#endif
  lyd = lyd_new ();

  if (!lyd_audio_init (lyd, "auto"))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      exit (1);
    }

#ifdef HAVE_OSC
  lyd_osc_init (lyd);
#endif
#ifdef HAVE_ALSA
  lyd_midi_init (lyd);
#endif

  if(argv[1])
    {
       long    length;
       char   *mididata = NULL;
       FILE   *file;

       file = fopen (argv[1], "r");
       if (file)
         {
           fseek (file, 0, SEEK_END);
           length = ftell (file);
           fseek (file, 0, SEEK_SET);
           mididata = malloc (length);
         }
       if (!file || fread (mididata, length, 1, file) != 1)
         {
           printf ("Failed to read midi data from %s\n", argv[1]);
           return -1;
         }
       else
         {
           printf ("Loaded %li bytes midi data from %s\n", length, argv[1]);
           fclose (file);
         }

       lyd_midi_load (lyd, (void *)mididata, length);
       lyd_midi_set_playing (lyd, 1);
       free (mididata);
    }
  else
    {
      welcome (lyd);
    }

  for (;;)
    sleep (1);
  return 0;
}
