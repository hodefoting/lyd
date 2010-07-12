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

int main (int    argc,
          char **argv)
{
  Lyd        *lyd;
  LydVoice   *voice;
  LydProgram *instrument;
  const char *code = "reverb (0.2, 0.123, low_pass (1.3, hz=440.0, 0.03, pulse(200 + sin(190) * 1.4, sin(0.1)) * adsr(0.12, 0.12, 0.7, 0.30) * volume=1.0))";

  lyd = lyd_new ();

  if (!lyd_audio_init (lyd, "auto"))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      return -1;
    }

  instrument = lyd_compile (lyd, code);

#define NOTE(time, duration, frequency) \
  voice = lyd_new_voice (lyd, instrument, 0); \
  lyd_voice_set_param (lyd, voice, "volume", 0.8);\
  lyd_voice_set_param (lyd, voice, "hz",     frequency);\
  lyd_voice_set_duration (lyd, voice, duration);\
  lyd_voice_set_delay (lyd, voice, time);

  NOTE(0.0, 0.3, 440.0);
  NOTE(0.1, 0.2, 660.0);
  NOTE(0.2, 0.1, 880.0);
  NOTE(0.3, 0.1, 440.0);
  NOTE(0.4, 0.1, 880.0);
  NOTE(0.5, 0.1, 440.0);
  NOTE(1.0, 0.15, 440.0);
  NOTE(1.2, 0.15, 500.0);
  NOTE(1.4, 0.15, 600.0);
  NOTE(1.6, 0.15, 700.0);
  NOTE(1.8, 0.15, 800.0);
#undef NOTE
  lyd_program_free (instrument);
  sleep (5);

  lyd_free (lyd);
  return 0;
}
