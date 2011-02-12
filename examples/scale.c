#include <lyd/lyd.h>
#include <stdio.h>  /* printf() */
#include <unistd.h> /* sleep() */

int main (int    argc, char **argv)
{
  Lyd        *lyd;
  LydVoice   *voice;
  LydProgram *instrument;
  int         i;

  const char *code = "saw(hz=440.0) * adsr(0.1, 0.2, 0.8, 0.50) * 2";
  float scale[]={261.63, 293.66, 329.63, 349.23, 392.0,
                 440.0,  493.88, 523.25, 493.88, 440.0,
                 392.0,  349.23, 329.63, 293.66, 261.0};

  lyd = lyd_new ();
  if (!lyd_audio_init (lyd, "auto"))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      return -1;
    }

  instrument = lyd_compile (lyd, code);
  for (i = 0; i<14;i++)
    {
      voice = lyd_voice_new (lyd, instrument, 0.3 * i, 0);
      lyd_voice_set_param (lyd, voice, "hz", scale[i]);
      lyd_voice_set_duration (lyd, voice, 0.2);
    }
  lyd_program_free (instrument);

  sleep (5);
  lyd_free (lyd);
  return 0;
}
