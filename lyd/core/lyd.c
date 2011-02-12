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
lyd_voice_release (Lyd   *lyd,
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
 * with "input(0) + (noise()-0.5) * (1./065536)"
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

void lyd_midi_iterate (Lyd *lyd, float elapsed);

#ifdef LYD_THREADED
void lyd_worker_threads_init (Lyd *lyd);
#endif

Lyd * lyd_new (void)
{
  Lyd *lyd = g_new0 (Lyd, 1);
  pthread_mutex_init(&lyd->mutex, NULL);
  pthread_mutex_init(&lyd->mmutex, NULL);
  lyd->max_active = 4000;
#ifdef LYD_EXTENDABLE
  lyd->last_op = LydLastOp;
#endif
  lyd_init_lookup_tables ();

#ifdef LYD_THREADED
  lyd_worker_threads_init (lyd);
#endif

  lyd_add_pre_cb (lyd, (void*)lyd_midi_iterate, NULL);
  lyd_set_sample_rate (lyd, 48000);
  lyd_set_voice_count (lyd, 5);

  /* this should be read from file,.. */
  lyd_add_op_program (lyd, "reverb", 1, lyd_compile (lyd,
                      "tapped_echo (tapped_delay (input(0),         "
                      "                           0.00297, 0.00371, "
                      "                           0.00411, 0.00437),"
                      "             0.009683, 0.003292)"));

  lyd_add_op_program (lyd, "remove_dc", 1, lyd_compile (lyd,
                      "high_pass (1, 20, 1, input(0))"));

  /*
  {
    LydProgram *program = lyd_compile (lyd, "low_pass (1, 15000, 1, input(0))");
    lyd_set_global_filter (lyd, program);
    lyd_program_free (program);
  }*/

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
  lyd_dead = 1;
  for (i = 0; i < LYD_MAX_WAVE; i++)
    if (lyd->wave[i])
      lyd_wave_free (lyd->wave[i]);

  /* XXX: shutdown properly */
  /* XXX: free per thread render bufs */
  /* XXX: free still active voices */
  /* XXX: free chunk pools */
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
  LOCK ();
  if (slist_find (lyd->voices, voice))
    {
      lyd_vm_set_param (voice, param, value);
    }
  UNLOCK ();
}

void lyd_voice_set_param_delayed (Lyd        *lyd,        LydVoice    *voice,
                                  const char *param_name, double       time,
                                  LydInterpolation interpolation,
                                  double      value)
{
  LOCK ();
  if (slist_find (lyd->voices, voice))
    {
      lyd_vm_set_param_delayed (voice, param_name, time, interpolation, value);
    }
  UNLOCK ();
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
lyd_add_post_cb (Lyd *lyd,
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
  return -1;
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

void
lyd_set_var_handler (Lyd *lyd,
                     void (*var_handler) (Lyd *lyd, const char *varname,
                                          double default_value,
                                          void *user_data),
                     void *user_data)
{
  LOCK ();
  lyd->var_handler = var_handler;
  lyd->var_handler_data = user_data;
  UNLOCK ();
}

void
lyd_set_voice_count (Lyd *lyd, int voice_count)
{
  lyd->voice_count = voice_count;
  lyd->i_voice_count = 1.0 / voice_count;
  lyd->level = 0.0;
}

int lyd_get_voice_count (Lyd *lyd)
{
  return lyd->voice_count;
}

#define POOL_SIZE   28
#define FULL_POOL   0xfffffff
typedef struct AllocPool
{
  char      *alloc;
  LydSample *mem;
  long       used; /* bitmask of used bufs */
} AllocPool;

LydSample *lyd_chunk_new (Lyd *lyd)
{
  AllocPool *pool;
  int no;
  pthread_mutex_lock (&lyd->mmutex);
  pool = lyd->chunk_pools?lyd->chunk_pools->data:NULL;
  if (!pool || pool->used == FULL_POOL)
    {
      SList *iter;
      for (iter = lyd->chunk_pools; iter; iter=iter->next)
        {
          pool = iter->data;
          if (pool->used != FULL_POOL)
            break;
          pool = NULL;
        }
      if (!pool)
        {
          pool = g_new0 (AllocPool, 1);
          lyd->chunk_pools = slist_prepend (lyd->chunk_pools, pool);
          pool->alloc = g_malloc0 (sizeof (LydSample) * LYD_CHUNK * POOL_SIZE + LYD_ALIGN * 2);
          /* align memory */
          pool->mem = (void*) (pool->alloc) + (LYD_ALIGN-((int)((char *)pool->alloc)) % LYD_ALIGN);
        }
    }
  for (no = 0; no < POOL_SIZE; no++)
    {
      int bitmask = 1 << no;
      if (!(pool->used & bitmask))
        {
          pool->used |= bitmask;
          pthread_mutex_unlock (&lyd->mmutex);
          return (LydSample*) &pool->mem[LYD_CHUNK * no];
        }
    }
  pthread_mutex_unlock (&lyd->mmutex);
  return NULL;
}

void lyd_chunk_free (Lyd *lyd, LydSample *chunk)
{
  SList *iter, *prev = NULL;
  pthread_mutex_lock (&lyd->mmutex);
  for (iter = lyd->chunk_pools; iter; prev = iter, iter = iter->next)
    {
      AllocPool *pool = iter->data;
      if ((char*)(chunk) - (char*)(pool->mem) <
          (int)POOL_SIZE * sizeof (LydSample) * LYD_CHUNK &&
          (char*)(chunk) >= (char*)(pool->mem))
        {
          int no = (int)((char*)(chunk) - (char*)(pool->mem)) / (LYD_CHUNK * sizeof (LydSample));

          if (pool->used & (1 << no))
            {
              pool->used ^= (1 << no);
              if (pool->used == 0 && 0)
                {
                  if (lyd->chunk_pools == iter)
                    lyd->chunk_pools = iter->next;
                  if (prev)
                    prev->next = iter->next;
                  g_free (pool->alloc);
                  g_free (pool);
                  iter->next = NULL;
                  slist_free (iter);
                  /* the pool with freed item should perhaps move to the start of
                   * the list to facilitate reuse as well as speeding up of
                   * subsequent frees, overkill with lyds access patterns though
                   */
                }
            }
          pthread_mutex_unlock (&lyd->mmutex);
          return;
        }
    }
  pthread_mutex_unlock (&lyd->mmutex);
}
#ifdef LYD_EXTENDABLE

static LydOpInfo *getinfo (Lyd *lyd, const char *name)
{
  LydOpInfo *info = NULL;
  SList *iter;
  for (iter =  lyd->op_info; iter; iter = iter->next)
    {
      LydOpInfo *i = iter->data;
      if (!strcmp (i->name, name))
        {
          info = i;
          g_free (info->name);
          if (info->program)
            lyd_program_free (info->program);
          info->process = NULL;
          info->init = NULL;
          info->free = NULL;
          break;
        }
    }
  if (!info)
    {
      info = g_new0 (LydOpInfo, 1);
      lyd->op_info = slist_prepend (lyd->op_info, info);
      info->op = lyd->last_op++;
    }
  return info;
}

void
lyd_add_op (Lyd *lyd, const char *name, int argc,
            void (*process) (LydVM *vm, LydOpState *state, int samples),
            void (*init)    (LydVM *vm, LydOpState *state),
            void (*free)    (LydVM *vm, LydOpState *state))
{
  LydOpInfo *info = getinfo (lyd, name);
  info->name = g_strdup (name);
  info->argc = argc;
  info->process = process;
  info->init = init;
  info->free = free;
}

void lyd_add_op_program (Lyd *lyd, const char *name, int argc,
                         LydProgram *program)
{
  LydOpInfo *info = getinfo (lyd, name);
  info->name = g_strdup (name);
  info->argc = argc;
  info->program = program;
}
#endif
