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

static float midi2hz (int midinote)
{
  return (440.0 * pow (2,(midinote-69.0)/12.0));
}

#define NOTE(time, duration, halfnote) do{                         \
  voice = lyd_voice_new (lyd, instrument, time, 0);                \
  lyd_voice_set_param (lyd, voice, "hz",     midi2hz(halfnote+69));\
  lyd_voice_set_duration (lyd, voice, duration);         }while(0);

static int scale[]={0,2,4,7,9,12,9,7,4,2,0};

int main (int    argc,
          char **argv)
{
  Lyd        *lyd = lyd_new ();
  LydVoice   *voice;
  LydProgram *instrument;
  const char *code = "sin(hz=440.0) * adsr(0.5, 0.10, 0.4, 0.30)";
  int         i;

  if (!lyd_audio_init (lyd, "auto"))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      return -1;
    }

  instrument = lyd_compile (lyd, code);
  for (i = 0; i<11;i++)
    NOTE(i*0.5, 0.3, scale[i]);

  lyd_program_free (instrument);

  sleep (5);

  lyd_free (lyd);
  return 0;
}

#undef NOTE
