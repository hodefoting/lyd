#ifndef __LYD_PRIVATE_H_
#define __LYD_PRIVATE_H_

#include <stdlib.h>
#include <ctype.h>
/* what we have is hard coded for now, simply comment one of these out to
 * disable that part
 */
#define HAVE_OSC 
#define HAVE_SDL
//#define HAVE_AO
#define HAVE_ALSA
#define HAVE_JACK

typedef struct _LydCommand LydCommand;

#define LYD_MAX_ELEMENTS 40
#define LYD_MAX_ARGS     4

/* constants for use in patch definitions */

/* These are the commands of lyds virtual machine: */
typedef enum
{
  LYD_NONE = 0,
  LYD_NOP, LYD_MIX, LYD_MIX3, LYD_MIX4,
  LYD_ADD, LYD_SUB, LYD_MUL, LYD_DIV, LYD_ABS, LYD_POW, LYD_NEG, LYD_SQRT,
  LYD_MOD,

  LYD_SIN, LYD_SAW, LYD_RAMP, LYD_SQUARE, LYD_PULSE, LYD_NOISE,

  LYD_ADSR, LYD_REVERB,

  LYD_LOW_PASS, LYD_HIGH_PASS, LYD_BAND_PASS, LYD_NOTCH, LYD_PEAK_EQ, 
  LYD_LOW_SHELF, LYD_HIGH_SELF  /* these must maintain internal order */
} LydOp;


struct _LydCommand
{
  LydOp   op;                /* The operation to execute */
  float   arg[LYD_MAX_ARGS]; /* arguments to operation */
};

struct _LydProgram
{
  LydCommand commands[LYD_MAX_ELEMENTS];
};

/* cheapish "hashing to floating point number
 * of the first few chars in a string
 */
static inline float str2float (const char *str)
{
  float ret = 0.0;
  int i;
  if (!str)
    return 0.0;
  for (i=0;i<10 && str[i];i++) /* we shouldnt be able to get much more than 6 unique chars out,..
                                * but we gamble and hope collisions are few, should
                                * perhaps add sanity checking to the compiler that warns on
                                * actual collisions?
                                */
    {
      ret += ((tolower(str[i])-'a')/30.0)*((1<<i)/100.0);
    }
  return ret;
}


/*** lyd uses glib conveniences but tries to be independent ***/
#ifdef NIH
/* XXX: the fake glib needs lock/unlock replacements */
#include <glib.h>
#undef g_new0
#undef G_UNLIKELY
#define LOCK()   g_static_mutex_lock (&mutex)
#define UNLOCK() g_static_mutex_unlock (&mutex)
GStaticMutex mutex;

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
  SList *new=g_new0(SList, 1);
  new->next=list;
  new->data=data;
  return new;
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

//#define LOCK()
//#define UNLOCK()


#else

#include <glib.h>
#define LOCK()   g_static_mutex_lock (&mutex)
#define UNLOCK() g_static_mutex_unlock (&mutex)
GStaticMutex mutex;

#define SList GSList
#define slist_prepend(l, a) g_slist_prepend((l), (a))
#define slist_remove(l, a) g_slist_remove((l), (a))
#define slist_find(l, a) g_slist_find((l), (a))
#define slist_free(l) g_slist_free((l))

#endif

#endif
