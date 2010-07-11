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

#include <glib.h>
#include <lyd/lyd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


static void test_lyd (Lyd *lyd);

gboolean lyd_audio_init (Lyd         *lyd,
                         const gchar *driver);

int main (int    argc,
          char **argv)
{
  Lyd *lyd;
  g_thread_init (NULL);
  lyd = lyd_new ();

  if (!lyd_audio_init (lyd, "auto"))
    {
      g_free (lyd);
      g_error ("failed to initialize lyd (audio output)\n");
    }

  test_lyd (lyd);
  sleep (6);

  lyd_free (lyd);
  return 0;
}

static void test_lyd (Lyd *lyd)
{ 
  LydVoice   *voice;
  LydProgram *instrument;
  instrument = lyd_compile (lyd, "(sin(hz=440 + saw(20)*10)) * volume=1");

  voice = lyd_new_voice (lyd, instrument, 0);
  lyd_voice_set_param (lyd, voice, "volume", 1.2);
  lyd_voice_set_duration (lyd, voice, 10.0);

  lyd_voice_set_param_delayed (lyd, voice, "volume", 5.0, LYD_CUBIC, 1.0);
  lyd_voice_set_param_delayed (lyd, voice, "volume", 6.0, LYD_LINEAR, 0.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 0.0, LYD_CUBIC, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 0.2, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 0.4, LYD_CUBIC, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 0.6, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 0.8, LYD_CUBIC, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 1.0, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 2.0, LYD_GAP, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 3.0, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 4.0, LYD_CUBIC, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 5.0, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 6.0, LYD_CUBIC, 440.0);

  lyd_program_free (instrument);
}


void welcome2 (Lyd *lyd)
{ 
}

