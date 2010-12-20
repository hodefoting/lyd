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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GLIB
#include <glib.h>
#endif
#include <lyd/lyd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif

#include <unistd.h>

#include "welcome.h"

static int wave_handler (Lyd *lyd, const char *wavename, void *user_data);
static void init_wav_write (Lyd *lyd, const char *file);

static float scale[]=
  {261.63, 293.66, 329.63, 349.23, 392.0, 440.0, 493.88, 523.25, 587.33, 622.25};

//static float scale[]=
//  {277.18, 311.13, 369.99, 415.30, 466.16, 544.37, 622.25, 739.99, 830.61 };


int lyd_audio_init   (Lyd        *lyd,
                      const char *driver);

#ifdef HAVE_OSC
void lyd_osc_init   (Lyd *lyd);
#endif

#ifdef HAVE_ALSA
void lyd_midi_init   (Lyd *lyd);
#endif

int main (int    argc,
          char **argv)
{
  Lyd *lyd;
#ifdef HAVE_GLIB
  g_thread_init (NULL);
#endif
  lyd = lyd_new ();

  if (!lyd_audio_init (lyd, "auto"))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      exit (1);
    }

#ifdef HAVE_SNDFILE
  lyd_set_wave_handler (lyd, wave_handler, NULL);
#endif

#ifdef HAVE_OSC
  lyd_osc_init (lyd);
#endif
#ifdef HAVE_ALSA
  lyd_midi_init (lyd);
#endif

  {
    int scale_walker = 0;
    const char *source = NULL;
    float duration = 0.3;
    float delay = 0.0;
    argv++;
    while (*argv)
      {
        if (!strcmp (*argv, "-h"))
          {
            printf ("Usage: lyd [-i 'instrument source' [-w delay]] [-o output.wav]\n"
                    "-i specifies an instrument to compile, this instrument becomes instrument 0 in the midi set of the ALSA midi device."
#ifdef HAVE_SNDFILE
                    "-o in addition to playback write output to a WAV file\n"
#endif
                    "-w creates a random scale walker with the specified delay playing the instrument\n"
                    );
            return 0;
          }
        else if (!strcmp (*argv, "-i"))
          {
            argv++;
            if (!*argv)
              {
                fprintf (stderr, "expected argument to -i\n");
                return -1;
              }
            source = *argv;
          }
        else if (!strcmp (*argv, "-d"))
          {
            argv++;
            if (!*argv)
              {
                fprintf (stderr, "expected argument to -d\n");
                return -1;
              }
            duration = atof(*argv);
          }
        else if (!strcmp (*argv, "-w"))
          {
            argv++;
            if (!*argv)
              {
                fprintf (stderr, "expected delay argument to -w\n");
                return -1;
              }
            delay = atof(*argv);
            scale_walker = 1;
          }
#ifdef HAVE_SNDFILE
        else if (!strcmp (*argv, "-o"))
          {
            argv++;
            if (!*argv)
              {
                fprintf (stderr, "expected argument to -o\n");
                return -1;
              }
            init_wav_write (lyd, *argv);
          }
#endif
        else
          {
            printf ("unhandled commandline arg: %s\n", *argv);
          }
        argv++;
      }
    if (source)
      {
        LydProgram *program;
        LydVoice   *voice;

        program = lyd_compile (lyd, source);
        printf ("Compiling: %s\n", source);
        if (program)
          {
            /* set this source as a midi patch */
            lyd_set_patch (lyd, 0, source);

            if (scale_walker)
              {
                int i;
                int spos = 0;
                int sdir = 1;
                for (i=0; i < 122; i++)
                  {
                    if (rand()%256 > 16)
                      {
                        voice = lyd_voice_new (lyd, program, i * delay, 0);
                        lyd_voice_set_param (lyd, voice, "volume", 1.0);
                        lyd_voice_set_param (lyd, voice, "hz", scale[spos]);
                        lyd_voice_set_duration (lyd, voice, duration);
                        lyd_voice_set_position (lyd, voice, 0.0);
                      }

                    if (rand()%256 > 128)
                     sdir *= -1;
                    spos += sdir;

                    if (spos >= 
                        (sizeof (scale)/sizeof(scale[0]))  ||
                        spos < 0)
                      {
                        sdir *= -1;
                        spos += sdir;
                        spos += sdir;
                      }
                  }
              }
            else
              if (0){
                voice = lyd_voice_new (lyd, program, 0.0, 0);
                lyd_voice_set_param (lyd, voice, "volume", 1.0);
                lyd_voice_set_param (lyd, voice, "hz", 440.0);
                lyd_voice_set_duration (lyd, voice, duration);
                lyd_voice_set_position (lyd, voice, 0.0);
              }
          }
        else
          return;
      }
    else 
      welcome (lyd);
  }

  for (;;)
    sleep (1);
  return 0;
}

#ifdef HAVE_SNDFILE
/* callback to load file on demand when compiling LydPrograms
 * using libsndfile
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

static void render_cb (Lyd *lyd, int len, void *stream, void *stream2, void *data);

static SF_INFO info;
static SNDFILE *sndfile;

static void flushit(void)
{
  sf_write_sync (sndfile);
  sf_close (sndfile);
  fprintf (stderr, "@flush\n");
}

static void init_wav_write (Lyd *lyd, const char *file)
{
  info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  info.channels = 2;
  info.samplerate = lyd_get_sample_rate (lyd);
  sndfile = sf_open (file, SFM_WRITE, &info);
  if (sndfile == NULL) {
      fprintf (stderr, "Error opening output sound file '%s': %s\n",
               file, sf_strerror(sndfile));
      return;
  }
  lyd_add_post_cb (lyd, render_cb, NULL);
  atexit (flushit);
}

static void render_cb (Lyd *lyd, int len, void *stream, void *stream2, void *data)
{
  sf_writef_short (sndfile, stream, len);
}
#endif
