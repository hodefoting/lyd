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

#define HAVE_GLIB
#ifdef HAVE_GLIB
#include <glib.h>
#endif
#include <lyd/lyd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

  welcome (lyd);

#ifdef HAVE_GLIB
  g_main_loop_run (g_main_loop_new (NULL, FALSE));
#else
  sleep(10);
#endif
  return 0;
}