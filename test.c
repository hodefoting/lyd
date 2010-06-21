#include <glib.h>
#include "lyd.h"
#include <stdlib.h>
#include <string.h>

//static int scale[]= {0,2,4,7,9};
//static int scale[5]={0-10,2-10,4-10,7-10,9-10};

Lyd *lyd;
gboolean hz_changer (gpointer voice)
{
  static float volume = 1.0;
  lyd_voice_set_param (lyd, voice, "volume", volume-=0.01);

  return TRUE;
}

void music (Lyd *lyd);
void test_instruments (Lyd *lyd);

gboolean lyd_audio_init   (Lyd       *lyd,
                             const gchar *driver);

#ifdef HAVE_OSC
void lyd_osc_init   (Lyd *lyd);
#endif

#ifdef HAVE_ALSA
void lyd_midi_init   (Lyd *lyd);
#endif

void test_parser (void);

int main (int    argc,
          char **argv)
{
  g_thread_init (NULL);
  //test_parser ();
  //return 0;
  lyd = lyd_new ();
  if (!lyd_audio_init (lyd, "auto"))
    {
      g_free (lyd);
      g_error ("failed to initialize lyd (audio output)\n");
    }

#ifdef HAVE_OSC
  lyd_osc_init (lyd);
#endif
#ifdef HAVE_ALSA
  lyd_midi_init (lyd);
#endif

  if (strstr (argv[0], "instruments"))
    {
      test_instruments (lyd);
    }
  else if (strstr (argv[0], "demo"))
    {
      music (lyd);
    }
  else if (strstr (argv[0], "random"))
    {
      //g_timeout_add (250, random_player, lyd);
    }
  else
   { 
     LydVoice   *voice;
     LydProgram *program;
    
     program = lyd_compile (lyd, "sin(hz=940) * adsr(0.02, 0.25, 0.9, 0.80) * volume");
     voice = lyd_new_voice (lyd, program, 0);
     lyd_voice_set_param (lyd, voice, "volume", 0.4);
     lyd_voice_set_param (lyd, voice, "foo",    0.4);
     lyd_voice_set_param (lyd, voice, "volume", 1.0);
     //lyd_voice_set_param (lyd, voice, "hz",     940.0);
     lyd_voice_release (lyd, voice);

      //g_timeout_add (50, hz_changer, voice);
   }

  g_main_loop_run (g_main_loop_new (NULL, FALSE));
  return 0;
}
