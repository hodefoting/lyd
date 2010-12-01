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
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "general-midi.inc"

const char * lyd_get_patch (Lyd *lyd,
                            int no)
{
  return midi_patches[no];
}

void lyd_set_patch (Lyd *lyd,
                    int no,
                    const char *patch)
{
  midi_patches[no] = strdup (patch); /* XXX: leaking */
}

LydVM *lyd_note_full (Lyd *lyd,
                         int patch,
                         float hz,
                         float volume,
                         float duration,
                         float pan,
                         int hashkey)
{
  LydProgram *program = lyd_compile (lyd, lyd_get_patch (lyd, patch));
  LydVM *voice;
  voice = lyd_voice_new (lyd, program, hashkey);

  lyd_voice_set_param (lyd, voice, "volume", volume);
  lyd_voice_set_param (lyd, voice, "hz", hz);
  lyd_voice_set_duration (lyd, voice, duration);
  lyd_voice_set_position (lyd, voice, pan);
  return voice;
}

LydVM *lyd_note (Lyd *lyd,
                    int patch,
                    float hz,
                    float volume,
                    float duration)
{
  return lyd_note_full (lyd, patch, hz, volume, duration, 0.0, 0);
}
