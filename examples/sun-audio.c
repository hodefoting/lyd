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

/* The output of this example can be sent directly to /dev/audio */

#include <lyd/lyd.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

static float midi2hz (int midinote)
{
  return (440.0 * pow (2,(midinote-69.0)/12.0)) * 2;
}

#define NOTE(time, duration, halfnote) do{                         \
  voice = lyd_voice_new (lyd, instrument, time, 0);                \
  lyd_voice_set_param (lyd, voice, "hz",     midi2hz(halfnote+69));\
  lyd_voice_set_duration (lyd, voice, duration);         }while(0);

static int scale[]={0,2,4,7,9,12,9,7,4,2,0};
static unsigned char linear2ulaw(int sample);

#define SAMPLES 1

int main (int    argc,
          char **argv)
{
  Lyd        *lyd = lyd_new ();
  LydVoice   *voice;
  LydProgram *instrument;
  short int   buf16[SAMPLES * 4];
  
  const char *code = "sin(hz=440.0) * adsr(0.5, 0.10, 0.4, 0.30)";
  int         i;

  lyd_set_sample_rate (lyd, 8000);
  lyd_set_format (lyd, LYD_s16S);

  instrument = lyd_compile (lyd, code);
  for (i = 0; i<16;i++)
    NOTE(i*0.5, 0.3, scale[i]);

  for (i=0; i< (8000.0/SAMPLES) * 5; i++) 
    {
      int j;
      lyd_synthesize (lyd, SAMPLES, buf16, buf16);

      for (j=0; j<SAMPLES; j++)
       {
        int val = (buf16[j*2+0] + buf16[j*2+1])/2;
        printf ("%c", linear2ulaw (val));
       }
    }

  lyd_program_free (instrument);
  lyd_free (lyd);
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


#undef NOTE
