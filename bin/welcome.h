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

void welcome (Lyd *lyd)
{ 
  LydVoice   *voice;
  LydProgram *program;
  program = lyd_compile (lyd, "echo (0.1, 0.223, low_pass (0.1, hz=440.0, 0.01, pulse(200 + sin(190) * 1.4, square(8.0)) * adsr(0.12, 0.12, 0.8, 0.30))) * volume=1.0 * 0.1");

#define Q(delay, duration, frequency, pos) \
  voice = lyd_voice_new (lyd, program, delay, 0);\
  lyd_voice_set_param (voice, "volume", 1.0);\
  lyd_voice_set_param (voice, "hz",     frequency);\
  lyd_voice_set_duration (voice, duration);\
  lyd_voice_set_position (voice, pos);
  Q(0.0, 0.3, 440.0, 0.0);
  Q(0.1, 0.2, 660.0, -1.0);
  Q(0.2, 0.2, 880.0, 1.0);
  Q(0.3, 0.3, 440.0, 0.0);
  Q(0.4, 0.4, 880.0, -1.0);
  Q(0.5, 0.3, 440.0, 1.0);
  Q(1.0, 0.15, 440.0, -0.8);
  Q(1.2, 0.15, 500.0, -0.2);
  Q(1.4, 0.15, 600.0,  0. );
  Q(1.6, 0.15, 700.0, 0.2);
  Q(1.8, 0.15, 800.0, 0.8);
#undef Q
  lyd_program_free (program);
}
