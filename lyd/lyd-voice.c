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

/** voice computation **/

#include "biquad.c"


#define LYD_ALIGN     16

#include <stdint.h>

/* create a new voice from a program */
static LydVoice * lyd_voice_create (Lyd *lyd, LydProgram *program)
{
  LydVoice *voice = g_malloc0 (sizeof (LydVoice)
                             + sizeof (LydOpState) * LYD_MAX_ELEMENTS + LYD_ALIGN);
  static LydSample nul = 0.0;
  int i, j;

  voice->state = (LydOpState*)(((char *)voice) + sizeof (LydVoice));

  /* ensure correct alignment of LydOpStates */
  {
    char *tmp = (void*)voice->state;
    int offset = LYD_ALIGN - ((uintptr_t) tmp) % LYD_ALIGN;
    tmp += offset;
    voice->state = (void*)tmp;
  }

  voice->lyd = lyd;

  for (i = 0; program->commands[i].op; i++)
    {
      voice->state[i].op = program->commands[i].op;
      for (j = 0; j < LYD_MAX_ARGS; j++)
        {
          int offset  = program->commands[i].arg[j];
          int k;

          for (k = 0; k < LYD_CHUNK; k++)
            voice->state[i].literal[j][k] = program->commands[i].arg[j];
          if (offset >= 0) /* direct pointer */
            {
              voice->state[i].arg[j] = &voice->state[i].literal[j][0];
              for (k = 0; k < LYD_CHUNK; k++)
                voice->state[i].out[k] = voice->state[i].literal[j][0];
            }
          else if (i + offset >= 0)
            voice->state[i].arg[j] = &voice->state[i + offset].out[0];
          else
            voice->state[i].arg[j] = &nul;
        }
      if (program->commands[i].op == LYD_ADSR)
        { /* premultiply with sample rate, simplifying runtime behavior  */
          int k;
          for (k = 0; k < LYD_CHUNK; k++)
            {
              voice->state[i].literal[0][k] *= lyd->sample_rate;
              voice->state[i].literal[1][k] *= lyd->sample_rate;
              voice->state[i].literal[3][k] *= lyd->sample_rate;
            }
        }
    }
  voice->position = 0.0;
  return voice;
}

static void
lyd_voice_free (LydVoice *voice)
{
  int i;
  for (i = 0; voice->state[i].op; i++)
    if (voice->state[i].data)
      switch (voice->state[i].op)
        {
          case LYD_LOW_PASS:  case LYD_HIGH_PASS: case LYD_BAND_PASS:
          case LYD_NOTCH:     case LYD_PEAK_EQ:
          case LYD_LOW_SHELF: case LYD_HIGH_SHELF:
            free (voice->state[i].data);
            break;
          case LYD_REVERB:
          default:
            g_free (voice->state[i].data);
        }
  {
    SList *l1, *l2;
    for (l1 = voice->params; l1; l1 = l1->next)
      {
        for (l2 = l1->data; l2; l2 = l2->next)
          g_free (l2->data);
        slist_free (l1->data);
      }
    slist_free (voice->params);
  }
  g_free (voice);
}

/* we store a phase incrementer in each voice, allowing us to change
 * the hz of a signal generator on the fly without sudden phase shifts.
 */
static inline float phase (LydVoice *voice, float *phasep, float hz)
{
  float old = *phasep;
  float new = old + hz * voice->i_sample_rate;
  *phasep = fmod (new, 1.0);
  return old;
}


static inline float input_sample (LydVoice *voice)
{
  float ret = 0.0;
  if (!voice->input_buf)
    return 0.0;
  if (voice->input_pos < voice->input_buf_len)
    ret = voice->input_buf[voice->input_pos];
  voice->input_pos ++;
  return ret;
}

#define MIDDLE_C 261.625565

static inline float wave_sample (LydVoice *voice, float *posp, int no, float hz)
{
  LydWave *wave = voice->lyd->wave[no];
  float old = *posp;
  float delta = voice->i_sample_rate * (hz>0.001?hz/MIDDLE_C:1.0);
  float new = old + delta;
  int sample_pos = new * wave->sample_rate;
  *posp = new;
  if (sample_pos < wave->samples)
    return wave->data[sample_pos];
  return 0.0;
}

static inline float wave_sample_loop (LydVoice *voice, float *posp, int no, float hz)
{
  LydWave *wave = voice->lyd->wave[no];
  float old = *posp;
  float delta = voice->i_sample_rate * (hz>0.001?hz/MIDDLE_C:1.0);
  float new = old + delta;
  int sample_pos = new * wave->sample_rate;
  *posp = new;
  if (sample_pos < wave->samples)
    return wave->data[sample_pos];
  else
    *posp = 0.0;
  return 0.0;
}

  /* Shortcut code used for implementing ops, keeping them short concise
   * and reabable.
   */

  /* The value coming in on an argument/from a source, for the first of
   * the computed samples, getting this value should always work (when
   * used, XXX: using ARG() instead should be considered once more of lyd is
   * buffer aware. */
  #define ARG0(no)           ((state->arg[no])[0])

  /* Get, and advance the phase for an oscillator: uses ARG0 because the state
   * for phase is carried per sample in chunk buffer  */
  #define PHASE_PEEK         ARG0(2)
  #define PHASE              phase(voice, &PHASE_PEEK, ARG0(0))

  /* pointer to data extension point */
  #define DATA               state->data

  /* macro used when defining ops, to indicate a callback function to
   * be used for processing
   */
  #define OP_FUN(fun)        fun(voice, state, samples);

  /* macro used to define looping code, this defines the variable i
   * used in some of the other macros.
   */
  #define OP_LOOP(code)      register int i;for (i = 0; i < samples; i++) {code}

  /* macros depending on i pointing to right index to work, needs
   * a loop over the samples to work properly
   */
  /* The current sample being computed */
  #define SAMPLE             (voice->sample + i)
  /* get the current time in seconds */
  #define TIME               (1.0 * SAMPLE * voice->i_sample_rate)
  /* the output destination for the current sample */
  #define OUT                state->out[i]

  /* the value of an argument, sample accurate */
  #define ARG(no)            ((state->arg[no])[i])

#ifndef ABS
  #define ABS(a)             ((a)>0?(a):-(a))
#endif

#define OP_ARGS LydVoice *voice, LydOpState *state, int samples

static inline void op_filter (OP_ARGS)
{
  int i;
  if (G_UNLIKELY (!DATA))
    DATA = BiQuad_new(state->op-LYD_LOW_PASS,ARG(0),ARG(1), voice->sample_rate, ARG(2));\
  
  for (i=0; i<samples; i++)
    {
      /* always updating the filter is expensive */
      BiQuad_update (DATA,state->op-LYD_LOW_PASS,ARG(0),ARG(1),voice->sample_rate,ARG(2));
      OUT = BiQuad(ARG(3), DATA);
    }
}

static inline void op_adsr (OP_ARGS)
{
  int i;
  for (i=0; i<samples; i++)
    {
      LydSample a = ARG(0),
                d = ARG(1),
                s = ARG(2),
                r = ARG(3);

      if (voice->released)
        {
          if (voice->released > r) /* after end of release */
            {
              OUT = 0.0;
            }
          else
            {
              float released_val;
              if ((SAMPLE - voice->released) <= a)     /* release in attack*/
                released_val = ((voice->sample - voice->released) / a) * ((SAMPLE - voice->released) / a);
              else if (SAMPLE - voice->released < (a+d))/*release in decay */
                released_val = 1.0 + (s-1) * (((SAMPLE - voice->released) - a) / d);
              else                                      /*release in sustain */
                released_val = s;
              OUT = released_val * (1.0 - (voice->released) / r);
            }
        }
      else if (SAMPLE <= a)                        /* in attack */
        OUT = (SAMPLE / a) * (SAMPLE / a);
      else if (SAMPLE < a + d)                     /* in decay */
        OUT = 1.0 + (s-1) * ((SAMPLE - a) / d);
      else                                         /* in sustain */
        OUT = s;                               
    }
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
  for (i=0; i<samples; i++)
    {
      LydSample   reverb = ARG(0),
                  length = ARG(1),
                  sample = ARG(2);
      int         size   = length * voice->sample_rate;

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
}

static inline void op_cycle (OP_ARGS)
{
  LydSample freq = *state->arg[0];
  int i;
  for (i = 0; i < samples; i++)
    {
      int      count, pos;

      for (count = LYD_MAX_ARGS - 1; count > 1 && ARG(count) == 0.0; count --);

      pos   = fmod (freq * count * SAMPLE / voice->sample_rate, count);

      OUT = ARG(1 + (pos+count) % count);
    }
}

#define LOOKUP_BITS  11   /* configuration of size of lookup-table */
#define LOOKUP_SIZE  (1<<11)
#define LOOKUP_MASK  (LOOKUP_SIZE-1)


static float lookup_inv_step;
static float sin_lookup[LOOKUP_SIZE];
static float noise_lookup[LOOKUP_SIZE];

void lyd_init_lookup_tables (void)
{
  static int done = 0;

  if (done)
    return;
  done = 1;

  {
    unsigned int i;
    float step;
    float f = 0;
    step = M_PI * 2.0f / (float)LOOKUP_SIZE;
    lookup_inv_step = 1.0f / step;

    for (i = 0; i <= LOOKUP_SIZE; i++, f += step)
      sin_lookup[i] = sinf(f);

    for (i = 0; i <= LOOKUP_SIZE; i++)
      noise_lookup[i] = g_random_double_range (-1.0, 1.0);
  }
}
/* inline lookuptable based versions version */
static inline float sine (float a)
{
  /* reduce size of sin lookup table by folding/mirroring the index value
   * with further arithmetic exploiting symmetry.
   */
  return sin_lookup [((int)(a * lookup_inv_step)) & LOOKUP_MASK];
}

static inline float noise (void)
{
  /* recycling a tiny buffer of noise is probably not what we want, but
   * it probably sounds good enough
   */
  static int pos = 0;
  pos = (pos+1) & LOOKUP_MASK;
  return noise_lookup [pos];
}

/* The core virtual machine, it computes maximum LYD_CHUNK
 * samples in one go, a pointer to the result is returned.
 */
static inline LydSample *
lyd_voice_compute (LydVoice  *voice,
                   int        samples)
{
  LydOpState *state;
  for (state = &voice->state[0]; state->op; state++)
    {
      switch (state->op)
        {
	        case LYD_NONE: break;
#define LYD_OP(name, OP_CODE, CODE, DOC, BAZ) \
          case LYD_##OP_CODE: { CODE } ; break;
          #include "lyd-ops.inc"
          /* the include expands into cases for opcodes and the code to
           * run when the opcode is invoked.
           */
          #undef LYD_OP
        }
    }
  voice->sample += samples;
  return (state-1)->out;
}
