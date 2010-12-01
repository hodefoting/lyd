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

typedef struct _LydOp LydOp;
typedef float LydSample;
                         /* global define for what type lyd computes with,
                            can be set to float or double, some mor jiggling
                            would be needed to support 32bit int directly */

// #define DEBUG_CLIPPING

#define LYD_CHUNK                      128
#define LYD_MAX_ELEMENTS               128
#define LYD_MAX_ARGS                   4

#define LYD_MAX_VARIABLES              16
#define LYD_MAX_CBS                    16
#define LYD_VOICE_VOLUME               0.05f
#define LYD_MAX_WAVE                   128
#define LYD_MAX_REVERB_SIZE            48000
#define LYD_RELEASE_SILENCE_DAMPENING  0.001
#define LYD_RELEASE_THRESHOLD          0.01
#define LYD_MIN_RELASE_TIME_DIVISOR    100   /* computed as samplerate/this */

/* The opcodes of lyds virtual machine */
typedef enum
{
#define LYD_OP(NAME, OP_CODE, FOO, BAR, BAZ)  ,LYD_##OP_CODE
  LYD_NONE = 0
#include "lyd-ops.inc"
#undef LYD_OP
} LydOpCode;

struct _LydOp
{
  LydOpCode op;                /* The operation to execute */
  float     arg[LYD_MAX_ARGS]; /* arguments to operation */
};

struct _LydProgram
{
  LydOp commands[LYD_MAX_ELEMENTS];
};


typedef struct _LydOpState  LydOpState;

struct _LydOpState 
{
  LydSample   out [LYD_CHUNK] __attribute__ ((aligned(16)));  

  LydOpCode   op;                 /* 4 bytes */
  LydSample  *arg[LYD_MAX_ARGS];  /*16 bytes */
  void       *data;               /* 4 bytes */
  LydOpState *next;               /* 4 bytes */
  LydSample   phase;              /* 4 bytes */
  
  LydSample   literal[LYD_MAX_ARGS][LYD_CHUNK] __attribute__ ((aligned(16)));
};



/* "hashing to floating point number
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

/*** lyd is written with an independently recoded on demand
 * minimal glib like core for single linked lists and memory management
 */
#ifdef NIH

#undef g_new0
#undef G_UNLIKELY

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
static inline SList *slist_append (SList *list, void *data)
{
  SList *new_=g_new0(SList, 1);
  new_->data=data;
  if (list)
    {
      SList *last;
      for (last = list; last->next; last=last->next);
      last->next = new_;
      return list;
    }
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
static inline SList *slist_nth (SList *list, int no)
{
  while(no-- && list)
    list = list->next;
  return list;
}
static inline SList *slist_find (SList *list, void *data)
{
  for (;list;list=list->next)
    if (list->data == data)
      break;
  return list;
}


#else

#define LOCK()   g_static_mutex_lock (&mutex)
#define UNLOCK() g_static_mutex_unlock (&mutex)
GStaticMutex mutex;

#define SList GSList
#define slist_prepend(l, a) g_slist_prepend ((l), (a))
#define slist_remove(l, a)  g_slist_remove ((l), (a))
#define slist_find(l, a)    g_slist_find ((l), (a))
#define slist_free(l)       g_slist_free ((l))

#endif

typedef struct LydWave
{
  char  *name;
  int    samples;
  int    sample_rate;
  float *data;
} LydWave;

struct _Lyd
{
  pthread_mutex_t mutex;
  int       sample_rate; /* sample rate */
  LydFormat format;      /* */

  unsigned int previous_samples; /* number of samples previously computed */
  unsigned long sample_no; /* counter for global sample no */
  SList    *voices;        /* list of currently playing voices */
  LydWave  *wave[LYD_MAX_WAVE];

  int     (*wave_handler) (Lyd *lyd, const char *name,
                           void *user_data);
  void     *wave_handler_data;

  LydSample level;
  int       active;
  int       max_active;

  LydFilter *global_filter[2];  /* a global filter applied to all generated sound,
                                   one instance for each channel
                                */

  LydSample  *buf;
  int         buf_len;

  void (*cb[LYD_MAX_CBS])(Lyd *lyd, float elapsed, void *data);
  void *cb_data[LYD_MAX_CBS];
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
  float i_sample_rate; /* 1.0/sample_rate */

  LydSample silence_min; /* Silence detection */
  LydSample silence_max; /* (after release) */

  void  (*complete_cb)(void *data); /* callback and data when voice is done*/
  void   *complete_data;             /* data for complete callback */
  
  int    tag;
  LydSample *input_buf;
  int        input_pos;
  int        input_buf_len;


  SList *params;        /* list of key-lists form params */
  LydOpState *state;/* points to immediately (optionally with needed
                       padding after the LydVoice struct in the
                       same allocation */
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
