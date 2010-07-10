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

#ifndef __LYD_H_
#define __LYD_H_

typedef struct _LydVoice   LydVoice;
typedef struct _Lyd        Lyd;
typedef struct _LydProgram LydProgram;

typedef enum {
  LYD_f32,  /* 32bit floating point mono */
  LYD_f32S, /* 32bit floating point stereo, stream2 is used */
  LYD_s16S  /* 16bit signed integer stereo, interleaved on stream1*/
} LydFormat;

typedef enum {
  LYD_GAP,    /* all values in transition are 0.0 */
  LYD_STEP,   /* all values before value have previous value */
  LYD_LINEAR, /* slide linearly between values */
  LYD_CUBIC,  /* slide smoothly between values */
} LydInterpolation;

Lyd        *lyd_new             (void);

void        lyd_set_sample_rate (Lyd *lyd, int sample_rate);

void        lyd_set_format      (Lyd *lyd, LydFormat format);

void        lyd_free            (Lyd *lyd);

long        lyd_synthesize      (Lyd *lyd, int len, void *stream,void *stream2);

LydProgram *lyd_compile         (Lyd *lyd, const char *source);

/* should perhaps be lyd_voice_new */
LydVoice   *lyd_new_voice       (Lyd *lyd, LydProgram *program, int tag);

void        lyd_kill            (Lyd *lyd, int tag);

void        lyd_program_free    (LydProgram *program);

void        lyd_voice_release   (Lyd *lyd, LydVoice *voice);

void        lyd_voice_kill      (Lyd *lyd, LydVoice *voice);

void        lyd_voice_set_delay (Lyd *lyd, LydVoice *voice, double seconds);

/* the voice will be automatically released after it has been playing
 * for the specified duration (in seconds).
 */
void        lyd_voice_set_duration (Lyd *lyd, LydVoice *voice, double duration);

void        lyd_voice_set_param (Lyd        *lyd,   LydVoice *voice,
                                 const char *param, double    value);

void        lyd_voice_set_param_delayed (Lyd        *lyd,   LydVoice  *voice,
                                         const char *param, float      time,
                                         LydInterpolation interpolation, 
                                         float       value); 
/* NYI */
void        lyd_wav_from_data   (Lyd *lyd,         const char *name,
                                 int  length,      LydFormat   format,
                                 int  sample_rate, void       *buf);

void        lyd_voice_set_position (Lyd         *lyd,
                                    LydVoice    *voice,
                                    double       position);
#endif
