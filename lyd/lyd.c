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
#include <assert.h>
#include "lyd-private.h"

int lyd_op_argc[]=
{
  0
  #define LYD_OP(name, OP_CODE, ARG_COUNT, CODE, DOC, ARG_DOC), ARG_COUNT 
  #include "lyd-ops.inc"
  #undef LYD_OP
  , -1
};

static void lyd_voice_update_params (LydVM *voice,
                                     int       samples);


/* we include the voice directly to make the mixing and the vm 
 * a single compilation unit
 */
#include "lyd-vm.c"

void lyd_midi_iterate (Lyd *lyd, float elapsed); 


static void
lyd_synthesize_voice (Lyd      *lyd,
                      LydVM *voice,
                      int       samples,
                      int       tot_samples,
                      int       pos)
{
  int        i;
  LydSample * __restrict__ result = NULL;

  /* the accumulation buffer is temporary.. and shared between all voices
   * we keep one around and grow it if it isn't large enough.. might need
   * to keep an accum- buffer per voice instead..
   *
   * if there were remainder samples from previous rendering, they should
   * be inserted at the start of the accumbuf,.. and perhaps the accumbuf
   * start should be adjusted to fulfil SIMD requirements?
   */

  /* blanking accumulation buffer... */
  assert (samples <= LYD_CHUNK);

  /* skip voices that are not yet playing */
  if (voice->sample + samples <0)
    {
      voice->sample += samples;
      return;
    }

  {
    int first_sample = voice->sample<0?-voice->sample:0;
    voice->sample += first_sample;
    
    lyd_voice_update_params (voice, samples - first_sample);
    /* result is a direct pointer to the results in the last processing chain */
    result = lyd_vm_compute (voice, samples - first_sample);

    if (  (voice->duration != 0 && voice->sample >= voice->duration)
           || voice->released)
      voice->released += samples - first_sample;

    /* do silence detection for released voices, to know when
     * the voice itself can be automatically destroyed.
     */
    if (voice->released)
      for (i=first_sample;i<samples;i++)
        {
          LydSample computed = result[i-first_sample];
          voice->silence_max = (computed > voice->silence_max)
           ?computed
           :voice->silence_max * (1.0 - LYD_RELEASE_SILENCE_DAMPENING);
          voice->silence_min = (computed < voice->silence_min)
           ?computed
           :voice->silence_min * (1.0 - LYD_RELEASE_SILENCE_DAMPENING);
        }

    /* simple stereo distribution of mix */
    if (voice->position == 0.0)
      {
        for (i=first_sample;i<samples;i++)
          lyd->buf[pos+i] += result[i-first_sample];
        for (i=0;i<samples;i++)
          lyd->buf[pos+i+tot_samples] += result[i-first_sample];
      }
    else if (voice->position > 0.0)
      {
        for (i=first_sample;i<samples;i++)
          lyd->buf[pos+i] += result[i-first_sample] * (1.0-voice->position);
        for (i=first_sample;i<samples;i++)
          lyd->buf[pos+i+tot_samples] += result[i-first_sample];
      }
    else 
      {
        for (i=first_sample;i<samples;i++)
          lyd->buf[pos+i] += result[i - first_sample];
        for (i=first_sample;i<samples;i++)
          lyd->buf[pos+i+tot_samples] += result[i - first_sample] * (1.0+voice->position);
      }
  }
 
}

/* should only be called after the sample rate has been set
 * on lyd, the global filter could be extended into a
 * generic posprocessing and stereo dispatch program.
 *
 * With separate processing pipeline statements all in a
 * single lyd source.  * With multiple statements specifying
 * and carrying out the assignment to output buffers would be
 * needed in programs (autodetecting and expanding the
 * single expression case would be good.)
 *
 * For now it is most useful for postprocessing, adding dither
 * with "input() + (noise()-0.5) * (1./065536)"
 */
void lyd_set_global_filter (Lyd *lyd, LydProgram *program)
{
  int i;
  if (!program)
    {
      for (i = 0; i< 2; i++)
        {
          if (lyd->global_filter[i])
            lyd_filter_free (lyd->global_filter[i]);
          lyd->global_filter[i] = NULL;
        }
      return;
    }
  for (i = 0; i< 2; i++)
    {
      if (lyd->global_filter[i])
        lyd_filter_free (lyd->global_filter[i]);
      if (program)
        lyd->global_filter[i] = lyd_filter_new (lyd, program);
      else
        lyd->global_filter[i] = NULL;
    }
}

void clean_silent (Lyd *lyd, SList *active)
{
  SList *iter;
  lyd->active = 0;
  for (iter=active; iter; iter=iter->next)
    {                                  /* remove released and silent voices */
      LydVM *voice = iter->data;
      if (voice->released > voice->sample_rate / LYD_MIN_RELASE_TIME_DIVISOR
       && (voice->silence_max -
           voice->silence_min < LYD_RELEASE_THRESHOLD ||
           voice->released > voice->sample_rate * 15.0)
          )
        {
          lyd->voices = slist_remove (lyd->voices, voice);
          if (voice->complete_cb)
            (voice->complete_cb) (voice->complete_data);
          lyd_vm_free (voice);
          iter->data = NULL;
        }
      else
        lyd->active ++;
    }
  while (lyd->active > lyd->max_active)
    {
      SList    *weakest_link = NULL;
      LydVM *weakest = NULL;
      float     best_score = 0;
      for (iter=active; iter; iter=iter->next)
        {
          LydVM *voice = iter->data;
          float score = 0;
          if (!voice)
            continue;
          if (voice->released)
            {
              score += voice->released * 10;
              score += voice->sample * 0.01;
            }
          else
              score = voice->sample * 0.1;
          if (score > best_score)
            {
              best_score = score;
              weakest = voice;
              weakest_link = iter;
            }
        }
      if (weakest){
        lyd->voices = slist_remove (lyd->voices, weakest);
        if (weakest->complete_cb)
          (weakest->complete_cb) (weakest->complete_data);
        lyd_vm_free (weakest);
        weakest_link->data = NULL;
        lyd->active--;
      }
      else
        break;
    }
}

long
lyd_synthesize (Lyd  *lyd,
                int   samples,
                void *stream,
                void *stream2)
{
  int i;
  SList     *iter, *active = NULL;
  LydSample * __restrict__ buf   = (void*)stream;
  LydSample * __restrict__ buf2  = (void*)stream2;
  short int * __restrict__ buf16 = (void*)stream;

  { /* XXX: revisit whether this should happen under lock.. */
    float elapsed = lyd->previous_samples/(1.0 * lyd->sample_rate);
    lyd_midi_iterate (lyd, elapsed);
    for (i = 0; i < LYD_MAX_CBS; i++)
      if (lyd->pre_cb[i])
        lyd->pre_cb[i](lyd, elapsed, lyd->pre_cb_data[i]);
  }

  if (lyd->buf_len < samples || lyd->buf == NULL)
    {
      if (lyd->buf)
        g_free (lyd->buf);
      lyd->buf = g_malloc0 (sizeof (LydSample) * samples * 2);
      lyd->buf_len = samples;
    }
  memset (lyd->buf, 0, sizeof (LydSample) * samples * 2);

  LOCK ();
  /* create a list of voices that are currently playing or will
   * start playing during the duration of samples
   */
  for (iter = lyd->voices; iter; iter=iter->next)
    {
      LydVM *voice = iter->data;
      if (voice->sample + samples >=0)
        {
          int left = samples;
          int pos = 0;
          while (left > 0) /* break processing up in sizes of size LYD_CHUNK or smaller */
            {
              int chunk = LYD_CHUNK;
              if (chunk > left)
                chunk = left;
              left -= chunk;
              lyd_synthesize_voice (lyd, voice, chunk, samples, pos);
              pos += chunk;
            }

          active = slist_prepend (active, iter->data);
        }
      else
        voice->sample += samples;
    }

  if (lyd->global_filter[0])
    lyd_filter_process (lyd->global_filter[0], lyd->buf, lyd->buf, samples);
  if (lyd->global_filter[1])
    lyd_filter_process (lyd->global_filter[1], lyd->buf + samples, lyd->buf + samples, samples);

  for (i=0;i<samples;i++)
    {
      LydSample value[2];
      value[0] = lyd->buf[i];
      value[1] = lyd->buf[i+samples];
      {
        LydSample nlevel = fabs(value[0]);
        if (nlevel > lyd->level)
          lyd->level = nlevel;
#ifdef DEBUG_CLIPPING
        if (nlevel > 1.0)
          printf ("clipping\n");
#endif
        nlevel = fabs(value[1]);
        if (nlevel > lyd->level)
          lyd->level = nlevel;
#ifdef DEBUG_CLIPPING
        if (nlevel > 1.0)
          printf ("clipping\n");
#endif
      } 
    }

  /* write from accumbuf into actual buffer */
  switch (lyd->format)
    {
      case LYD_f32:
        for (i=0;i<samples;i++)
          buf[i] = (lyd->buf[i] + lyd->buf[i+samples])/2 * LYD_VOICE_VOLUME ;
        break;
      case LYD_f32S:
        for (i=0;i<samples;i++)
          buf[i] = lyd->buf[i] * LYD_VOICE_VOLUME;
        for (i=0;i<samples;i++)
          buf2[i] = lyd->buf[i+samples] * LYD_VOICE_VOLUME;
        break;
      case LYD_s16S:
        for (i=0;i<samples;i++)
          buf16[i*2]  = (lyd->buf[i] * 32767 * LYD_VOICE_VOLUME);
        for (i=0;i<samples;i++)
          buf16[i*2+1] = (lyd->buf[i+samples] * 32767 * LYD_VOICE_VOLUME);
    }
  lyd->sample_no += samples;

  clean_silent (lyd, active);
  slist_free (active);

  UNLOCK ();

  lyd->previous_samples = samples;
  for (i = 0; i < LYD_MAX_CBS; i++)
    if (lyd->post_cb[i])
      lyd->post_cb[i](lyd, samples, stream, stream2, lyd->post_cb_data[i]);
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
      LydVM *voice = iter->data;
      if (voice->tag == tag)
        {
          iter = lyd->voices = slist_remove (lyd->voices, voice);
          lyd_vm_free (voice);
          goto again;
        }
    }
  UNLOCK ();
}

void lyd_voice_kill (Lyd *lyd,
                     LydVM *voice)
{
  LOCK ();
  if (slist_find (lyd->voices, voice))
    {
      lyd->voices = slist_remove (lyd->voices, voice);
      lyd_vm_free (voice);
    }
  UNLOCK ();
}

void
lyd_voice_release (Lyd      *lyd,
                   LydVM *voice)
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
  voice = lyd_vm_create (lyd, program);
  voice->sample_rate = lyd->sample_rate;
  voice->i_sample_rate = 1.0/lyd->sample_rate;
  voice->tag = tag;
  lyd->voices = slist_prepend (lyd->voices, voice);
  return voice;
}

LydVoice *lyd_voice_new (Lyd        *lyd,
                         LydProgram *program,
                         double      delay,
                         int         tag)
{
  LydVoice *voice;
  LOCK ();
  voice = lyd_voice_new_unlocked (lyd, program, tag);
  voice->sample = - (delay * lyd->sample_rate);
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
                          
void lyd_init_lookup_tables (void);

Lyd * lyd_new (void)
{
  Lyd *lyd = g_new0 (Lyd, 1);
  pthread_mutex_init(&lyd->mutex, NULL);
  lyd->max_active = 2000;
  lyd_init_lookup_tables ();
  return lyd;
}

void lyd_set_sample_rate (Lyd *lyd, int sample_rate)
{
  lyd->sample_rate = sample_rate;
}

int lyd_get_sample_rate (Lyd *lyd)
{
  return lyd->sample_rate;
}

LydFormat lyd_get_format (Lyd *lyd)
{
  return lyd->format;
}

void lyd_set_format (Lyd *lyd, LydFormat format)
{
  lyd->format = format;
}

int lyd_dead;

static void
lyd_wave_free (LydWave *wave)
{
  g_free (wave->name);
  g_free (wave->data);
  g_free (wave);
}

void lyd_free (Lyd *lyd)
{
  int i;
  for (i = 0; i < LYD_MAX_WAVE; i++)
    if (lyd->wave[i])
      lyd_wave_free (lyd->wave[i]);

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
      LydOpState *state;
      /* the variable constants are stored as a sequence of nops at the
       * beginning of the program
       */
      for (state=voice->state; state->op == LYD_NOP; state = state->next)
        {
          if (STREQUAL(state->literal[1 * LYD_CHUNK], hash))
            {
              int k;
              for (k = 0; k < LYD_CHUNK; k++)
                state->literal[k] = value;/* could set the out directly,
                                             and make nops be true no-ops? */
              break;
            }
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
      LydOpState *state;
      for (state = voice->state; state->op == LYD_NOP; state = state->next)
        if (STREQUAL (state->literal[LYD_CHUNK * 1], param->param_name))
          {
            param->ptr = &(state->literal[0]);
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

static void lyd_voice_update_params (LydVoice *voice,
                                     int       samples)
{
  SList *paramlist;
  int j;

  for (j=0; j<samples; j++)
    {
      for (paramlist = voice->params;
           paramlist;
           paramlist = paramlist->next)
        {
          SList *i;
          LydParam *prev = NULL, *prev_prev = NULL;
          int freefirst = 0;

          for (i = paramlist->data; 
               i && LYD_PARAM (i->data)->sample_no < voice->lyd->sample_no + j;
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
                             ((voice->lyd->sample_no + j) - prev->sample_no)
                            /(curr->sample_no - prev->sample_no * 1.0);
          
              switch (curr->interpolation)
                {
                  case LYD_LINEAR:
                    curr->ptr[j] = prev->value * (1.0-dt) + curr->value * dt;
                    break;
                  case LYD_GAP:
                    curr->ptr[j] = 0.0;
                    break;
                  case LYD_STEP:
                    curr->ptr[j] = dt < 0.9999?prev->value:curr->value;
                    break;
                  case LYD_CUBIC:
                   {
                    LydParam *next = i->next?i->next->data:curr;

                    if (!prev_prev)
                      prev_prev = prev;
                    curr->ptr[j] = cubic (dt, prev_prev->value, prev->value,
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
}

int
lyd_add_pre_cb (Lyd *lyd,
                void (*cb)(Lyd *lyd, float elapsed, void *data),
                void *data)
{
  int i = 0;
  LOCK ();
  while (i<LYD_MAX_CBS && lyd->pre_cb[i] != NULL)
    i++;

  if (i<LYD_MAX_CBS)
    {
      lyd->pre_cb[i] = cb;
      lyd->pre_cb_data[i] = data;
      UNLOCK ();
      return i;
    }
  UNLOCK ();
  return -1;
}

int
lyd_add_post_cb     (Lyd *lyd,
                     void (*cb)(Lyd *lyd, int samples,
                                void *stream, void *stream2,
                                void *data),
                     void *data)
{
  int i = 0;
  LOCK ();
  while (i<LYD_MAX_CBS && lyd->post_cb[i] != NULL)
    i++;

  if (i<LYD_MAX_CBS)
    {
      lyd->post_cb[i] = cb;
      lyd->post_cb_data[i] = data;
      UNLOCK ();
      return i + LYD_MAX_CBS;
    }
  UNLOCK ();
}

void
lyd_remove_cb (Lyd *lyd, int id)
{
  LOCK ();
  if (id >=0 && id < LYD_MAX_CBS)
    lyd->pre_cb[id] = NULL;
  else {
    id-=LYD_MAX_CBS;
    if (id >=0 && id < LYD_MAX_CBS)
      lyd->post_cb[id] = NULL;
  }
  UNLOCK ();
}


void
lyd_load_wave (Lyd *lyd, const char *name,
              int  samples, int sample_rate, float *data)
{
  LydWave *wave;
  int i;
  LOCK ();
  wave = g_new0 (LydWave, 1);
  for (i = 0; i < LYD_MAX_WAVE; i++)
    {
      LydWave *p = lyd->wave[i];
      if (p && !strcmp(p->name, name))
        {
          lyd_wave_free (p);
          lyd->wave[i] = NULL;
          break;
        }
    }

  if (data == NULL || samples == 0)
    { 
      UNLOCK ();
      return;
    }

  wave->name = g_strdup (name);
  wave->data = g_malloc0 (sizeof (float) * samples);
  wave->samples = samples;
  wave->sample_rate = sample_rate;
  memcpy (wave->data, data, sizeof (float) * samples);
  for (i = 0; i < LYD_MAX_WAVE; i++)
    if (!lyd->wave[i])
      {
        lyd->wave[i] = wave;
        break;
      }

  UNLOCK ();
}

void
lyd_set_wave_handler (Lyd *lyd,
                     int (*wave_handler) (Lyd *lyd, const char *wavename,
                                          void *user_data),
                     void *user_data)
{
  LOCK ();
  lyd->wave_handler = wave_handler;
  lyd->wave_handler_data = user_data;
  UNLOCK ();
}

LydFilter  *lyd_filter_new      (Lyd *lyd, LydProgram *program)
{
  LydVM *filter;
  filter     = lyd_vm_create (lyd, program);
  filter->sample_rate = lyd->sample_rate;
  filter->i_sample_rate = 1.0/lyd->sample_rate;
  return filter;
}

void
lyd_filter_process (LydFilter *filter,
                    LydSample *input,
                    LydSample *output,
                    int        samples)
{
  int left = samples;
  int pos = 0;

  filter->sample_rate = filter->lyd->sample_rate;
  filter->i_sample_rate = 1.0/filter->lyd->sample_rate;
  if (input)
    {
      filter->input_buf = input;
      filter->input_pos = 0;
      filter->input_buf_len = samples;
    }

  while (left)
    {
      int i;
      LydSample *result = NULL;
      int chunk = LYD_CHUNK;
      if (chunk > left)
        chunk = left;
      left -= chunk;

      lyd_voice_update_params (filter, chunk);
      result = lyd_vm_compute (filter, chunk);
      for (i = 0; i< chunk; i++)
        output[pos+i] = result[i];
      pos += chunk;
    }
}

void lyd_filter_free (LydFilter *filter)
{
  lyd_vm_free (filter);
}
