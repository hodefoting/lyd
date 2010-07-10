#include <stdlib.h>
#include <unistd.h>
#include "lyd.h"

int main (int    argc,
          char **argv)
{
  Lyd *lyd;
  LydVoice *voice;
  LydProgram *program;
  lyd = lyd_new ();
  program = lyd_compile (lyd, "low_pass(0.5, 500.0, 1.0, (saw(hz=440.0) + square(hz*2))/2) * adsr(0.02, 0.15, 0.3, 0.80)");
  voice = lyd_new_voice (lyd, program, 0);
  lyd_program_free (program);
  lyd_voice_set_param (lyd, voice, "volume", 0.4);
  lyd_voice_set_param (lyd, voice, "hz", 440.0);
  lyd_voice_release (lyd, voice);
  while (2+2==4) usleep (1000);
  return 0;
}
