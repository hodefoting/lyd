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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lyd-private.h"

#define DEBUG_CLIPPING

static void lyd_voice_update_params (LydVoice *voice);


struct _Lyd
{
  int       sample_rate; /* sample rate */
  LydFormat format;      /* */

  unsigned int previous_samples; /* number of samples previously computed */
  unsigned long sample_no; /* counter for global sample no */
  SList    *voices;        /* list of currently playing voices */

  LydSample reverb;
  LydSample reverb_length;

  LydSample level;
  int       active;

  int       reverb_pos;

  LydSample *accbuf;
  int accbuf_len;

  LydSample reverb_old[2][LYD_MAX_REVERB_SIZE];
};

/* we include the voice directly to make the mixing and the vm 
 * a single compilation unit
 */
#include "lyd-voice.c"

static LydSample lyd_reverb (Lyd *lyd, int channel, LydSample sample)
{
  int size;
  int p;

  size = lyd->reverb_length * lyd->sample_rate;
  if (size > LYD_MAX_REVERB_SIZE)
    size = LYD_MAX_REVERB_SIZE;
  
  p = lyd->reverb_pos;
  sample = sample + lyd->reverb_old[channel][p] * lyd->reverb;
  lyd->reverb_old[channel][p] = sample / (1.0 + lyd->reverb);
  lyd->reverb_pos++;

  if (lyd->reverb_pos >= size)
    lyd->reverb_pos=0;
  return sample;
}

void lyd_midi_iterate (Lyd *lyd, float elapsed); 

long
lyd_synthesize (Lyd  *lyd,
                int   samples,
                void *stream,
                void *stream2)
{
  SList     *active = NULL;
  SList     *iter;
  int        i;
  LydSample *buf   = (void*)stream;
  LydSample *buf2  = (void*)stream2;
  short int *buf16 = (void*)stream;

  lyd_midi_iterate (lyd, lyd->previous_samples/(1.0 * lyd->sample_rate));
  LOCK ();

  if (!lyd->accbuf || lyd->accbuf_len < samples)
    {
      if (lyd->accbuf) free (lyd->accbuf);
      lyd->accbuf = malloc (sizeof (LydSample) * samples * 2);
      lyd->accbuf_len = samples;
    }

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

  memset (lyd->accbuf, 0, sizeof (LydSample) * samples * 2);

  for (iter=active; iter; iter=iter->next)
    {
      LydVoice *voice = iter->data;
      for (i=0;i<samples;i++)
        {
          voice->sample++;
          if (voice->sample > 0)
            {
              LydSample computed;
              if (  (voice->duration != 0 && voice->sample >= voice->duration)
                  || voice->released)
                voice->released++;
              
              lyd_voice_update_params (voice);
              computed = lyd_voice_compute (voice);

              if (voice->released)
                {
                  voice->silence_max = (computed > voice->silence_max)
                   ?computed
                   :voice->silence_max * (1.0 - LYD_RELEASED_SILENCE_DAMPENING);
                  voice->silence_min = (computed < voice->silence_min)
                   ?computed
                   :voice->silence_min * (1.0 - LYD_RELEASED_SILENCE_DAMPENING);
                }

              /* simple stereo distribution of mix */
              if (voice->position == 0.0)
                {
                  lyd->accbuf[i*2+0] += computed;
                  lyd->accbuf[i*2+1] += computed;
                }
              else if (voice->position > 0.0)
                {
                  lyd->accbuf[i*2+0] += computed * (1.0-voice->position);
                  lyd->accbuf[i*2+1] += computed;
                }
              else 
                {
                  lyd->accbuf[i*2+0] += computed;
                  lyd->accbuf[i*2+1] += computed * (1.0+voice->position);
                }
            }
        }
    }

  for (i=0;i<samples;i++)
    {
      LydSample value[2] = {lyd->accbuf[i*2+0], lyd->accbuf[i*2+1]};

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
          buf[i] = (value[0] + value[1])/2 * LYD_VOICE_VOLUME ;
          break;
        case LYD_f32S:
          buf[i] = value[0] * LYD_VOICE_VOLUME;
          buf2[i] = value[1] * LYD_VOICE_VOLUME;
          break;
        case LYD_s16S:
          buf16[i*2]   = (value[0] * 32767 * LYD_VOICE_VOLUME);
          buf16[i*2+1] = (value[1] * 32767 * LYD_VOICE_VOLUME);
#ifdef DEBUG_CLIPPING
          if (value[0] * 32767 * LYD_VOICE_VOLUME<-65535/2 ||
              value[0] * 32767 * LYD_VOICE_VOLUME>65535/2)
            {
              printf ("clipping\n");
            }
#endif
      }
      lyd->sample_no++;
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
  lyd->previous_samples = samples;
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

static LydVoice *lyd_voice_new_unlocked (Lyd       *lyd,
                                         LydProgram *program,
                                         int        tag)
{
  LydVoice *voice;
  voice     = lyd_voice_create (lyd, program);
  voice->sample_rate = lyd->sample_rate;
  voice->tag = tag;
  lyd->voices = slist_prepend (lyd->voices, voice);
  return voice;
}

LydVoice *lyd_voice_new (Lyd       *lyd,
                         LydProgram *program,
                         int        tag)
{
  LydVoice *voice;
  LOCK ();
  voice = lyd_voice_new_unlocked (lyd, program, tag);
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

void lyd_set_format (Lyd *lyd, LydFormat format)
{
  lyd->format = format;
}

int lyd_dead;

void lyd_free (Lyd *lyd)
{
  /* XXX: shutdown properly */
  lyd_dead = 1;
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
        if (STREQUAL(voice->state[j].literal[1], hash))
          {
            voice->state[j].literal[0] = value;/* could set the out directly?*/
            break;
          }
    }
  UNLOCK ();
}

typedef struct _LydParam
{
  LydSample        param_name;   /* string hashed to float */
  long             sample_no;    /* which absolute lyd-time to set this param for*/
  LydSample        value;        /* the value set */
  LydInterpolation interpolation;/* Interpolation to use for this segment */
  LydSample       *ptr;          /* the data location written */
} LydParam;
#define LYD_PARAM(a) ((LydParam*)(a))

void lyd_voice_set_param_delayed (Lyd        *lyd,        LydVoice    *voice,
                                  const char *param_name, float        time,
                                  LydInterpolation interpolation, 
                                  float       value)
{
  LydParam *param = g_new0 (LydParam, 1);

  param->sample_no = lyd->sample_no + lyd->sample_rate * time;

  param->param_name = str2float (param_name);
  param->value = value;
  param->interpolation = interpolation;

  LOCK ();
  if (slist_find (lyd->voices, voice))
    {
      int i;
      for (i=0; voice->state[i].op == LYD_NOP; i++)
        if (STREQUAL (voice->state[i].literal[1], param->param_name))
          {
            param->ptr = &(voice->state[i].literal[0]);
            break;
          }

      if (voice->params)
        {
          SList *param_i, *param_key, *prev = NULL;

          /* find parameter sublist */
          for (param_i = voice->params;
               param_i && !STREQUAL(LYD_PARAM (((SList*)(param_i->data))->data)->param_name, param->param_name);
               param_i = param_i->next);

          /* find insertion point in sublist */
          for (param_key = param_i?param_i->data:NULL;
               param_key
            && LYD_PARAM (param_key->data)->sample_no < param->sample_no;
               param_key = param_key->next)
            prev = param_key;

          if (prev) /* */
             prev->next = slist_prepend (param_key, param);
          else
            {
              if (param_i)
                param_i->data = slist_prepend (param_key, param);
              else
                voice->params = slist_prepend (voice->params,
                                               slist_prepend (NULL, param));
            }
        }
      else
        voice->params = slist_prepend (NULL, slist_prepend (NULL, param));
    }
  UNLOCK ();
}

static float
cubic (const float dx,
       const float prev, 
       const float j,
       const float next,
       const float nextnext)
{
  return (((( - prev + 3 * j - 3 * next + nextnext ) * dx +
            ( 2 * prev - 5 * j + 4 * next - nextnext ) ) * dx +
            ( - prev + next ) ) * dx + (j + j) ) / 2.0;
}

static void lyd_voice_update_params (LydVoice *voice)
{
  SList *paramlist;

  for (paramlist = voice->params;
       paramlist;
       paramlist = paramlist->next)
    {
      SList *i;
      LydParam *prev = NULL, *prev_prev = NULL;
      int freefirst = 0;

      for (i = paramlist->data; 
           i && LYD_PARAM (i->data)->sample_no < voice->lyd->sample_no;
           i = i->next)
        {
          prev_prev = prev;
          prev = i->data;
        }
      if (prev_prev && prev_prev != ((SList*)paramlist->data)->data)
        freefirst = 1;

      if (i && prev)
        {
          LydParam *curr = i->data;
          float     dt = curr->sample_no == prev->sample_no ? 1.0:
                         (voice->lyd->sample_no - prev->sample_no)
                        /(curr->sample_no - prev->sample_no * 1.0);
      
          switch (curr->interpolation)
            {
              case LYD_LINEAR:
                curr->ptr[0] = prev->value * (1.0-dt) + curr->value * dt;
                break;
              case LYD_GAP:
                curr->ptr[0] = 0.0;
                break;
              case LYD_STEP:
                curr->ptr[0] = dt < 0.9999?prev->value:curr->value;
                break;
              case LYD_CUBIC:
               {
                LydParam *next = i->next?i->next->data:curr;

                if (!prev_prev)
                  prev_prev = prev;
                curr->ptr[0] = cubic (dt, prev_prev->value, prev->value,
                                          curr->value, next->value);
                break;
               }
            }
        }

      if (freefirst) /* we trim away now unneeded items from the list */
        {            /* to speed up subsequent evaluation */
          SList *oldfirst = paramlist->data;
          paramlist->data = oldfirst->next;
          oldfirst->next = NULL;
          slist_free (oldfirst);
        }
    }
}
