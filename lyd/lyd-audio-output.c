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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lyd/lyd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
extern int lyd_dead;
#ifdef HAVE_ALSA
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* This is what my sound card gives me when using ALSA directly,
 * and it is what gives the best performance when using pulseaudio.
 */
#define LYD_DESIRED_PERIOD_SIZE 1024

static snd_pcm_t *alsa_open(char *dev, int rate, int channels)
{
   snd_pcm_hw_params_t *hwp;
   snd_pcm_sw_params_t *swp;
   snd_pcm_t *h;
   int r;
   int dir;
   snd_pcm_uframes_t period_size_min;
   snd_pcm_uframes_t period_size_max;
   snd_pcm_uframes_t period_size;
   snd_pcm_uframes_t buffer_size;

   if ((r = snd_pcm_open(&h, dev, SND_PCM_STREAM_PLAYBACK, 0) < 0))
           return NULL;

   hwp = alloca(snd_pcm_hw_params_sizeof());
   memset(hwp, 0, snd_pcm_hw_params_sizeof());
   snd_pcm_hw_params_any(h, hwp);

   snd_pcm_hw_params_set_access(h, hwp, SND_PCM_ACCESS_RW_INTERLEAVED);
   snd_pcm_hw_params_set_format(h, hwp, SND_PCM_FORMAT_S16_LE);
   snd_pcm_hw_params_set_rate(h, hwp, rate, 0);
   snd_pcm_hw_params_set_channels(h, hwp, channels);

   dir = 0;
   snd_pcm_hw_params_get_period_size_min(hwp, &period_size_min, &dir);
   dir = 0;
   snd_pcm_hw_params_get_period_size_max(hwp, &period_size_max, &dir);

   period_size = LYD_DESIRED_PERIOD_SIZE;

   dir = 0;
   r = snd_pcm_hw_params_set_period_size_near(h, hwp, &period_size, &dir);
   r = snd_pcm_hw_params_get_period_size(hwp, &period_size, &dir);
   buffer_size = period_size * 4;
   r = snd_pcm_hw_params_set_buffer_size_near(h, hwp, &buffer_size);
   r = snd_pcm_hw_params(h, hwp);
   swp = alloca(snd_pcm_sw_params_sizeof());
   memset(hwp, 0, snd_pcm_sw_params_sizeof());
   snd_pcm_sw_params_current(h, swp);
   r = snd_pcm_sw_params_set_avail_min(h, swp, period_size);
   snd_pcm_sw_params_set_start_threshold(h, swp, 0);
   r = snd_pcm_sw_params(h, swp);
   r = snd_pcm_prepare(h);

   return h;
}

static  snd_pcm_t *h = NULL;
static void *alsa_audio_start(void *aux)
{
  Lyd *lyd = aux;
  int c;
  void *data = NULL;
  int data_len = 0;

  for (;;) {
    if (lyd_dead)
      return NULL;

    c = snd_pcm_wait(h, 1000);

    if (c >= 0)
            c = snd_pcm_avail_update(h);

    if (c == -EPIPE)
            snd_pcm_prepare(h);

    if (c > 0){
       if (!data || data_len < c)
         {
           if (data) free (data);
           data = malloc (8 * (c + 512));
           data_len = c + 512;
         }
       if (lyd_dead)
         return NULL;
       lyd_synthesize (lyd, c, data, NULL);
       snd_pcm_writei(h, data, c);
    } else {
      if (getenv("LYD_FATAL_UNDERRUNS"))
        {
          printf ("dying XXxx need to add API for this debug\n");
          //printf ("%i", lyd->active);
          exit(0);
        }
      //fprintf (stderr, "alsa underun %d voices\n", lyd->active);
      //exit(0);
    }
  }
  return NULL;
}

int
lyd_audio_init_alsa (Lyd *lyd)
{
  pthread_t tid;

  h = alsa_open("default", 48000, 2);
  if (!h) {
    fprintf(stderr, "Unable to open ALSA device (%d channels, %d Hz), dying\n",
            2, 48000);
    return 0;
  }

  pthread_create(&tid, NULL, alsa_audio_start, lyd);
  lyd_set_sample_rate (lyd, 48000);
  lyd_set_format (lyd, LYD_s16S);
  return 1;
}
#endif

#ifdef HAVE_JACK
#include <jack/jack.h>
#include <jack/transport.h>

static jack_client_t *client = NULL;
static jack_port_t *output_port = NULL;
static Lyd *jack_lyd = NULL;

int jack_process (jack_nframes_t nframes,
                  void          *arg)
{
  unsigned char *buf;
  int samples = nframes;
  buf = jack_port_get_buffer (output_port, nframes);
  lyd_synthesize (jack_lyd, samples, buf, NULL);
  return 0;
}

int
lyd_audio_init_jack (Lyd *lyd)
{
  const char **ports;
  int i;

  jack_lyd = lyd;
  if ((client = jack_client_open ("picolyd",
                                  JackNoStartServer, NULL)) == 0)
    {
      return 0;
    }
  jack_set_process_callback (client, jack_process, 0);
  output_port = jack_port_register (client, "lyd",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput, 0);

  if (jack_activate (client))
    {
      fprintf (stderr, "cannot activate jack client\n");
      return 0;
    }

  if ((ports = jack_get_ports (client, NULL, NULL,
                               JackPortIsPhysical|JackPortIsInput)) == NULL)
    {
      fprintf (stderr, "cannot find a physical capture port\n");
      return 0;
    }

  for (i = 0; ports[i]; i++)
    {
      /* brute force connect */
      jack_connect (client, jack_port_name (output_port), ports[i]);
    }
  free (ports);

  lyd_set_sample_rate (lyd, jack_get_sample_rate (client));
  lyd_set_format (lyd, LYD_f32);

  return 1;
}
#endif

int
lyd_audio_init (Lyd       *lyd,
                const char *driver)
{
  if (driver == NULL
   || strstr (driver, "none"))
    return 1;
  else if (strstr (driver, "auto"))
    {
#ifdef HAVE_JACK
       if (lyd_audio_init_jack (lyd))
         return 1;
#endif
#ifdef HAVE_ALSA
       if (lyd_audio_init_alsa (lyd))
         return 1;
#endif
    }
#ifdef HAVE_ALSA
  else if (strstr (driver, "alsa"))
    return lyd_audio_init_alsa (lyd);
#endif
#ifdef HAVE_JACK
  else if (strstr (driver, "jack"))
    return lyd_audio_init_jack (lyd);
#endif
  return 0;
}
