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

typedef struct _LydOp LydOp;

/* #define DEBUG_CLIPPING */

/* LYD_MAX_ARGC                        8    is defined in lyd-extend.h */
/* #define LYD_CHUNK                   128  defined in lyd-extend.h */
#define LYD_MAX_ELEMENTS               128 /* largest compiled program */
#define LYD_MAX_VARIABLES              16  /* maximum number of variables
                                              per voice */
#define LYD_MAX_CBS                    16  /* maximum number of registered
                                              callbacks */

#define LYD_MAX_WAVE                   128
#define LYD_MAX_REVERB_SIZE            48000
#define LYD_RELEASE_SILENCE_DAMPENING  0.001
#define LYD_RELEASE_MIN                2.0   /* kill voices after 1s of silence */
#define LYD_RELEASE_THRESHOLD          0.001  /* time average amplitude
                                              * at which released 
                                              * voices are killed
                                              */

#define LYD_ALIGN                      16    /* needed for tree-vectorize SIMD*/


/* The following features can be disabled by commenting them out */
#define LYD_EXTENDABLE     /* whether lyd_add_op is compiled (and support
                              for dynamic ops elsewhere */
#define LYD_THREADED       /* workload distribution in threads */
#define LYD_MAX_THREADS                4


typedef enum
{
#define LYD_OP(NAME, OP_CODE, ARGC, CODE, INIT, FREE, DOC, ARGDOC)  ,LYD_##OP_CODE
  LYD_NONE = 0
#include "lyd-ops.inc"
#undef LYD_OP
  ,LydLastOp
} LydOpCode;

#include "lyd.h"
#include "lyd-extend.h"

LydSample *lyd_chunk_new  (Lyd *lyd);
void       lyd_chunk_free (Lyd *lyd, LydSample *chunk);


struct _LydOp
{
  LydOpCode op;                /* The operation to execute */
  int       argc;              /* argument count */
  float     arg[LYD_MAX_ARGC]; /* arguments to operation */
};

#ifdef LYD_EXTENDABLE
struct _LydOpInfo  /* structure describing an extension to lyd
                    * language/vm 
                    */
{
  LydOpCode   op;       /* opcode */
  char       *name;     /* function name */
  int         argc;     /* argument count */

  /* Program to be executed, or ..  */
  LydProgram *program; 

  /* .. C implementation  */
  void (*process) (LydVM *vm, LydOpState *state, int samples);
  void (*init)    (LydVM *vm, LydOpState *state);
  void (*free)    (LydVM *vm, LydOpState *state);

};
#endif

struct _LydProgram
{
  LydOp commands[LYD_MAX_ELEMENTS];
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


/*** lyd is written with an independently recoded on demand
 * minimal glib like core for single linked lists and memory management,
 * these glibisms should probably be removed...
 */
#undef  g_new0
#undef  G_UNLIKELY
#define TRUE  1
#define FALSE 0
#define G_UNLIKELY(arg)     arg
#define g_malloc0(size)     calloc (1, size)
#define g_new0(type, n)     calloc (n, sizeof(type))
#define g_free(buf)         free (buf)
#define g_strdup(a)         strdup(a)
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

#define LOCK()    pthread_mutex_lock(&lyd->mutex)
#define UNLOCK()  pthread_mutex_unlock(&lyd->mutex)


typedef struct LydWave
{
  char  *name;
  int    samples;
  int    sample_rate;
  float *data;
} LydWave;

typedef struct LydMic
{
  char  *name;
  int    samples;      /* total number of samples accumulated */
  int    sample_rate;
  float *data;
  int    data_size;
  int    read_pos;
  int    write_pos;
} LydMic;

struct _Lyd
{
  pthread_mutex_t mutex;
  pthread_mutex_t mmutex;
  SList          *chunk_pools;

  int       sample_rate; /* sample rate */
  LydFormat format;      /* */

  unsigned int previous_samples; /* number of samples previously computed */
  unsigned long sample_no; /* counter for global sample no */
  SList    *voices;        /* list of currently playing voices */
#ifdef LYD_EXTENDABLE
  SList    *op_info;  /* it would be better if this lived in an array for
                         both built in and extended ops */
  int       last_op;
#endif

  void    (*var_handler) (Lyd        *lyd,
                          const char *var,
                          double      default_value,
                          void       *user_data);
  void     *var_handler_data;


  LydSample level;
  int       active;
  int       max_active;

  LydFilter *global_filter[2];  /* a global filter applied to all generated sound,
                                   one instance for each channel
                                */

  int   voice_count;
  float i_voice_count; /* 1.0/voice_count */

#ifdef LYD_THREADED
  int             threads;
  int             tsamples;
  pthread_t       tids[LYD_MAX_THREADS];
  pthread_mutex_t tmutex[LYD_MAX_THREADS];
  pthread_cond_t  tcond[LYD_MAX_THREADS];

  int             pending_data[LYD_MAX_THREADS];
  LydSample      *buf[LYD_MAX_THREADS];
  SList          *queued_voices[LYD_MAX_THREADS];
#else
  SList          *queued_voices[1];
  LydSample      *buf[1];
#endif
  int             buf_len;


  void (*pre_cb[LYD_MAX_CBS])(Lyd *lyd, float elapsed, void *data);
  void *pre_cb_data[LYD_MAX_CBS];
  void (*post_cb[LYD_MAX_CBS])(Lyd *lyd, int len, void *stream, void *stream2, void *data);
  void *post_cb_data[LYD_MAX_CBS];

  SList *extensions;

  //LydMic   *mic[LYD_MAX_MIC];

  LydWave  *wave[LYD_MAX_WAVE];
  int     (*wave_handler) (Lyd *lyd, const char *name,
                           void *user_data);
  void     *wave_handler_data;
};

/* LydVM is the datastructure representing a single voice,
 * it contains meta data for managing the voice as well
 * as the core execution engine.
 */
struct _LydVM
{
  Lyd      *lyd;      /* backpointer to the lyd instance */
  LydSample position; /* 0.0 center -1.0 left 1.0 right */
  LydSample duration; /* how long the sample should last */
  int       released; /* the number of samples we have been released, calling
                         voice_release increments this and starts the release
                         process */
  long  sample;       /* position, negative values means queued for playback,
                         controlled by lyd */
  int   sample_rate;
  float i_sample_rate; /* 1.0/sample_rate */

  LydSample silence_min; /* Silence detection */
  LydSample silence_max; /* (after release) */

  void  (*complete_cb)(void *data); /* callback and data when voice is done*/
  void   *complete_data;            /* data for complete callback */

  int        tag;
  LydSample *input_buf[LYD_MAX_ARGC];
  int        input_pos[LYD_MAX_ARGC];
  int        input_buf_len;

  SList      *params;  /* list of key-lists variable interpolation params */
  LydOpState *state;   /* points to immediately after the allocation
                          of LydVM (padded for alignment). */
};

/* get duration of loaded midi file in seconds */
float        lyd_midi_get_duration (Lyd *lyd);
/* set loop positions (also enables looping)  */
void         lyd_midi_set_repeat (Lyd *lyd, float start, float end);

void lyd_vm_update_params (LydVM *vm,
                           int    samples);
LydSample *
lyd_vm_compute (LydVM  *vm,
                int     samples);

void lyd_vm_free (LydVM *vm);
LydVM * lyd_vm_create (Lyd *lyd, LydProgram *program);


#endif
