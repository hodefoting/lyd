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

#include <lyd/lyd.h>
#include <unistd.h>
#include <stdlib.h>
#include <sndfile.h>

static int wave_handler (Lyd *lyd, const char *wavename, void *user_data);

int main (int    argc,
          char **argv)
{
  Lyd *lyd = lyd_new ();
  int  i;

  if (!argv[1])
    {
      printf ("Usage:\n");
      printf (" %s file.wav [file2.wav ..]\n", argv[0]);
      return -1;
    }

  if (!lyd_audio_init (lyd, "auto"))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      return -1;
    }

  lyd_set_wave_handler (lyd, wave_handler, NULL);

  while(1)
    {
      for (i = 0; i < 20; i++)
        {
          const char *wavpath = argv[(rand()%(argc-1)) + 1];
          LydProgram *instrument;
          LydVoice   *voice;
          char        code[1024];

          switch(rand()%9)
            {
              case 0: /* plain playback */
                sprintf (code, "wave('%s') * volume", wavpath);
                break;
              case 1:
                sprintf (code, "low_pass (0.4, 700.0, 1.0, wave('%s')) * volume", wavpath);
                break;
              case 2:
                sprintf (code, "low_pass (1.4, 700.0, 1.0, wave('%s')) * volume", wavpath);
                break;
              case 3:
                sprintf (code, "band_pass (1.4, 700.0, 1.0, wave('%s')) * volume", wavpath);
                break;
#define MIDDLE_C 261.6235565 
              case 4: /* 150% speed */
                sprintf (code, "wave('%s', %f) * volume", wavpath, MIDDLE_C*1.5);
                break;
              case 5: /* 66.67% speed */
                sprintf (code, "wave('%s', %f) * volume", wavpath, MIDDLE_C * 0.666);
                break;
              case 6: /* multi tap delay */
                sprintf (code, "reverb(0.2, 0.0.6, reverb(0.4, 0.1, wave('%s')))* volume", wavpath);
                break;
              case 7: /* tremolo */
                sprintf (code, "wave('%s', 261.6235565 + sin(5) * 10) * volume", wavpath);
                break;
              case 8: /* vibrato */
                sprintf (code, "wave('%s') * sin(4) * 2 * volume", wavpath);
                break;
              /* with band pass*/
            }

          instrument = lyd_compile (lyd, code);
          voice = lyd_voice_new (lyd, instrument, 0.0, 0);

          /* any variable (that is non-reserved keyword) in the source can be manipulated
           * in realtime like this: 
           */
          lyd_voice_set_param (lyd, voice, "volume", 1.5 * (rand()%511)/511.0);

          lyd_voice_set_delay (lyd, voice, 20.3 * (rand()%511)/511.0);

          /* for fire and forget voices, it is nice to set a time to live before release*/
          lyd_voice_set_duration (lyd, voice, 3.0);

          lyd_voice_set_position (lyd, voice, 2 * (rand()%511)/511.0 - 1);

          lyd_program_free (instrument);
        }
      printf ("queued a bunch of sounds\n");
      sleep (10);
    }
  lyd_free (lyd);
  return 0;
}


/* callback to load file on demand when compiling LydPrograms
 */
static int
wave_handler (Lyd *lyd, const char *wavename, void *user_data)
{
  SNDFILE *infile;
  SF_INFO  sfinfo;

  sfinfo.format = 0;
  if (!(infile = sf_open (wavename, SFM_READ, &sfinfo)))
    {
      float data[10];
      lyd_load_wave (lyd, wavename, 10, 400, data);
      printf ("failed to open file %s\n", wavename);
      sf_perror (NULL);
      return 1;
    }

  if (sfinfo.channels > 1)
    {
      printf ("too many channels\n");
      return 1;
    }
  {
    float *data = malloc (sfinfo.frames * sizeof (float));
    sf_read_float (infile, data, sfinfo.frames);
    lyd_load_wave (lyd, wavename, sfinfo.frames, sfinfo.samplerate, data);
    free (data);
    sf_close (infile);
    printf ("loaded %s\n", wavename);
  }
  return 0;
}
