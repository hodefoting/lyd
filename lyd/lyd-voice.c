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

static LydSample adsr (LydVoice *voice, LydSample **args, void **data)
{
  LydSample a = *args[0],  d = *args[1], s = *args[2],  r = *args[3];
  if (voice->released)
    {
      if (voice->released > r) /* after end of release */
        return 0.0;            
      else
        {
          float released_val;
          if ((voice->sample - voice->released) <= a)     /* release in attack*/
            released_val = ((voice->sample - voice->released) / a) * ((voice->sample - voice->released) / a);
          else if (voice->sample - voice->released < (a+d))/*release in decay */
            released_val = 1.0 + (s-1) * (((voice->sample - voice->released) - a) / d);
          else                                            /*release in sustain*/
            released_val = s;
          return released_val * (1.0 - (voice->released) / r);
        }
    }
  else if (voice->sample <= a)  /* in attack */
    {
      return (voice->sample / a) * (voice->sample / a);
    }
  else if (voice->sample < a + d)
    {                          /* in decay */
      return 1.0 + (s-1) * ((voice->sample - a) / d);
    }
  return s;  /* in sustain */
}

typedef struct _ReverbData 
{
   int    pos;
   int    size;
   LydSample *old;
} ReverbData;

static inline LydSample voice_reverb (LydVoice   *voice,
                                      LydSample **args,
                                      void      **edata)
{
  ReverbData *data   = *edata;
  LydSample   reverb = *args[0];
  LydSample   length = *args[1];
  LydSample   sample = *args[2];
  int         size   = length * voice->sample_rate;

  if (size <=0)
    return 0;

  if (G_UNLIKELY (size > LYD_MAX_REVERB_SIZE))
    size = LYD_MAX_REVERB_SIZE;

  if (G_UNLIKELY (data == NULL ||
      size != data->size))
    {
      data = *edata = g_malloc0 (sizeof (LydSample) *size + sizeof(ReverbData));
      data->size = size;
      data->old = (void*)((  ((char*)(data)) + sizeof (ReverbData)));
    }
  
  sample = sample + data->old[data->pos] * reverb;
  data->old[data->pos++] = sample / (1.0 + reverb);
  if (G_UNLIKELY (data->pos >= size))
    data->pos = 0;
  return sample;
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

/* shortcut boilerplate macros */
  #define A(no)              voice->state[i].arg[no][0] 
  #define TIME               ((1.0*voice->sample)/voice->sample_rate)
  #define OUT                voice->state[i].out
  #define DATA               voice->state[i].data
  #define OPS_START()        switch (voice->state[i].op){case LYD_NONE:{
  #define OPS_END()          }
  #define OP_START(op)       case op:{
  #define OP_END()           };break;

#if we //were processing buffers.. we would use this boilerplate instead ...

#define OPS_START()          switch (voice->state[i].op){case LYD_NONE:{{
  #define A(no)              voice->state[i].arg[no][0][j]
  #define OUT                voice->state[i].out[j]
  #define TIME               ((1.0*(voice->sample + j))/voice->sample_rate)
  #define OP_START(op)       case op:{int j;for (j=0;j<samples;j++){ 
  #define OP_END()           }};break;
#endif

/* we store a phase incrementer in each voice, allowing us to change
 * the hz of a signal generator on the fly without aliasing 
 */
static inline float phaseit(float oldphase, float hz, int sample_rate)
{
  float phase = oldphase + hz / sample_rate; /* XXX: we're always one ahead*/
  return fmod (phase, 1.0);
}

#define ABS(a) ((a)>0?a:-a)

  #define PHASE              (A(2) = phaseit(A(2), A(0), voice->sample_rate))

  #define OP(op)             OP_END() OP_START(op)
  #define OP_FUN(op, fun_name) OP_END() OP_START(op) OUT = fun_name (voice, \
                             &voice->state[i].arg[0], &DATA);

static inline LydSample lyd_voice_compute (LydVoice  *voice)
{
  register int i;
  
  for (i = 0; voice->state[i].op; i++)
    OPS_START()
    OP(LYD_NOP)    OUT =  voice->state[i].literal[0];
    OP(LYD_MUL)    OUT =  A(0) * A(1);
    OP(LYD_DIV)    OUT =  A(1)!=0.0?A(0) / A(1):0.0;
    OP(LYD_MIX)    OUT = (A(0) + A(1))/2;
    OP(LYD_MIX3)   OUT = (A(0) + A(1) + A(2))/3;
    OP(LYD_MIX4)   OUT = (A(0) + A(1) + A(2) + A(3))/4;
    OP(LYD_ADD)    OUT =  A(0) + A(1);
    OP(LYD_SUB)    OUT =  A(0) - A(1);
    OP(LYD_NEG)    OUT = - (A(0));
    OP(LYD_MOD)    OUT = fmod (A(0),A(1));
    OP(LYD_SQRT)   OUT = sqrt (A(0));
    OP(LYD_POW)    OUT = pow (A(0), A(1));
    OP(LYD_ABS)    OUT = fabs (A(0));
    OP(LYD_SIN)    OUT = sin (PHASE * M_PI * 2);
    OP(LYD_SQUARE) OUT = PHASE > 0.5?1.0:-1.0;
    OP(LYD_PULSE)  OUT = PHASE > A(1)?1.0:-1.0;

    OP(LYD_SAW)    OUT = PHASE * 2 - 1.0;
    OP(LYD_RAMP)   OUT = -(PHASE * 2 - 1.0);
    OP(LYD_NOISE)  OUT = g_random_double_range (-1.0, 1.0);

    /* OPL2 oscillators */
    OP(LYD_ABSSIN) OUT = fabs (sin (PHASE * M_PI * 2));
    OP(LYD_POSSIN) LydSample res = sin (PHASE * M_PI * 2); OUT = res>0?res:0.0;
    OP(LYD_EVENSIN) float angle = PHASE * M_PI * 2; LydSample res = sin (angle); OUT = res>0?sin(angle*2):0.0;

    OP_FUN(LYD_ADSR,   adsr)  
    OP_FUN(LYD_REVERB, voice_reverb) 

    /* handle all the filters specially in one block */
    OP_END() case LYD_LOW_PASS:  case LYD_HIGH_PASS: case LYD_BAND_PASS:
             case LYD_NOTCH:     case LYD_PEAK_EQ:   case LYD_LOW_SHELF:
    OP_START(LYD_HIGH_SELF)

    if (G_UNLIKELY (!DATA))
      DATA = BiQuad_new(voice->state[i].op-LYD_LOW_PASS,A(0),A(1), voice->sample_rate, A(2));
    else if (1)
       /* we normally wont need live updates, should be enabled when needed */
      BiQuad_update (DATA,voice->state[i].op-LYD_LOW_PASS,A(0),A(1),voice->sample_rate,A(2));

    OUT = BiQuad(A(3), DATA);
    OP_END()
    OPS_END()
  return voice->state[i-1].out;
}
