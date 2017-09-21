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

/* macro to point into an allocation, after a header struct, used to
 * implement extensions for ops as single allocations including
 * data buffers at the end.
 */
#define after_ptr(ptr, type) (void*)((((char*)(ptr)) + sizeof (type)))

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

/**********************************************************************/

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

/**********************************************************************/

#include "biquad.c"

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

static inline void op_filter_free (LydOpState *state)
{
  free (state->data);
}

/**********************************************************************/

static inline float noise (void)
{
  /* not thread safe, but we do not care the results are random enough */
  static int seed = 1996;
  float rand;
  const int ia = 853, im = 981287;
  seed = (seed*ia)%im;
  rand = ((float) seed)/((float) (im - 1)) - 0.5;
  return rand;
}

static inline void op_noise (OP_ARGS)
{
  OP_LOOP(OUT = noise();)
}

/**********************************************************************/

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

static inline void op_inputp (OP_ARGS)
{
  OP_LOOP(OUT = input_sample_peek (vm, ARG0(0));)
}

static inline void op_input (OP_ARGS)
{
  OP_LOOP(OUT = input_sample (vm, ARG0(0));)
}

/**********************************************************************/

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

static inline void op_wave (OP_ARGS)
{
  OP_LOOP(OUT = wave_sample (vm, state, ARG0(0), ARG0(1));)
}

/**********************************************************************/

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

static inline void op_wave_loop (OP_ARGS)
{
  OP_LOOP(OUT = wave_sample_loop (vm, state, ARG0(0), ARG0(1));)
}

/**********************************************************************/

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


/**********************************************************************/

typedef struct _EchoData
{
   int    pos;
   int    size;
   LydSample *old;
} EchoData;

static inline void op_echo (OP_ARGS)
{
  EchoData *data   = state->data;
  int i;
  ALIGNED_ARGS;
  for (i=0; i<samples; i++)
    {
      LydSample   strength = ARG(0),
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
          data = state->data = g_malloc0 (sizeof (LydSample) *size + sizeof(EchoData));
          data->size = size;
          data->old = after_ptr (data, EchoData);
        }

      sample = sample + data->old[data->pos] * strength;
      data->old[data->pos++] = sample / (1.0 + strength);
      if (G_UNLIKELY (data->pos >= size))
        data->pos = 0;
      OUT = sample;
    }
  ALIGNED_ARGS_SILENCE;
}

static inline void op_free (LydOpState *state)
{
  g_free (state->data);
}

/**********************************************************************/

typedef struct _PluckData
{
   int        pos;
   int        size;
   int        asize;
   float      decay_ratio;
   LydSample *old;
} PluckData;

/* Karplus Strong plucked string, implements the decaying of harmonics
 * similar to a plucked string.
 */
static inline void op_pluck (OP_ARGS)
{
  PluckData *data   = state->data;
  ALIGNED_ARGS;
  int i = 0;
  
  for (i=0; i<samples; i++)
    {
      int         size   = (vm->sample_rate / ARG0(0));
      int         p2;

      if (size <=0)
        return;

      if (G_UNLIKELY (size > LYD_MAX_REVERB_SIZE))
        size = LYD_MAX_REVERB_SIZE;

      if (G_UNLIKELY (data == NULL ||
          size > data->asize))
        {
          int asize = size;
          if (asize < vm->sample_rate * 1.0)
            asize = vm->sample_rate * 1.0;
          if (state->data)
            g_free (state->data);
          data = state->data = g_malloc0 (sizeof (LydSample) * asize + sizeof(PluckData));
          data->asize = asize;
          data->size = size;
          data->old = after_ptr (data, PluckData);

          data->decay_ratio = ARG0(1);
          if (data->decay_ratio != 0.0)
            data->decay_ratio = 1.0/data->decay_ratio;
        }

      /* varying the generated original wave varies the type of pluck..
       */
      if (SAMPLE < size)
        {
          if (state->argc > 2)
            data->old[data->pos] = ARG(2);
          else
            data->old[data->pos] = noise () * 2;
        }

      OUT = data->old[data->pos];
      p2 = data->pos;
      p2--;
      if (p2 <0)
        p2 += size;

      if (state->argc < 2 ||
          data->decay_ratio > (noise() + 0.5))
        data->old[data->pos] = (data->old[data->pos] + data->old[p2])/2.00;

      data->pos++;
      if (G_UNLIKELY (data->pos >= size))
        data->pos -= size;
    }
  ALIGNED_ARGS_SILENCE;
}

/**********************************************************************/

#define MAX_DELAY_SIZE   (48000 * 200)

typedef struct _DelayData
{
   int    pos;
   int    size;
   int    asize;
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

      if (G_UNLIKELY (size > MAX_DELAY_SIZE))
        size = MAX_DELAY_SIZE;

      if (G_UNLIKELY (data == NULL ||
          size > data->asize))
        {
          int asize = size;
          if (asize < vm->sample_rate * 1.0)
            asize = vm->sample_rate * 1.0;
          if (state->data)
            g_free (state->data);
          data = state->data = g_malloc0 (sizeof (LydSample) * asize + sizeof(DelayData));
          data->asize = asize;
          data->size = size;
          data->old = after_ptr (data, DelayData);
        }

      OUT = data->old[data->pos];
      data->old[data->pos] = sample;
      data->pos ++;
      if (G_UNLIKELY (data->pos >= size))
        data->pos = 0;
    }
  ALIGNED_ARGS_SILENCE;
}

/**********************************************************************/

typedef struct _TappedDelayData
{
   int    pos;
   int    size;
   int    asize;
   int    taps[8];
   LydSample *old;
} TappedDelayData;


static inline void op_tapped_delay (OP_ARGS) /* XXX: should perhaps have the data as last arg? */
{
  /* XXX: allow dynamically changing the length */
  TappedDelayData *data   = state->data;
  int i;
  ALIGNED_ARGS;
  for (i=0; i<samples; i++)
    {
      LydSample sample = ARG(0),
                length[8];
      float     max_length=0.0;
      LydSample result = 0.0;
      int       size;

      int j;
      if (state->argc > 1) {length[0] = ARG(1);
      if (state->argc > 2) {length[1] = ARG(2);
      if (state->argc > 3) {length[2] = ARG(3);
      if (state->argc > 4) {length[3] = ARG(4);
      if (state->argc > 5) {length[4] = ARG(5);
      if (state->argc > 6) {length[5] = ARG(6);
      if (state->argc > 7) {length[6] = ARG(7);}}}}}}}

      for (j = 0; j < state->argc-1; j++)
          if (length[j] > max_length)
            max_length = length[j];

      size = max_length * vm->sample_rate;

      if (size <=0)
        return;

      if (G_UNLIKELY (size > MAX_DELAY_SIZE))
        size = MAX_DELAY_SIZE;

      if (G_UNLIKELY (data == NULL ||
          size > data->asize))
        {
          int asize = size;
          if (asize < vm->sample_rate * 1.0)
            asize = vm->sample_rate * 1.0;
          if (state->data)
            g_free (state->data);
          data = state->data = g_malloc0 (sizeof (LydSample) * asize + sizeof(TappedDelayData));
          data->asize = asize;

          data->size = size;
          data->old = after_ptr (data, TappedDelayData);
        }

      for (j = 0; j < state->argc-1; j++)
        {
          int tappos = data->pos + (max_length - length[j]) * vm->sample_rate + 1;
          while (tappos >= size)
            tappos -= size;
          result += data->old[tappos];
        }
      result /= (state->argc-1);

      OUT = result;
      data->old[data->pos] = sample;
      data->pos ++;
      if (data->pos >= size)
        data->pos = 0;
    }
  ALIGNED_ARGS_SILENCE;
}

/**********************************************************************/

typedef struct _TappedEchoData
{
   int    pos;
   int    size;
   int    asize;
   int    taps[8];
   LydSample *old;
} TappedEchoData;


static inline void op_tapped_echo (OP_ARGS)
{
  /* XXX: allow dynamically changing the length */
  TappedEchoData *data   = state->data;
  int i;
  ALIGNED_ARGS;
  for (i=0; i<samples; i++)
    {
      LydSample sample = ARG(0),
                length[8];
      float     max_length=0.0;
      LydSample result = 0.0;
      int       size;

      int j;
      if (state->argc > 1) {length[0] = ARG(1);
      if (state->argc > 2) {length[1] = ARG(2);
      if (state->argc > 3) {length[2] = ARG(3);
      if (state->argc > 4) {length[3] = ARG(4);
      if (state->argc > 5) {length[4] = ARG(5);
      if (state->argc > 6) {length[5] = ARG(6);
      if (state->argc > 7) {length[6] = ARG(7);}}}}}}}

      for (j = 0; j < state->argc-1; j++)
          if (length[j] > max_length)
            max_length = length[j];

      size = max_length * vm->sample_rate;

      if (size <=0)
        return;

      if (G_UNLIKELY (size > MAX_DELAY_SIZE))
        size = MAX_DELAY_SIZE;

      if (G_UNLIKELY (data == NULL ||
          size > data->asize))
        {
          int asize = size;
          if (asize < vm->sample_rate * 1.0)
            asize = vm->sample_rate * 1.0;
          if (state->data)
            g_free (state->data);
          data = state->data = g_malloc0 (sizeof (LydSample) * asize + sizeof(TappedEchoData));
          data->asize = asize;

          data->size = size;
          data->old = after_ptr (data, TappedEchoData);
        }

      for (j = 0; j < state->argc-1; j++)
        {
          int tappos = data->pos + (max_length - length[j]) * vm->sample_rate;
          while (tappos >= size)
            tappos -= size;
          result += data->old[tappos];
        }
      result /= (state->argc-1);

      OUT = data->old[data->pos] = sample + result * 0.9;
      data->pos ++;
      while (data->pos >= size)
        data->pos -= size;
    }
  ALIGNED_ARGS_SILENCE;
}

/**********************************************************************/

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
      pos = fmodf (freq * count * SAMPLE / vm->sample_rate, count);

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

/**********************************************************************/
