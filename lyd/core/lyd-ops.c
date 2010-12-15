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

/* this file is included from lyd-vm.c */

static inline void op_adsr (OP_ARGS)
{
  int i;
  ALIGNED_ARGS
  LydSample a = ARG0(0),
            d = ARG0(1),
            s = ARG0(2),
            r = ARG0(3);
  for (i=0; i<samples; i++)
    {
      if (vm->released)
        {
          if (vm->released > r) /* after end of release */
            {
              OUT = 0.0;
            }
          else
            {
              float released_val;
              if ((SAMPLE - vm->released) <= a)     /* release in attack*/
                released_val = ((vm->sample - vm->released) / a) *
                               ((SAMPLE - vm->released) / a);
              else if (SAMPLE - vm->released < (a+d))/*release in decay */
                released_val = 1.0 + (s-1) * (((SAMPLE - vm->released) - a)/ d);
              else                                      /*release in sustain */
                released_val = s;
              OUT = released_val * (1.0 - (vm->released) / r);
            }
        }
      else if (SAMPLE <= a)                        /* in attack */
        OUT = (SAMPLE / a) * (SAMPLE / a);
      else if (SAMPLE < a + d)                     /* in decay */
        OUT = 1.0 + (s-1) * ((SAMPLE - a) / d);
      else                                         /* in sustain */
        OUT = s;
    }
  ALIGNED_ARGS_SILENCE;
}

static inline void op_ddadsr (OP_ARGS)
{
  int i;
  ALIGNED_ARGS
  LydSample delay = ARG0(0),
            duration = ARG0(1),
            a = ARG0(2),
            d = ARG0(3),
            s = ARG0(4),
            r = ARG0(5);
  for (i=0; i<samples; i++)
    {
      int sample = SAMPLE - delay;

      if (sample < 0)
        {
          OUT = 0.0;
        }
      else if (sample > duration)
        {
          int released = sample - duration;
          if (released > r) /* after end of release */
            {
              OUT = 0.0;
            }
          else
            {
              float released_val;
              if ((sample - released) <= a)  /* release in attack*/
                released_val = (((vm->sample - delay)- released) / a) * ((sample - released) / a);
              else if (sample - released < (a+d))/*release in decay */
                released_val = 1.0 + (s-1) * (((sample - released) - a) / d);
              else  /*release in sustain */
                released_val = s;
              OUT = released_val * (1.0 - (released) / r);
            }
        }
      else if (sample <= a)                        /* in attack */
        OUT = (sample / a) * (sample / a);
      else if (sample < a + d)                     /* in decay */
        OUT = 1.0 + (s-1) * ((sample - a) / d);
      else                                         /* in sustain */
        OUT = s;
    }
  ALIGNED_ARGS_SILENCE;
}

#include "biquad.c"

static inline void op_filter_free (LydOpState *state)
{
  free (state->data);
}

static inline void op_filter (OP_ARGS)
{
  int i = 0;
  ALIGNED_ARGS;
  if (G_UNLIKELY (!DATA))
    DATA = BiQuad_new(state->op-LYD_LOW_PASS,/* compute the right biquad-enum */
                      ARG0(0),ARG0(1), vm->sample_rate, ARG0(2));

  /* always updating the filter is expensive, so we do it once per chunk  */
  BiQuad_update (DATA,state->op-LYD_LOW_PASS,/* compute the right biquad-enum */
                 ARG0(0),ARG0(1), vm->sample_rate,ARG0(2));

  for (i=0; i<samples; i++)
    OUT = BiQuad(ARG(3), DATA);

  ALIGNED_ARGS_SILENCE;
}

static inline float noise (void)
{
  static int seed = 1996;
  float rand;
  const int ia = 853, im = 981287;
  seed = (seed*ia)%im;
  rand = ((float) seed - 0.5)/((float) (im - 1));
  return rand;
}

static inline void op_noise (OP_ARGS)
{
  OP_LOOP(OUT = noise();)
}

static inline float input_sample_peek (LydVM *vm,
                                       int    no)
{
  float ret = 0.0;
  if (!vm->input_buf)
    return 0.0;
  if (vm->input_pos[no] < vm->input_buf_len)
    ret = vm->input_buf[no][vm->input_pos[no]];
  return ret;
}

static inline float input_sample (LydVM *vm,
                                  int    no)
{
  float ret = 0.0;
  if (!vm->input_buf)
    return 0.0;
  ret = input_sample_peek (vm, no);
  vm->input_pos[no] ++; /* this should not happen here, this 
                           makes multiple concurrent uses of the
                           same input impossible,
                           all positions should be iterated together.
                         */
  return ret;
}

static inline void op_input (OP_ARGS)
{
  OP_LOOP(OUT = input_sample (vm, ARG0(0));)
}

#define MIDDLE_C 261.625565

static inline float wave_sample (LydVM *vm, LydOpState *state, int no, float hz)
{
  LydWave *wave = vm->lyd->wave[no];
  float old = state->phase;
  float delta = vm->i_sample_rate * (hz>0.001?hz/MIDDLE_C:1.0);
  float new = old + delta;
  int sample_pos = new * wave->sample_rate;
  state->phase = new;
  if (sample_pos < wave->samples)
    return wave->data[sample_pos];
  return 0.0;
}

static inline float wave_sample_loop  (LydVM *vm, LydOpState *state, int no, float hz)
{
  LydWave *wave = vm->lyd->wave[no];
  float old = state->phase;
  float delta = vm->i_sample_rate * (hz>0.001?hz/MIDDLE_C:1.0);
  float new = old + delta;
  int sample_pos = new * wave->sample_rate;
  state->phase = new;
  if (sample_pos < wave->samples)
    return wave->data[sample_pos];
  state->phase = 0.0;
  return 0.0;
}

static inline void op_wave (OP_ARGS)
{
  OP_LOOP(OUT = wave_sample (vm, state, ARG0(0), ARG0(1));)
}

static inline void op_wave_loop (OP_ARGS)
{
  OP_LOOP(OUT = wave_sample_loop (vm, state, ARG0(0), ARG0(1));)
}

static inline void op_mix (OP_ARGS)
{
  int i;
  ALIGNED_ARGS;
  switch (state->argc)
    {
      case 0: for (i = 0; i < samples; i++)
         OUT = 0; break;
      case 1: for (i = 0; i < samples; i++)
         OUT = ARG(0); break;
      case 2: for (i = 0; i < samples; i++)
         OUT = (ARG(0) + ARG(1))/2; break;
      case 3: for (i = 0; i < samples; i++)
         OUT = (ARG(0) + ARG(1) + ARG(2))/3; break;
      case 4: for (i = 0; i < samples; i++)
         OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3))/4; break;
      case 5: for (i = 0; i < samples; i++)
         OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3) +
                ARG(4))/5; break;
      case 6: for (i = 0; i < samples; i++)
         OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3) +
                ARG(4) + ARG(5))/6; break;
      case 7: for (i = 0; i < samples; i++)
         OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3) +
                ARG(4) + ARG(5) + ARG(6))/7; break;
      case 8: for (i = 0; i < samples; i++)
         OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3) +
                ARG(4) + ARG(5) + ARG(6) + ARG(7))/8; break;
    }
  ALIGNED_ARGS_SILENCE;
}

typedef struct _ReverbData
{
   int    pos;
   int    size;
   LydSample *old;
} ReverbData;

static inline void op_reverb (OP_ARGS)
{
  ReverbData *data   = state->data;
  int i;
  ALIGNED_ARGS;
  for (i=0; i<samples; i++)
    {
      LydSample   reverb = ARG(0),
                  length = ARG(1),
                  sample = ARG(2);
      int         size   = length * vm->sample_rate;

      if (size <=0)
        return;

      if (G_UNLIKELY (size > LYD_MAX_REVERB_SIZE))
        size = LYD_MAX_REVERB_SIZE;

      if (G_UNLIKELY (data == NULL ||
          size != data->size))
        {
          data = state->data = g_malloc0 (sizeof (LydSample) *size + sizeof(ReverbData));
          data->size = size;
          data->old = (void*)((  ((char*)(data)) + sizeof (ReverbData)));
        }

      sample = sample + data->old[data->pos] * reverb;
      data->old[data->pos++] = sample / (1.0 + reverb);
      if (G_UNLIKELY (data->pos >= size))
        data->pos = 0;
      OUT = sample;
    }
  ALIGNED_ARGS_SILENCE;
}

static inline void op_reverb_free (LydOpState *state)
{
  g_free (state->data);
}


typedef struct _DelayData
{
   int    pos;
   int    size;
   LydSample *old;
} DelayData;

static inline void op_delay (OP_ARGS)
{
  /* XXX: allow dynamically changing the length */
  DelayData *data   = state->data;
  int i;
  ALIGNED_ARGS;
  for (i=0; i<samples; i++)
    {
      LydSample length = ARG(0),
                sample = ARG(1);
      int       size   = length * vm->sample_rate;

      if (size <=0)
        return;

      if (G_UNLIKELY (size > LYD_MAX_REVERB_SIZE))
        size = LYD_MAX_REVERB_SIZE;

      if (G_UNLIKELY (data == NULL ||
          size != data->size))
        {
          data = state->data = g_malloc0 (sizeof (LydSample) *size + sizeof(DelayData));
          data->size = size;
          data->old = (void*)((  ((char*)(data)) + sizeof (DelayData)));
        }

      OUT = data->old[data->pos];
      data->old[data->pos++] = sample;
      if (G_UNLIKELY (data->pos >= size))
        data->pos = 0;
    }
  ALIGNED_ARGS_SILENCE;
}

static inline void op_delay_free (LydOpState *state)
{
  g_free (state->data);
}


static inline void op_cycle (OP_ARGS)
{
  int i, pos, count;
  ALIGNED_ARGS
  LydSample freq = ARG0(0);

  if (state->argc < 2)
    {
      for (i = 0; i < samples; i++)
        OUT = 0;
      return;
    }

  count = state->argc - 1;

  for (i = 0; i < samples; i++)
    {
      pos = fmod (freq * count * SAMPLE / vm->sample_rate, count);

      switch (1 + (pos+count) % count)
        {
          case 1: OUT = ARG(1); break;
          case 2: OUT = ARG(2); break;
          case 3: OUT = ARG(3); break;
          case 4: OUT = ARG(4); break;
          case 5: OUT = ARG(5); break;
          case 6: OUT = ARG(6); break;
          case 7: OUT = ARG(7); break;
        }
    }
  ALIGNED_ARGS_SILENCE;
}
