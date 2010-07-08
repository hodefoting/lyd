#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <string.h>
#include "lyd.h"

#if 1
/* an experiment to create a score writing language using 
 * just the c pre processor
 */

#define BEAT     3
#define INTERVAL 0.6

//static int scale[5]={0-10,2-10,4-10,7-10,9-10};
static float note2hz (double note)
{
  return (440.0 * pow (2,(note)/12.0));
}

static void lyd_note (Lyd  *lyd,
                      gchar *instrument,
                      float hz,
                      float volume,
                      float duration,
                      float position,
                      float pos,
                      int   tag,
                      void *complete,
                      void *complete_data)
{
  LydVoice *voice;
  LydProgram *program;
  program = lyd_compile (lyd, instrument);
  voice = lyd_new_voice (lyd, program, tag);
  lyd_voice_set_delay (lyd, voice, pos);
  lyd_voice_set_duration (lyd, voice, duration);
  lyd_voice_set_param (lyd, voice, "volume", volume);
  lyd_voice_set_param (lyd, voice, "hz", hz);
  lyd_voice_set_position (lyd, voice, position);
}

char *instruments[]=
{
  "saw(hz) * adsr(0.1,0.1,0.8,0.1) * volume",
  "sin(hz) * adsr(0.1,0.1,0.8,0.1) * volume",
  "square(hz) * adsr(0.1,0.1,0.8,0.1) * volume",
  "pulse(hz, 0.2) * adsr(0.1,0.1,0.8,0.1) * volume",
  "noise(hz) * adsr(0.1,0.1,0.8,0.1) * volume",
};

#define PREAMBLE \
  int tag = 0;  \
  gchar *instrument = ""; \
  float volume = 1.0; \
  void *    complete = NULL; \
  gpointer  complete_data = NULL; \
  float position = 0; \
  float pos = 0; \

#define p  pos += INTERVAL / 3;
#define P  pos += INTERVAL / 3 * 2;
#define NOTE(no, duration) lyd_note (lyd, instrument, note2hz(scale[no]), volume, INTERVAL * duration, position, pos, tag, complete, complete_data);P;
#define O(n) NOTE(n, 0.7);P;
#define o(n) NOTE(n, 0.4);P;
#define a o(0)
#define b o(1)
#define c o(2)
#define d o(3)
#define e o(4)
#define A o(0)
#define B o(1)
#define C o(2)
#define D o(3)
#define E o(4)

#define dp1 pos += INTERVAL ;
#define d1  instrument=instruments[3];NOTE(0, 0.1);dp1;
#define dp2 pos += INTERVAL / 2;
#define d2  instrument=instruments[3];NOTE(0, 0.2);dp2;
#define d3  instrument=instruments[3];NOTE(0, 0.2);dp2;
#define reset pos = 0;position = 0;volume = 1.0;

static int scale[]= {0,2,4,7,9};

void foo (void)
{
  exit (0);
}

void bar (gpointer data)
{
  //g_print ("played %i            \n", GPOINTER_TO_INT (data));
}


void test_instruments (Lyd *lyd)
{
  PREAMBLE
  int i;

  volume = 1.0;
  
  reset;
  i = 0;
  P P P
  for (i=0; i < 10; i++)
    {
      instrument = instruments[i];
      complete = bar;
      complete_data = GINT_TO_POINTER (i);
      A P P P P
    }
  complete = foo;
  c

  return;
}

void music (Lyd *lyd)
{
  PREAMBLE
  int i;

  volume = 0.5;
  
  /* drums */
  volume = 0.5;
  reset;

  reset; for (i=0; i< 10; i++) { d1 d2 d2 }
  reset; for (i=0; i< 10; i++) { dp2 d3 dp2 }
  //return;
  /* piano */
  instrument = instruments[0]; volume = 0.4;
#define _ P
  reset  _ _ _ 
  A c D p A c P P P P P P P P p C P c A C P c A p D A P 
  c A p p c p E p A c p p A D p b A b p

  /* strings */
  instrument = "saw(hz) * adsr(0,0,1.0,0) * volume"; 
  volume = 0.8;
  //vol_fade (0.5, 1.2);
  reset  P P P P P P
  A b c p p b P E p
  C P c A A c p p A 
  A c P P P P P P P 
  p p p A c D p c p 
  E p p D 
  
  //complete = foo;
  A 
}

/*******************/

//instrument 4 "sin(hz)"
//c d d e @ c d d e |
//freq
#endif

gboolean lyd_audio_init (Lyd         *lyd,
                         const gchar *driver);

int main (int    argc,
          char **argv)
{
  Lyd *lyd;
  g_thread_init (NULL);
  lyd = lyd_new ();
  if (!lyd_audio_init (lyd, "auto"))
    {
      g_free (lyd);
      g_error ("failed to initialize lyd (audio output)\n");
    }
  music (lyd);
  g_main_loop_run (g_main_loop_new (NULL, FALSE));
  return 0;
}
