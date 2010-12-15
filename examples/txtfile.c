/*
 * Copyright (c) 2010 Simon Budig <simon@budig.de>
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <endian.h>

static unsigned char linear2ulaw(int sample);
static int           wav_write_header (FILE *stream, int num_samples);

static float midi2hz (int midinote)
{
  return (440.0 * pow (2,(midinote-69.0)/12.0));
}

#define INFILE_SIZE 8192
#define SAMPLES 1
#define SAMPLE_RATE 48000.0

int main (int    argc,
          char **argv)
{
  Lyd        *lyd = lyd_new ();
  LydVoice   *voice;
  LydProgram *instrument;
  FILE       *infile, *outfile = NULL;
  char        code[INFILE_SIZE] = "sin(hz=440.0) * adsr(0.5, 0.10, 0.4, 0.30)";
  int         i;
  double      duration, time, halfnote = 0;
  short int   buf16[SAMPLES * 4];

  if (argc < 3)
    {
      if (!lyd_audio_init (lyd, "auto"))
        {
          lyd_free (lyd);
          printf ("failed to initialize lyd (audio output)\n");
          return -1;
        }
    }
  else
    {
      if (strcmp (argv[2], "-"))
        outfile = fopen (argv[2], "w");
      else
        outfile = stdout;

      lyd_set_sample_rate (lyd, SAMPLE_RATE);
      lyd_set_format (lyd, LYD_s16S);
    }

  while (1)
    {
      if (argc > 1)
        {
          infile = fopen (argv[1], "r");
          fread (code, 1, INFILE_SIZE, infile);
          fclose (infile);
        }

      instrument = lyd_compile (lyd, code);
      if (instrument)
        {
          duration = 1.5;
          time = 0.0;
          voice = lyd_voice_new (lyd, instrument, 0, 0);
          lyd_voice_set_param (lyd, voice, "hz", midi2hz (halfnote + 69));
          lyd_voice_set_param (lyd, voice, "half", halfnote);
          lyd_voice_set_duration (lyd, voice, duration);
          lyd_voice_set_delay (lyd, voice, time);

          if (outfile)
            {
              wav_write_header (outfile, SAMPLE_RATE * 4);

              for (i=0; i < (SAMPLE_RATE/SAMPLES) * 4; i++)
                {
                  int j;
                  lyd_synthesize (lyd, SAMPLES, buf16, buf16);
                  for (j = 0; j < SAMPLES; j++)
                    {
                      int val = (buf16[j * 2 + 0] + buf16[j * 2 + 1]) / 2;
                      fprintf (outfile, "%c", linear2ulaw (val));
                    }
                }
            }
          lyd_program_free (instrument);
        }

      if (argc < 3)
        sleep (4);

      if (argc != 2)
        break;
    }

  lyd_free (lyd);
  return 0;
}

static int
wav_write_header (FILE *stream,
                  int   num_samples)
{
  int32_t i32;
  int16_t i16;

  fwrite ("RIFF", 4, 1, stream);
  i32 = htole32 (num_samples + 0x32);
  fwrite (&i32, sizeof (i32), 1, stream);
  fwrite ("WAVE", 4, 1, stream);
  fwrite ("fmt ", 4, 1, stream);
  i32 = htole32 (0x12);
  fwrite (&i32, sizeof (i32), 1, stream);
  /* ITU G.711 mu-law */
  i16 = htole16 (0x07);
  fwrite (&i16, sizeof (i16), 1, stream);
  /* num_channels */
  i16 = htole16 (0x01);
  fwrite (&i16, sizeof (i16), 1, stream);
  /* samples/second */
  i32 = htole32 (48000);
  fwrite (&i32, sizeof (i32), 1, stream);
  /* bytes/second */
  i32 = htole32 (48000);
  fwrite (&i32, sizeof (i32), 1, stream);
  /* block align (bytes/sample) */
  i16 = htole16 (0x01);
  fwrite (&i16, sizeof (i16), 1, stream);
  /* bits/sample */
  i16 = htole16 (0x08);
  fwrite (&i16, sizeof (i16), 1, stream);
  i16 = htole16 (0x00);
  fwrite (&i16, sizeof (i16), 1, stream);
  fwrite ("fact", 4, 1, stream);
  i32 = htole32 (0x04);
  fwrite (&i32, sizeof (i32), 1, stream);
  i32 = htole32 (num_samples);
  fwrite (&i32, sizeof (i32), 1, stream);
  fwrite ("data", 4, 1, stream);
  fwrite (&i32, sizeof (i32), 1, stream);
  i32 = htole32 (num_samples);

  return 0;
}

/* http://www.speech.cs.cmu.edu/comp.speech/Section2/Q2.7.html */

/*
 * ** This routine converts from linear to ulaw
 * **
 * ** Craig Reese: IDA/Supercomputing Research Center
 * ** Joe Campbell: Department of Defense
 * ** 29 September 1989
 * **
 * ** References:
 * ** 1) CCITT Recommendation G.711  (very difficult to follow)
 * ** 2) "A New Digital Technique for Implementation of Any
 * **     Continuous PCM Companding Law," Villeret, Michel,
 * **     et al. 1973 IEEE Int. Conf. on Communications, Vol 1,
 * **     1973, pg. 11.12-11.17
 * ** 3) MIL-STD-188-113,"Interoperability and Performance Standards
 * **     for Analog-to_Digital Conversion Techniques,"
 * **     17 February 1987
 * **
 * ** Input: Signed 16 bit linear sample
 * ** Output: 8 bit ulaw sample
 * */

#define ZEROTRAP    /* turn on the trap as per the MIL-STD */
#define BIAS 0x84   /* define the add-in bias for 16 bit samples */
#define CLIP 32635

static unsigned char
linear2ulaw(int sample)
{
  static int exp_lut[256] = { 0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,
  4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
  5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7};
  int sign, exponent, mantissa;
  unsigned char ulawbyte;

  /* Get the sample into sign-magnitude. */
  sign = (sample >> 8) & 0x80;          /* set aside the sign */
  if (sign != 0) sample = -sample;              /* get magnitude */
  if (sample > CLIP) sample = CLIP;             /* clip the magnitude */

  /* Convert from 16 bit linear to ulaw. */
  sample = sample + BIAS;
  exponent = exp_lut[(sample >> 7) & 0xFF];
  mantissa = (sample >> (exponent + 3)) & 0x0F;
  ulawbyte = ~(sign | (exponent << 4) | mantissa);
#ifdef ZEROTRAP
  if (ulawbyte == 0) ulawbyte = 0x02;   /* optional CCITT trap */
#endif

  return(ulawbyte);
}

