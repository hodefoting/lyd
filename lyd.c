#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lyd.h"
#include "lyd-private.h"

#define VOLUME  0.05

#define MAX_REVERB_SIZE  44100


typedef float LydSample; /* global define for what type lyd computes with,
                            can be set to float or double, some mor jiggling
                            would be needed to support 32bit int directly */

struct _Lyd
{
  int       sample_rate; /* sample rate    */
  LydFormat format;  /* */

  unsigned long sample_no; /* counter for global sample no */
  SList    *voices;    /* list of currently playing voices */

  LydSample reverb;
  LydSample reverb_length;

  LydSample level;
  int       active;

  int       reverb_pos;
  LydSample reverb_old[2][MAX_REVERB_SIZE];
};

typedef struct _CommandState 
{ LydOp  op;
  LydSample  out; /* XXX: for buffer mode should be a pointer to a LydSample buf */
  LydSample *arg[LYD_MAX_ARGS];
  LydSample  literal[LYD_MAX_ARGS];
  void  *data;
} CommandState;

struct _LydVoice
{
  LydSample position; /* 0.0 center -1.0 left 1.0 right */
  LydSample duration; /* how long the sample should last */
  int   released; /* the number of samples we have been released, calling
                       voice_release increments this and starts the release
                       process */
  long  sample;   /* position, negative values means queued for playback,
                       controlled by lyd */
  int   sample_rate;

  LydSample silence_min; /* Silence detection */
  LydSample silence_max; /* (after release) */

  void  (*complete_cb)(void *data); /* callback and data when voice is done*/
  void  *complete_data;             /* data for complete callback */
  
  int   tag;

  CommandState state[]; /* instruction and working data should
                           stay close using this layout, note: variable
                           sized array
                         */
};

static LydVoice    *lyd_voice_new     (LydProgram *program);
static void         lyd_voice_free    (LydVoice  *voice);
static inline LydSample lyd_voice_compute (LydVoice  *voice);


static LydSample lyd_reverb (Lyd *lyd, int channel, LydSample sample)
{
  int size;
  int p;

  size = lyd->reverb_length * lyd->sample_rate;
  if (size > MAX_REVERB_SIZE)
    size = MAX_REVERB_SIZE;
  
  p = lyd->reverb_pos;
  sample = sample + lyd->reverb_old[channel][p] * lyd->reverb;
  lyd->reverb_old[channel][p] = sample / (1.0 + lyd->reverb);
  lyd->reverb_pos++;

  if (lyd->reverb_pos >= size)
    lyd->reverb_pos=0;
  return sample;
}


static void bar (int width, float value)
{
  char *eights[] = { " ", "▏", "▍", "▌", "▋", "▊", "▉", "█" };
  int blocks = width * 8 * value;
  int     i;

  for (i = 0; i < blocks / 8; i++)
    printf  ("█");
  printf (eights[blocks % 8]);
  for (i = blocks / 8; i< width; i++)
    printf (" ");
  fflush (0);
}


static void lyd_stats (Lyd *lyd)
{
  printf ("[lyd: voices: %3i] [", lyd->active);
  bar (12, pow(lyd->level/10, 0.5));
  printf ("] %2.4f       \r", lyd->level/10);
  lyd->level = 0.0;
}

long
lyd_synthesize (Lyd  *lyd,
                int   samples,
                void *stream,
                void *stream2)
{
  SList     *active = NULL;
  SList     *iter;
  int        i;
  LydSample     *buf32 = (void*)stream;
  short int *buf16 = (void*)stream;

  LOCK ();

  /* create a list of voices that are currently playing or will
   * start playing during the duration of samples
   */
  for (iter = lyd->voices; iter; iter=iter->next)
    {
      LydVoice *voice = iter->data;
      if (voice->sample + samples >=0)
        active = slist_prepend (active, iter->data);
      else
        voice->sample += samples;
    }

  for (i=0;i<samples;i++)
    {
      LydSample value[2] = {0.0, 0.0};
      for (iter=active; iter; iter=iter->next)
        {
          LydVoice *voice = iter->data;

          voice->sample++;
          if (voice->sample > 0)
            {
              LydSample computed;
              if ((voice->duration != 0 && voice->sample >= voice->duration)
               || voice->released)
                voice->released++;
              
              computed = lyd_voice_compute (voice);

#define SILENCE_DETECT_DAMPENING 0.001
              if (voice->released)
                {
                  voice->silence_max = (computed > voice->silence_max)
                     ?computed
                     :voice->silence_max * (1.0 - SILENCE_DETECT_DAMPENING);
                  voice->silence_min = (computed < voice->silence_min)
                     ?computed
                     :voice->silence_min * (1.0 - SILENCE_DETECT_DAMPENING);
                }

              if (voice->position == 0.0)         /* stereo decomposition */
                {
                  value[0] += computed;
                  value[1] += computed;
                }
              else if (voice->position > 0.0)
                {
                  value[0] += computed * (1.0-voice->position);
                  value[1] += computed;
                }
              else 
                {
                  value[0] += computed;
                  value[1] += computed * (1.0+voice->position);
                }
            }
        }
      if (lyd->reverb > 0.0001)
        {
          value[0] = lyd_reverb (lyd, 0, value[0]);
          value[1] = lyd_reverb (lyd, 1, value[1]);
        }

      {
        LydSample nlevel = fabs(value[0]);
        if (nlevel > lyd->level)
          lyd->level = nlevel;
      }

    switch (lyd->format)
      {
        case LYD_f32:
          buf32[i] = (value[0] + value[1])/2 * VOLUME ;
          break;
        case LYD_f32S:
          /* XXX */
          break;
        case LYD_s16S:
          buf16[i*2]   = (value[0] * 32767 * VOLUME);
          buf16[i*2+1] = (value[1] * 32767 * VOLUME);
#if 1
          if (value[0] * 32767 * VOLUME<-65535/2 ||
              value[0] * 32767 * VOLUME>65535/2)
            {
              printf ("clipping\n");
            }
#endif
      }
      lyd->sample_no++;
      if (lyd->sample_no % (44100/3) == 0)
        lyd_stats (lyd);
    }

  lyd->active = 0;
  for (iter=active; iter; iter=iter->next)
    {                                  /* remove released and silent voices */
      LydVoice *voice = iter->data;
      if (voice->released &&
          (voice->silence_max -
           voice->silence_min < 0.0000001 ||
           voice->released > voice->sample_rate * 15.0)
          )
        {
          lyd->voices = slist_remove (lyd->voices, voice);
          if (voice->complete_cb)
            (voice->complete_cb) (voice->complete_data);
          lyd_voice_free (voice);
        }
      lyd->active ++;
    }
  slist_free (active);
  UNLOCK ();
  return lyd->sample_no;
}

void
lyd_kill (Lyd *lyd,
          int  tag)
{
  SList *iter;
  LOCK ();
 
again: 
  for (iter = lyd->voices; iter; iter=iter->next)
    {
      LydVoice *voice = iter->data;
      if (voice->tag == tag)
        {
          iter = lyd->voices = slist_remove (lyd->voices, voice);
          lyd_voice_free (voice);
          goto again;
        }
    }
  UNLOCK ();
}

void lyd_voice_kill (Lyd *lyd,
                     LydVoice *voice)
{
  LOCK ();
  if (slist_find (lyd->voices, voice))
    {
      lyd->voices = slist_remove (lyd->voices, voice);
      lyd_voice_free (voice);
    }
  UNLOCK ();
}

void
lyd_voice_release (Lyd      *lyd,
                   LydVoice *voice)
{
  LOCK ();
  if (slist_find (lyd->voices, voice))
    {
      voice->released++;
      voice->silence_min = -100;
      voice->silence_max = 100;
    }
  UNLOCK ();
}

static LydVoice *lyd_new_voice_unlocked (Lyd       *lyd,
                                         LydProgram *program,
                                         int        tag)
{
  LydVoice *voice;
  voice     = lyd_voice_new (program);
  voice->sample_rate = lyd->sample_rate;
  voice->tag = tag;
  lyd->voices = slist_prepend (lyd->voices, voice);
  return voice;
}

LydVoice *lyd_new_voice (Lyd       *lyd,
                         LydProgram *program,
                         int        tag)
{
  LydVoice *voice;
  LOCK ();
  voice = lyd_new_voice_unlocked (lyd, program, tag);
  UNLOCK ();
  return voice;
}

void lyd_voice_set_duration (Lyd *lyd, LydVoice *voice, double seconds)
{
  LOCK ();
  if (slist_find (lyd->voices, voice))
    voice->duration = seconds * lyd->sample_rate;
  UNLOCK ();
}

void lyd_voice_set_delay (Lyd *lyd, LydVoice *voice, double seconds)
{
  LOCK ();
  if (slist_find (lyd->voices, voice))
    voice->sample = - (seconds * lyd->sample_rate);
  UNLOCK ();
}
                          
Lyd * lyd_new (void)
{
  Lyd *lyd = g_new0 (Lyd, 1);
  return lyd;
}

void lyd_set_sample_rate (Lyd *lyd, int sample_rate)
{
  lyd->sample_rate = sample_rate;
}

void lyd_set_format      (Lyd *lyd, LydFormat format)
{
  lyd->format = format;
}

void lyd_free (Lyd *lyd)
{
  /* XXX: shutdown properly */
  g_free (lyd);
}

void lyd_voice_set_position (Lyd      *lyd,
                             LydVoice *voice,
                             double    position)
{
  LOCK ();
  if (slist_find (lyd->voices, voice))
    voice->position = position;
  UNLOCK ();
}

/** voice computation **/

static LydVoice * lyd_voice_new (LydProgram *program)
{
  LydVoice *voice = g_malloc0 (sizeof (LydVoice)
                             + sizeof (CommandState) * LYD_MAX_ELEMENTS);
  static LydSample nul = 0.0;
  int i, j;

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
            }
          else if (i + offset >= 0)
            voice->state[i].arg[j] = &voice->state[i + offset].out;
          else
            voice->state[i].arg[j] = &nul;
        }
    }
  voice->position = 0.0;
  return voice;
}

static LydSample adsr (LydVoice *voice, LydSample **args, void **data)
{
  LydSample a = *args[0],  d = *args[1],
        s = *args[2],  r = *args[3];

  if (voice->sample <= a * voice->sample_rate)
    return (voice->sample * 1.0 / (a * voice->sample_rate));
  else if (voice->sample < (a * voice->sample_rate) + (d * voice->sample_rate))
    {
      LydSample d2 = (((voice->sample - (a * voice->sample_rate))) / (d * voice->sample_rate));
      return 1.0 * d2 + s * (1.0-d2);
    }
  else if (voice->released)
    {
      if (voice->released > r * voice->sample_rate)
        return 0.0; /* after end of release */
      else
        return s * (1.0 -(voice->released) / (r * voice->sample_rate));
    }
  return s;
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
  LydSample       reverb = *args[0];
  LydSample       length = *args[1];
  LydSample       sample = *args[2];
  int         size   = length * voice->sample_rate;

  if (size <=0)
    return 0;

  if (G_UNLIKELY (size > MAX_REVERB_SIZE))
    size = MAX_REVERB_SIZE;

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
  return sample
;
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
  g_free (voice);
}

void
lyd_program_set_param (LydProgram *program, 
                       const char *param,
                       double      value)
{
  printf ("%s NYI\n", __FUNCTION__);
}

void
lyd_voice_set_param (Lyd        *lyd,
                     LydVoice   *voice,
                     const char *param,
                     double      value)
{
  int j;
  LOCK ();
  if (slist_find (lyd->voices, voice))
    {
      float hash = str2float (param);
      for (j=0;voice->state[j].op == LYD_NOP; j++)
        if (voice->state[j].literal[1] == hash)
          {
            voice->state[j].literal[0] = value;
            break;
          }
    }
  UNLOCK ();
}

double
lyd_voice_get_param (Lyd        *lyd,
                     LydVoice   *voice,
                     const char *param)
{
  double ret = 0;
  int j;
  LOCK ();
  if (slist_find (lyd->voices, voice))
    {
      float hash = str2float (param);
      for (j=0;voice->state[j].op == LYD_NOP; j++)
        {
          if (voice->state[j].literal[1] == hash)
            {
              ret = voice->state[j].literal[0];
              break;
            }
        }
    }
  UNLOCK ();
  return ret;
}


/* What follows is the actual execution engine of the virtual machine */

#include "biquad.c"

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

  #define PHASE              fmod((TIME) * A(0), 1.0)
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
    OP(LYD_SIN)    OUT = sin (TIME * M_PI * 2 * A(0));
    OP(LYD_SQUARE) OUT = PHASE > 0.5?1.0:-1.0;
    OP(LYD_PULSE)  OUT = PHASE > A(1)?1.0:-1.0;
    OP(LYD_SAW)    OUT = PHASE * 2 - 1.0;
    OP(LYD_RAMP)   OUT = -(PHASE *2 - 1.0);
    OP(LYD_NOISE)  OUT = g_random_double_range (-1.0, 1.0);

    OP_FUN(LYD_ADSR, adsr)  
    OP_FUN(LYD_REVERB, voice_reverb) 

    /* handle all the filters specially in one block */
    OP_END() case LYD_LOW_PASS:  case LYD_HIGH_PASS: case LYD_BAND_PASS:
             case LYD_NOTCH:     case LYD_PEAK_EQ:   case LYD_LOW_SHELF:
    OP_START(LYD_HIGH_SELF)

    if (G_UNLIKELY (!DATA))
      DATA = BiQuad_new(voice->state[i].op-LYD_LOW_PASS,A(0),A(1), voice->sample_rate, A(2));
    else if (0)
       /* we normally wont need live updates, should be enabled when needed */
      BiQuad_update (DATA,voice->state[i].op-LYD_LOW_PASS,A(0),A(1),voice->sample_rate,A(2));

    OUT = BiQuad(A(3), DATA);
    OP_END()
    OPS_END()
  return voice->state[i-1].out;
}
