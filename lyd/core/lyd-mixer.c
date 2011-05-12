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
#include <unistd.h>
#include "core/lyd-private.h"

/* we include the voice directly to make the mixing and the vm 
 * a single compilation unit
 */

static void   lyd_prepare_buffer (Lyd *lyd, int samples);
static void   lyd_pre_cb (Lyd *lyd, int samples);
static SList *lyd_queue_voices (Lyd *lyd, int samples);
static void   lyd_thread_render_voices (Lyd *lyd, int samples, int thread_no);
static void   lyd_collapse_threads (Lyd *lyd, int samples);
static void   lyd_apply_global_filter (Lyd *lyd, int samples);
static void   lyd_scale_volume (Lyd *lyd, int samples);
static void   lyd_write_to_output (Lyd *lyd, int samples,
                                   void *stream, void *stream2);
static void   lyd_kill_silent_voices (Lyd *lyd, SList *active);
static void   lyd_kill_excessive_voices (Lyd *lyd, SList *active);
static void   lyd_post_cb (Lyd *lyd, int samples, void *stream, void *stream2);
void lyd_worker_threads_init (Lyd *lyd);

long
lyd_synthesize (Lyd  *lyd,
                int   samples,
                void *stream,
                void *stream2)
{
  SList *active = NULL;
  int i;

  /* we do this here to ensure that the locking is done by the right thread */
#ifdef LYD_THREADED
  lyd_worker_threads_init (lyd);
#endif

  lyd_prepare_buffer (lyd, samples);
  lyd_pre_cb (lyd, samples);

  LOCK ();

  active = lyd_queue_voices (lyd, samples);

#ifndef LYD_THREADED
  lyd_thread_render_voices (lyd, samples, 0);
#else
  for (i = 1; i < lyd->threads; i++)
    {
      lyd->pending_data[i]=1;
      pthread_mutex_unlock (&lyd->tmutex[i]);
      pthread_cond_signal (&lyd->tcond[i]);
    }

  lyd_thread_render_voices (lyd, samples, 0);

  for (i = 1; i < lyd->threads; i++)
    {
      pthread_mutex_lock (&lyd->tmutex[i]);
      while (lyd->pending_data[i])
        {
          pthread_cond_wait (&lyd->tcond[i], &lyd->tmutex[i]);
        }
    }
  lyd_collapse_threads (lyd, samples);
#endif

  lyd_apply_global_filter (lyd, samples);

  lyd_scale_volume (lyd, samples);

  lyd_write_to_output (lyd, samples, stream, stream2);
  lyd_kill_silent_voices (lyd, active);
  lyd_kill_excessive_voices (lyd, active);

  slist_free (active);
  lyd->sample_no += samples;
  UNLOCK ();

  lyd_post_cb (lyd, samples, stream, stream2);

  return lyd->sample_no;
}

#ifdef LYD_THREADED
typedef struct ThreadData {Lyd *lyd; int thread_no;} ThreadData;

static void *render_thread (void *aux)
{
  ThreadData *tdata = aux;
  Lyd *lyd = tdata->lyd;
  int thread_no = tdata->thread_no;
  g_free (aux);

  for (;;)
    {
      pthread_mutex_lock(&lyd->tmutex[thread_no]);

      while (!lyd->pending_data[thread_no])
        pthread_cond_wait(&lyd->tcond[thread_no], &lyd->tmutex[thread_no]);

      lyd_thread_render_voices (lyd, lyd->tsamples, thread_no);
      lyd->pending_data[thread_no]=0;
      pthread_mutex_unlock(&lyd->tmutex[thread_no]);
      pthread_cond_signal (&lyd->tcond[thread_no]);
    }
  return NULL;
}

static int lyd_get_num_cores (void)
{
/* XXX: works only for Linux, Solaris, AIX */
  return sysconf (_SC_NPROCESSORS_ONLN);
}

void lyd_worker_threads_init (Lyd *lyd)
{
  static int done = 0;
  int i;
  if (done)
    return;

  lyd->threads = lyd_get_num_cores ();
  if (lyd->threads > LYD_MAX_THREADS)
    lyd->threads = LYD_MAX_THREADS;

  done = 1;
  for (i = 1;i < lyd->threads; i++)
    {
      ThreadData *tdata = g_new0 (ThreadData, 1);
      tdata->lyd = lyd;
      tdata->thread_no = i;
      pthread_mutex_init (&lyd->tmutex[i], NULL);
      pthread_cond_init  (&lyd->tcond[i], NULL);
      pthread_mutex_lock(&lyd->tmutex[i]);
      pthread_create (&lyd->tids[i], NULL, render_thread, tdata);
    }
}
#endif

static void lyd_prepare_buffer (Lyd *lyd, int samples)
{
  int i;
  if (lyd->buf_len < samples || lyd->buf[0] == NULL)
    {
#ifdef LYD_THREADED
      for (i = 0; i < lyd->threads; i++)
#else
      i=0;
#endif
        {
          if (lyd->buf[i])
            g_free (lyd->buf[i]);
          lyd->buf[i] = g_malloc0 (sizeof (LydSample) * samples * 2);
        }
      lyd->buf_len = samples;
    }
#ifdef LYD_THREADED
  for (i = 0; i < lyd->threads; i++)
#else
  i=0;
#endif
    memset (lyd->buf[i], 0, sizeof (LydSample) * samples * 2);
}

static void lyd_pre_cb (Lyd *lyd, int samples)
{
  float elapsed = lyd->previous_samples/(1.0 * lyd->sample_rate);
  int i;
  for (i = 0; i < LYD_MAX_CBS; i++)
    if (lyd->pre_cb[i])
      lyd->pre_cb[i](lyd, elapsed, lyd->pre_cb_data[i]);
  lyd->previous_samples = samples;
}

static void lyd_voice_release_handling (Lyd   *lyd,
                                        LydVM *voice,
                                        int    first_sample,
                                        int    samples,
                                        LydSample * __restrict__ result)
{
  int i;
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
}

static void
lyd_voice_spatialize (Lyd   *lyd,
                      LydVM *voice,
                      int    thread_no,
                      int    first_sample,
                      int    samples,
                      int    tot_samples,
                      int    pos,
                      LydSample * result)
{
  int i;
  /* simple stereo spatialization */
  if (voice->position == 0.0)
    {
      for (i=first_sample;i<samples;i++)
        lyd->buf[thread_no][pos+i] += result[i-first_sample];
      for (i=first_sample;i<samples;i++)
        lyd->buf[thread_no][pos+i+tot_samples] += result[i-first_sample];
    }
  else if (voice->position > 0.0)
    {
      for (i=first_sample;i<samples;i++)
        lyd->buf[thread_no][pos+i] += result[i-first_sample] * (1.0-voice->position);
      for (i=first_sample;i<samples;i++)
        lyd->buf[thread_no][pos+i+tot_samples] += result[i-first_sample];
    }
  else
    {
      for (i=first_sample;i<samples;i++)
        lyd->buf[thread_no][pos+i] += result[i - first_sample];
      for (i=first_sample;i<samples;i++)
        lyd->buf[thread_no][pos+i+tot_samples] += result[i - first_sample] * (1.0+voice->position);
    }
}

static void
lyd_synthesize_voice (Lyd   *lyd,
                      LydVM *voice,
                      int    thread_no,
                      int    samples,
                      int    tot_samples,
                      int    pos)
{
  LydSample * __restrict__ result = NULL;
  int first_sample = voice->sample<0?-voice->sample:0;

  /* blanking accumulation buffer... */
  assert (samples <= LYD_CHUNK);

  /* skip voices that are not yet playing,
   * they will be active later in the current batch */
  if (voice->sample + samples <0)
    {
      voice->sample += samples;
      return;
    }

  voice->sample += first_sample;

  voice->sample++;
  lyd_vm_update_params (voice, samples - first_sample);

  /* result is a direct pointer to the results in the last processing chain */
  result = lyd_vm_compute (voice, samples - first_sample);
  lyd_voice_spatialize (lyd, voice, thread_no, first_sample, samples, tot_samples, pos, result);
  voice->sample--;

  lyd_voice_release_handling  (lyd, voice, first_sample, samples, result);
}


static SList *lyd_queue_voices (Lyd *lyd, int samples)
{
  SList *active = NULL, *iter = NULL;
  int thread_no = 0;
#ifdef LYD_THREADED
  lyd->tsamples = samples;
#endif

  for (iter = lyd->voices; iter; iter=iter->next)
    {
      LydVM *voice = iter->data;
      if (voice->sample + samples >=0)
        {
          lyd->queued_voices[thread_no] = slist_prepend (lyd->queued_voices[thread_no], voice);
          active = slist_prepend (active, voice);
        }
      else
        voice->sample += samples;
#ifdef LYD_THREADED
      thread_no++;
      if (thread_no >= lyd->threads)
        thread_no = 0;
#endif
    }
  return active;
}


static void lyd_thread_render_voices (Lyd *lyd, int samples, int thread_no)
{
  SList *iter = NULL;

  if (!lyd->queued_voices[thread_no])
    return;
  for (iter = lyd->queued_voices[thread_no]; iter; iter = iter->next)
    {
      LydVM *voice = iter->data;
      int left = samples;
      int pos = 0;
      while (left > 0) /* break processing up in sizes of size LYD_CHUNK or smaller */
        {
          int chunk = LYD_CHUNK;
          if (chunk > left)
            chunk = left;
          left -= chunk;
          lyd_synthesize_voice (lyd, voice, thread_no, chunk, samples, pos);
          pos += chunk;
        }
    }
  slist_free (lyd->queued_voices[thread_no]);
  lyd->queued_voices[thread_no] = NULL;
}

#ifdef LYD_THREADED

static void lyd_collapse_threads (Lyd *lyd, int samples)
{
  int i;
  for (i = 1; i < lyd->threads; i++)
    {
      int j;
      for (j = 0; j < samples * 2; j++)
        lyd->buf[0][j] += lyd->buf[i][j];
    }
}
#endif

static void lyd_apply_global_filter (Lyd *lyd, int samples)
{
  LydSample *inputs[]={NULL};
  inputs[0] = lyd->buf[0];
  if (lyd->global_filter[0])
    lyd_filter_process (lyd->global_filter[0], inputs, 1, lyd->buf[0], samples);
  inputs[0] = lyd->buf[0] + samples;
  if (lyd->global_filter[1])
    lyd_filter_process (lyd->global_filter[1], inputs, 1, lyd->buf[0] + samples, samples);
}

static void lyd_scale_volume (Lyd *lyd, int samples)
{
  int i;
  float factor = lyd->i_voice_count;
  for (i=0;i<samples;i++)
    {
      LydSample value[2];
      value[0] = lyd->buf[0][i];
      value[1] = lyd->buf[0][i+samples];

      {
        LydSample level = fabs(value[0]);
        if (level > lyd->level)
          lyd->level = level;
        level = fabs(value[1]);
        if (level > lyd->level)
          lyd->level = level;
      }

      if (lyd->level > lyd->voice_count)
        {
          factor = 1.0/lyd->level;
          lyd->buf[0][i] *= factor;
          lyd->buf[0][i+samples] *= factor;
        }
      else
        {
          lyd->buf[0][i] *= factor;
          lyd->buf[0][i+samples] *= factor;
        }
    }
}

static void lyd_write_to_output (Lyd *lyd, int samples,
                                 void *stream, void *stream2)
{
  int i;
  LydSample * __restrict__ buf   = (void*)stream;
  LydSample * __restrict__ buf2  = (void*)stream2;
  short int * __restrict__ buf16 = (void*)stream;
  /* write from accumbuf into actual buffer */
  switch (lyd->format)
    {
      case LYD_f32:
        for (i=0;i<samples;i++)
          buf[i] = (lyd->buf[0][i] + lyd->buf[0][i+samples])/2;
        break;
      case LYD_f32S:
        for (i=0;i<samples;i++)
          buf[i] = lyd->buf[0][i];
        for (i=0;i<samples;i++)
          buf2[i] = lyd->buf[0][i+samples];
        break;
      case LYD_s16S:
        for (i=0;i<samples;i++)
          buf16[i*2]  = (lyd->buf[0][i] * 32767);
        for (i=0;i<samples;i++)
          buf16[i*2+1] = (lyd->buf[0][i+samples] * 32767);
    }
}

static void lyd_kill_silent_voices (Lyd *lyd, SList *active)
{
  SList *iter;
  lyd->active = 0;
  for (iter=active; iter; iter=iter->next)
    {                                  /* remove released and silent voices */
      LydVM *voice = iter->data;
      if (voice->released > LYD_RELEASE_MIN * voice->sample_rate
       && (voice->silence_max -
           voice->silence_min < LYD_RELEASE_THRESHOLD ||
           voice->released > voice->sample_rate * 30.0)
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
}

static void lyd_kill_excessive_voices (Lyd *lyd, SList *active)
{
  SList *iter;
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

static void lyd_post_cb (Lyd *lyd, int samples, void *stream, void *stream2)
{
  int i;
  for (i = 0; i < LYD_MAX_CBS; i++)
    if (lyd->post_cb[i])
      lyd->post_cb[i](lyd, samples, stream, stream2, lyd->post_cb_data[i]);
}
