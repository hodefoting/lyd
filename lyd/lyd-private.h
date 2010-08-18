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

#ifndef __LYD_PRIVATE_H_
#define __LYD_PRIVATE_H_

#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include "lyd.h"

typedef struct _LydCommand LydCommand;
typedef float LydSample; /* global define for what type lyd computes with,
                            can be set to float or double, some mor jiggling
                            would be needed to support 32bit int directly */

#define LYD_MAX_ELEMENTS               80
#define LYD_MAX_VARIABLES              8
#define LYD_MAX_ARGS                   4
#define LYD_VOICE_VOLUME               0.05
#define LYD_MAX_REVERB_SIZE            48000
#define LYD_RELEASE_SILENCE_DAMPENING  0.001
#define LYD_RELEASE_THRESHOLD          0.01
#define LYD_MIN_RELASE_TIME_DIVISOR    100   /* computed as samplerate/this */

/* The opcodes of lyds virtual machine */
typedef enum
{
  LYD_NONE = 0,
  LYD_NOP, LYD_MIX, LYD_MIX3, LYD_MIX4,
  LYD_ADD, LYD_SUB, LYD_MUL, LYD_DIV, LYD_ABS, LYD_POW, LYD_NEG, LYD_SQRT,
  LYD_MOD,

  LYD_SIN, LYD_SAW, LYD_RAMP, LYD_SQUARE, LYD_PULSE, LYD_NOISE, LYD_ABSSIN, LYD_POSSIN, LYD_EVENSIN,

  LYD_ADSR, LYD_REVERB, LYD_CYCLE,

  LYD_LOW_PASS, LYD_HIGH_PASS, LYD_BAND_PASS, LYD_NOTCH, LYD_PEAK_EQ, 
  LYD_LOW_SHELF, LYD_HIGH_SELF  /* the filters must maintain internal order */
} LydOpCode;

struct _LydCommand
{
  LydOpCode op;                /* The operation to execute */
  float     arg[LYD_MAX_ARGS]; /* arguments to operation */
};

struct _LydProgram
{
  LydCommand commands[LYD_MAX_ELEMENTS];
};


typedef struct _LydCommandState 
{ LydOpCode  op;
  LydSample  out; 
  LydSample *arg[LYD_MAX_ARGS];
  LydSample  literal[LYD_MAX_ARGS];
  void      *data;
} LydCommandState;



/* cheapish "hashing to floating point number
 * of the first few chars in a string
 */
static inline float str2float (const char *str)
{
  float ret = 0.0;
  int i;
  if (!str)
    return 0.0;
  for (i=0;i<10 && str[i];i++) 
    {
      ret += ((tolower(str[i])-'a')/30.0)*((1<<i)/100.0);
    }
  return ret;
}

#define STREQUAL(str1,str2) (fabs((str1)-(str2))<0.0000001)

/*** lyd uses glib conveniences but tries to be independent ***/
#ifdef NIH

#undef g_new0
#undef G_UNLIKELY
/* XXX: the fake glib needs lock/unlock replacements */
//#define LOCK()   g_static_mutex_lock (&mutex)
//#define UNLOCK() g_static_mutex_unlock (&mutex)
//GStaticMutex mutex;

#define TRUE  1
#define FALSE 0
#define G_UNLIKELY(arg)  arg
#define g_malloc0(size)  calloc (1, size)
#define g_new0(type, n)  calloc (n, sizeof(type))
#define g_free(buf)      free (buf)
#define g_random_double_range(min,max) (((rand()%100000)/100000.0)*(max-min)+min)
#define g_strdup(a)        strdup(a)
#define g_ascii_strtod(a,b) strtod(a,b)

/* independent reimplementation of singly linked list, with same API as
 * GSList
 */
typedef struct _SList SList;
struct _SList {void *data;SList *next;};
static inline SList *slist_prepend (SList *list, void *data)
{
  SList *new_=g_new0(SList, 1);
  new_->next=list;
  new_->data=data;
  return new_;
}
static SList *slist_remove (SList *list, void *data)
{
  SList *iter, *prev = NULL;
  if (list->data == data)
    {
      prev = (void*)list->next;
      g_free (list);
      return prev;
    }
  for (iter = list; iter; iter = iter->next)
    if (iter->data == data)
      {
        prev->next = iter->next;
        g_free (iter);
        break;
      }
    else
      prev = iter;
  return list;
}
static inline void slist_free (SList *list)
{
  while (list)
    list = slist_remove (list, list->data);
}
static inline SList *slist_find (SList *list, void *data)
{
  for (;list;list=list->next)
    if (list->data == data)
      break;
  return list;
}


#else

//#include <glib.h>
#define LOCK()   g_static_mutex_lock (&mutex)
#define UNLOCK() g_static_mutex_unlock (&mutex)
GStaticMutex mutex;

#define SList GSList
#define slist_prepend(l, a) g_slist_prepend ((l), (a))
#define slist_remove(l, a)  g_slist_remove ((l), (a))
#define slist_find(l, a)    g_slist_find ((l), (a))
#define slist_free(l)       g_slist_free ((l))

#endif

struct _Lyd
{
  pthread_mutex_t mutex;
  int       sample_rate; /* sample rate */
  LydFormat format;      /* */

  unsigned int previous_samples; /* number of samples previously computed */
  unsigned long sample_no; /* counter for global sample no */
  SList    *voices;        /* list of currently playing voices */

  LydSample reverb;
  LydSample reverb_length;

  LydSample level;
  int       active;
  int       max_active;

  int       reverb_pos;

  LydSample *accbuf;
  int accbuf_len;

  LydSample reverb_old[2][LYD_MAX_REVERB_SIZE];
};

struct _LydVoice
{
  Lyd      *lyd;      /* backpointer to the lyd instance */
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
  void   *complete_data;             /* data for complete callback */
  
  int    tag;

  SList *params;        /* list of key-lists form params */
  LydCommandState state[]; /* instruction and working data should
                           stay close using this layout, note: variable
                           sized array
                         */
};

#ifdef NIH
#define LOCK()    pthread_mutex_lock(&lyd->mutex)
#define UNLOCK()  pthread_mutex_unlock(&lyd->mutex)
#endif


/* get duration of loaded midi file in seconds */
float        lyd_midi_get_duration (Lyd *lyd);
/* set loop positions (also enables looping)  */
void         lyd_midi_set_repeat (Lyd *lyd, float start, float end);

#endif
