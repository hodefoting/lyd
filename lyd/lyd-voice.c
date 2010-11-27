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

/* create a new voice from a program */
static LydVoice * lyd_voice_create (Lyd *lyd, LydProgram *program)
{
  LydVoice *voice = g_malloc0 (sizeof (LydVoice)
                             + sizeof (LydCommandState) * LYD_MAX_ELEMENTS);
  static LydSample nul = 0.0;
  int i, j;
  voice->state = (LydCommandState*)(((char *)voice) + sizeof (LydVoice));

  voice->lyd = lyd;

  for (i = 0; program->commands[i].op; i++)
    {
      voice->state[i].op = program->commands[i].op;
      for (j = 0; j < LYD_MAX_ARGS; j++)
        {
          int offset  = program->commands[i].arg[j];

          voice->state[i].literal[j] = program->commands[i].arg[j];
          if (offset >= 0) /* direct pointer */
            {
              voice->state[i].arg[j] = &voice->state[i].literal[j];
              voice->state[i].out = voice->state[i].literal[j];
            }
          else if (i + offset >= 0)
            voice->state[i].arg[j] = &voice->state[i + offset].out;
          else
            voice->state[i].arg[j] = &nul;
        }
      if (program->commands[i].op == LYD_ADSR)
        { /* premultiply with sample rate, simplifying runtime behavior  */
          voice->state[i].literal[0] *= lyd->sample_rate;
          voice->state[i].literal[1] *= lyd->sample_rate;
          voice->state[i].literal[3] *= lyd->sample_rate;
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
          case LYD_LOW_SHELF: case LYD_HIGH_SELF:
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


/* What follows is the actual execution engine of the virtual machine */



/* Misc profiling log keeping, the numbers listed are the maximum concurrent
 * number of given programs.
 *
 * just square wave: 478 438 400 464 477 481
 *
 *   after adding buf: 461 458 446 449 450
 *
 * just sin    wave: 249 225 241
 *
 * sum of two sqw, w separate adsrs: 274 281 239
 * sum of two sinqw, w separate adsrs: 122 129 125
 *
 * waveform fully solo: 528 522
 * waveform w adsr or scale: 387 394 376 438 440
 */


/* we store a phase incrementer in each voice, allowing us to change
 * the hz of a signal generator on the fly without sudden phase shifts.
 */
static inline float phase(LydVoice *voice, float *phasep, float hz)
{
  float old = *phasep;
  float new = old + hz * voice->i_sample_rate;
  *phasep = fmod (new, 1.0);
  return old;
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


/* shortcut boilerplate macros, used in case the execution engine is
 * modified to operate on buffers instead of individual samples
 */
  #define ARG(no)            (*(state->arg[no]))
  #define SAMPLE             (voice->sample + i)
  #define TIME               (1.0 * SAMPLE * voice->i_sample_rate)
  #define OUT                state->out
  #define PHASE              phase(voice, &ARG(2), ARG(0))
  #define DATA               state->data

  #define OPS_START()        switch (state->op){case LYD_NONE:{
  #define OPS_END()          }}
  #define OP_START(op)       case LYD_##op:{
  #define OP_END()           };break;

  #define OP(op)               OP_END() OP_START(op)
  #define OP_FUN(op, fun_name) OP_END() OP_START(op) fun_name (voice, state, i);
  #define ABS(a)               ((a)>0?(a):-(a))

  #define HANDLE_FILTER \
    if (G_UNLIKELY (!DATA))\
      DATA = BiQuad_new(state->op-LYD_LOW_PASS,ARG(0),ARG(1), voice->sample_rate, ARG(2));\
    else if (1)\
      BiQuad_update (DATA,state->op-LYD_LOW_PASS,ARG(0),ARG(1),voice->sample_rate,ARG(2));\
    OUT = BiQuad(ARG(3), DATA);

  #define OP_ARGS LydVoice *voice, LydCommandState *state, int i

static void adsr (OP_ARGS)
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
          else                                            /*release in sustain*/
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

typedef struct _ReverbData 
{
   int    pos;
   int    size;
   LydSample *old;
} ReverbData;

static inline void voice_reverb (OP_ARGS)
{
  ReverbData *data   = state->data;
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

static inline void voice_cycle (OP_ARGS)
{
  LydSample freq = *state->arg[0];
  int      count, pos;

  for (count = LYD_MAX_ARGS - 1; count > 1 && ARG(count) == 0.0; count --);

  pos   = fmod (freq * count * SAMPLE / voice->sample_rate, count);

  OUT = ARG(1 + (pos+count) % count);
}


//void
static inline void
lyd_voice_compute (LydVoice  *voice,
                   int        samples,
                   LydSample *retbuf)
{
  LydCommandState *state;
  int i;
  
  //printf ("%i %i  ", samples, ((unsigned int)(retbuf)) % 32);

  /* here, do a check on whether SIMD might be applicable or not, and
   * dispatch correct version
   */
  for (i = 0; i<samples; i++)
    {
      for (state = &voice->state[0]; state->op; state++)
        {
          OPS_START()
          OP(NOP)    OUT =  state->literal[0];
          OP(MUL)    OUT =  ARG(0) * ARG(1);
          OP(DIV)    OUT =  ARG(1)!=0.0 ? ARG(0) / ARG(1):0.0;
          OP(MIX)    OUT = (ARG(0) + ARG(1))/2;
          OP(MIX3)   OUT = (ARG(0) + ARG(1) + ARG(2))/3;
          OP(MIX4)   OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3))/4;
          OP(ADD)    OUT =  ARG(0) + ARG(1);
          OP(SUB)    OUT =  ARG(0) - ARG(1);
          OP(NEG)    OUT = - (ARG(0));
          OP(MOD)    OUT = fmod (ARG(0),ARG(1));
          OP(SQRT)   OUT = sqrt (ARG(0));
          OP(POW)    OUT = pow (ARG(0), ARG(1));
          OP(ABS)    OUT = fabs (ARG(0));
          OP(SIN)    OUT = sin (PHASE * M_PI * 2);
          OP(SQUARE) OUT = PHASE > 0.5?1.0:-1.0;
          OP(PULSE)  OUT = PHASE > ARG(1)?1.0:-1.0;

          OP(TRIANGLE) OUT = PHASE < 0.25 ? 0 + ARG(2)*4 :
                              ARG(2)  < 0.75 ? 2 - ARG(2)*4 : -4 + ARG(2)*4;

          OP(SAW)    OUT = PHASE * 2 - 1.0;
          OP(RAMP)   OUT = -(PHASE * 2 - 1.0);
          OP(NOISE)  OUT = g_random_double_range (-1.0, 1.0);

          /* OPL2 oscillators */
          OP(ABSSIN) OUT = fabs (sin (PHASE * M_PI * 2));
          OP(POSSIN) OUT = PHASE < 0.5 ? sin (ARG(2) * M_PI * 2) : 0.0;
          OP(PULSSIN) OUT = fmod (PHASE, 0.5) < 0.25 ? fabs (sin (ARG(2) * M_PI * 2)) : 0.0;

          /* OPL3 oscillators */
          OP(EVENSIN) OUT = PHASE < 0.5 ? sin (2 * ARG(2) * M_PI * 2) : 0.0;
          OP(EVENPOSSIN) OUT = PHASE < 0.5 ? fabs (sin (2 * ARG(2) * M_PI * 2)) : 0.0;

          /* wave data */
          OP(WAVE)   OUT = wave_sample (voice, &ARG(3), ARG(0), ARG(1));
          /* wave data */
          OP(WAVELOOP) OUT = wave_sample_loop (voice, &ARG(3), ARG(0), ARG(1));

          OP_FUN(ADSR,   adsr)
          OP_FUN(REVERB, voice_reverb)
          OP_FUN(CYCLE,  voice_cycle)

          OP(LOW_PASS)   HANDLE_FILTER
          OP(HIGH_PASS)  HANDLE_FILTER
          OP(BAND_PASS)  HANDLE_FILTER
          OP(NOTCH)      HANDLE_FILTER
          OP(LOW_SHELF)  HANDLE_FILTER
          OP(HIGH_SELF)  HANDLE_FILTER
          OP(PEAK_EQ)    HANDLE_FILTER
          OPS_END()
        }
      retbuf[i] = (state-1)->out;
    }
}
