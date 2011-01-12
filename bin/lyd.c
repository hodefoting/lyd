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
#include <math.h>

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

static void program_play_score (Lyd *lyd, LydProgram *program,
                                const char *score, double duration);

static void scale_walk (Lyd *lyd, LydProgram *program, float delay, float duration);

#define MAX_INSTRUMENTS 64

int main (int    argc,
          char **argv)
{
  Lyd *lyd;
  int nosound = 0;
#ifdef HAVE_GLIB
  g_thread_init (NULL);
#endif
  lyd = lyd_new ();

  lyd_set_sample_rate (lyd, 44100);

  {
    int scale_walker = 0;
    const char *source[MAX_INSTRUMENTS] = {NULL,};
    const char *score[MAX_INSTRUMENTS] = {NULL,};
    float duration = 0.3;
    float delay = 0.0;
    argv++;
    while (*argv)
      {
        if (!strcmp (*argv, "-h"))
          {
            printf ("Usage: lyd [-s score] [-i 'instrument source' [-w delay]] [-o output.wav]\n"
                    "-s score specifies a tune to play, in a subset of ABC\n"
                    "-i specifies an instrument to compile, this instrument becomes instrument 0 in the midi set of the ALSA midi device."
#ifdef HAVE_SNDFILE
                    "-o in addition to playback write output to a WAV file\n"
#endif
                    "-nosound do not attempt to initialize audio output\n"
                    "-w creates a random scale walker with the specified delay playing the instrument\n"
                    );
            return 0;
          }
        else if (!strcmp (*argv, "-nosound"))
          {
            nosound = 1;
          }
        else if (!strcmp (*argv, "-i"))
          {
            argv++;
            if (!*argv)
              {
                fprintf (stderr, "expected argument to -i\n");
                return -1;
              }
            source[0] = *argv;
          }
        else if (!strcmp (*argv, "-s"))
          {
            argv++;
            if (!*argv)
              {
                fprintf (stderr, "expected argument to -s\n");
                return -1;
              }
            score[0] = *argv;
          }
        else if (strstr(*argv, "-i") == *argv)
          {
            int no = atoi(*argv+2);
            argv++;
            if (no>MAX_INSTRUMENTS || no<0)
              no = 0;
            if (!*argv)
              {
                fprintf (stderr, "expected argument to -i\n");
                return -1;
              }
            source[no] = *argv;
          }
        else if (strstr(*argv, "-s") == *argv)
          {
            int no = atoi(*argv+2);
            if (no>MAX_INSTRUMENTS || no<0)
              no = 0;
            argv++;
            if (!*argv)
              {
                fprintf (stderr, "expected argument to -s#\n");
                return -1;
              }
            score[no] = *argv;
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

  if (nosound)
    {
      lyd_set_sample_rate (lyd, 44100);
      lyd_set_format (lyd, LYD_s16S);
    }
  else
    {
      if (!lyd_audio_init (lyd, "auto"))
        {
          lyd_free (lyd);
          printf ("failed to initialize lyd (audio output)\n");
          exit (1);
        }
    }


#ifdef HAVE_OSC
  lyd_osc_init (lyd);
#endif
#ifdef HAVE_ALSA
  lyd_midi_init (lyd);
#endif

    if (source[0])
      {
        LydProgram *program;
        LydVoice   *voice;
        int trackno=0;

#ifdef HAVE_SNDFILE
        lyd_set_wave_handler (lyd, wave_handler, NULL);
#endif

        for (trackno = 0; source[trackno]; trackno++)
          {
            program = lyd_compile (lyd, source[trackno]);
            fprintf (stderr, "Compiling: %s\n", source[trackno]);
            if (program)
              {
                /* set this source as a midi patch */
                lyd_set_patch (lyd, 0, source[trackno]);

                if (score[trackno])
                  {
                    program_play_score (lyd, program, score[trackno], duration);
                  }
                else if (scale_walker)
                  {
                    scale_walk (lyd, program, duration, delay);
                  }
              }
            else
              {
                /* hand back compile error */
                return 0;
              }
          }
      }
    else
      {
        welcome (lyd);
#ifdef HAVE_SNDFILE
        lyd_set_wave_handler (lyd, wave_handler, NULL);
#endif
      }
  }

  if (nosound)
    {
      int buf[1000];
      for (;;)
        lyd_synthesize (lyd, 1000, buf, NULL);
      exit(0);
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

/** abc score playback **/

static int abcscale[]={0,2,4,5,7,9, 11};

static float midi2hz (int midinote)
{
    return (440.0 * pow (2,(midinote-69.0)/12.0));
}

static LydVM *last_voice = NULL;

static void abc_flush (Lyd *lyd, LydProgram *program,
                       double *position, double duration,
                       int *note, int *octave, int *accidental,
                       int *gotnominator, int *nominator, int *denominator,
                       int *gotnote, int *gotrest)
{
  if (*gotrest)
    {
      *position += (duration * *nominator) / *denominator;
      *note = 0;
      *nominator = 1;
      *denominator = 1;
      *gotnominator = 0;
      *octave = 0;
      *accidental = 0;
      *gotnote = 0;
      *gotrest = 0;
    }
  if (*gotnote)
    {
      int res;
      LydVoice *voice;
      res = abcscale[((*note) + 7 * 20)%7];
      if (*note >= 7)
        *octave += 1;
      if (*octave)
        res += *octave * 12;
      res+= *accidental;
      res += 60 - 12;

      voice = lyd_voice_new (lyd, program, *position, 0);
      lyd_voice_set_param (lyd, voice, "volume", 1.0);
      lyd_voice_set_param (lyd, voice, "hz", midi2hz(res));

      lyd_voice_set_duration (lyd, voice, (duration * *nominator) / *denominator);
      lyd_voice_set_position (lyd, voice, 0.0);

      *position += (duration * *nominator) / *denominator;

      *note = 0;
      *nominator = 1;
      *denominator = 1;
      *gotnominator = 0;
      *octave = 0;
      *accidental = 0;
      *gotnote = 0;
      *gotrest = 0;

      last_voice = voice;
    }
}

static int tracks = 0;
static void completed (void *data)
{
  tracks--;
  if (tracks <= 0)
    {
      exit(0);
    }
}

static void program_play_score (Lyd *lyd, LydProgram *program,
                                const char *score, double duration)
{
  const char *p;
  int octave = 0;
  int note = 0;
  int gotnote = 0;
  int gotrest = 0;
  int accidental = 0;
  int nominator = 1;
  int denominator = 1;
  int gotnominator = 0;
  int instring = 0;
  double position = 0.0;
  char notes[] = "CDEFGABcdefgab";

#define FLUSH abc_flush(lyd, program, &position, duration, &note, &octave, &accidental, &gotnominator, &nominator, &denominator, &gotnote, &gotrest);

  for (p = score; *p; p++)
    {
      if (instring)
        {
          printf ("skipping %c\n", *p);
          if (*p=='"')
            instring=0;
        }
      else switch (*p)
        {
          case '.': /* not in ABC but can be convenient */
          case 'z':
            FLUSH
            gotrest = 1;
            break;
          case '"':
            instring=1;
            break;
          case '^':
            FLUSH
            accidental ++;
            break;
          case '_':
            FLUSH
            accidental --;
            break;
          case '\'':
            octave++;
            break;
          case ',':
            octave--;
            break;
          case '/':
            gotnominator = 1;
            denominator *= 2;
            break;
          /* XXX: extend to deal with floating point and numbers >9,
           * should perhaps also add * so the full expanded form is N*1.1/1.0 */
          case '0': case '1': case '2': case '3': case '4':
          case '5': case '6': case '7': case '8': case '9':
            if (!gotnominator)
              {
                nominator = *p - '0';
                gotnominator = 1;
              }
            else
              {
                denominator = *p - '0';
              }
            break;
          default:
            if (strchr (notes, *p))
              {
                FLUSH
                note = strchr(notes, *p)-notes;
                gotnote = 1;
                break;
              }
          case ' ':
            break;
        }
    }
  FLUSH
#undef FLUSH

  tracks++;
  if (last_voice)
    lyd_vm_set_complete_cb (last_voice, completed, NULL);
}


static void scale_walk (Lyd *lyd, LydProgram *program, float delay, float duration)
{
int i;
int spos = 0;
int sdir = 1;
for (i=0; i < 122; i++)
  {
    if (rand()%256 > 16)
      {
        LydVoice *voice;
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
