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
//#include <glib.h>
#include <lyd/lyd.h>
#include <lyd/lyd-private.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALSA
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static snd_pcm_t *alsa_open(char *dev, int rate, int channels)
{
   snd_pcm_hw_params_t *hwp;
   snd_pcm_sw_params_t *swp;
   snd_pcm_t *h;
   int r;
   int dir;
   snd_pcm_uframes_t period_size_min;
   snd_pcm_uframes_t period_size_max;
   snd_pcm_uframes_t buffer_size_min;
   snd_pcm_uframes_t buffer_size_max;
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

   /* Configurue period */

   dir = 0;
   snd_pcm_hw_params_get_period_size_min(hwp, &period_size_min, &dir);
   dir = 0;
   snd_pcm_hw_params_get_period_size_max(hwp, &period_size_max, &dir);

   period_size = 1024;

   dir = 0;
   r = snd_pcm_hw_params_set_period_size_near(h, hwp, &period_size, &dir);

   if (r < 0) {
           fprintf(stderr, "audio: Unable to set period size %lu (%s)\n",
                   period_size, snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

   dir = 0;
   r = snd_pcm_hw_params_get_period_size(hwp, &period_size, &dir);

   if (r < 0) {
           fprintf(stderr, "audio: Unable to get period size (%s)\n",
                   snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

   /* Configure buffer size */

   snd_pcm_hw_params_get_buffer_size_min(hwp, &buffer_size_min);
   snd_pcm_hw_params_get_buffer_size_max(hwp, &buffer_size_max);
   buffer_size = period_size * 4;

   dir = 0;
   r = snd_pcm_hw_params_set_buffer_size_near(h, hwp, &buffer_size);

   if (r < 0) {
           fprintf(stderr, "audio: Unable to set buffer size %lu (%s)\n",
                   buffer_size, snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

   r = snd_pcm_hw_params_get_buffer_size(hwp, &buffer_size);

   if (r < 0) {
           fprintf(stderr, "audio: Unable to get buffer size (%s)\n",
                   snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

   /* write the hw params */
   r = snd_pcm_hw_params(h, hwp);

   if (r < 0) {
           fprintf(stderr, "audio: Unable to configure hardware parameters (%s)\n",
                   snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

   /*
    * Software parameters
    */

   swp = alloca(snd_pcm_sw_params_sizeof());
   memset(hwp, 0, snd_pcm_sw_params_sizeof());
   snd_pcm_sw_params_current(h, swp);

   r = snd_pcm_sw_params_set_avail_min(h, swp, period_size);

   if (r < 0) {
           fprintf(stderr, "audio: Unable to configure wakeup threshold (%s)\n",
                   snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

   snd_pcm_sw_params_set_start_threshold(h, swp, 0);

   if (r < 0) {
           fprintf(stderr, "audio: Unable to configure start threshold (%s)\n",
                   snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

   r = snd_pcm_sw_params(h, swp);

   if (r < 0) {
           fprintf(stderr, "audio: Cannot set soft parameters (%s)\n",
           snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

   r = snd_pcm_prepare(h);
   if (r < 0) {
           fprintf(stderr, "audio: Cannot prepare audio for playback (%s)\n",
           snd_strerror(r));
           snd_pcm_close(h);
           return NULL;
   }

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
    //pthread_mutex_lock(&alsa->mutex);
    //pthread_mutex_unlock(&alsa->mutex);

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
       lyd_synthesize (lyd, c, data, NULL);
       snd_pcm_writei(h, data, c);
    }
  }
  return NULL;
}

int
lyd_audio_init_alsa (Lyd *lyd)
{
  pthread_t tid;

  h = alsa_open("default", 44100, 2);

  //pthread_mutex_init(&alsa->mutex, NULL);
  if (!h) {
    fprintf(stderr, "Unable to open ALSA device (%d channels, %d Hz), dying\n",
            2, 44100);
    return 0;
  }

  pthread_create(&tid, NULL, alsa_audio_start, lyd);
  lyd_set_sample_rate (lyd, 44100);
  lyd_set_format (lyd, LYD_s16S);
  return 1;
}
#endif

#ifdef HAVE_SDL
#include <SDL.h>

static void
sdl_generate_audio (void    *data,
                    guchar  *buf,
                    int      len)
{
  int samples = len / 4; /* 16bit stereo = 4 bytes*/
  lyd_synthesize (data, samples, buf, NULL);
}

int
lyd_audio_init_sdl (Lyd *lyd)
{
  /* Open the audio device */
  SDL_AudioSpec *desired, *obtained;

  SDL_Init(SDL_INIT_AUDIO);
  desired = g_malloc(sizeof(SDL_AudioSpec));
  obtained = g_malloc(sizeof(SDL_AudioSpec));
  desired->freq=44100;
  desired->format=AUDIO_S16SYS;
  desired->channels=2;
  desired->samples=4096;
  desired->callback=sdl_generate_audio;
  desired->userdata=lyd;
  if ( SDL_OpenAudio(desired, obtained) < 0 ){
    g_warning ("Couldn't open audio: %s\n", SDL_GetError());
    return 0;
  }
  lyd_set_sample_rate (lyd, obtained->freq);
  lyd_set_format (lyd, LYD_s16S);

  SDL_PauseAudio(0);
  g_free(desired);
  g_free(obtained);
  return 1;
}
#endif


#ifdef HAVE_AO
#include <ao/ao.h>

static Lyd *ao_lyd = NULL;
static gboolean ao_generate_audio (gpointer data)
{
  /* XXX: needs a separate structure carrying both ao_play arg
   * and lyd
   */
  void *buf;
  int samples = 2048;
  buf = g_malloc ( samples * 4 );
  lyd_synthesize (ao_lyd, samples, buf, NULL);
  ao_play (data, buf, samples * 4);
  g_free (buf);
  return TRUE;
}

gboolean
lyd_audio_init_ao (Lyd *lyd)
{
  int driver_id;
  ao_device *device;
  ao_sample_format format;
  format.bits = 16;
  format.channels = 2;
  format.rate = 44100;
  format.byte_format = AO_FMT_NATIVE;
  format.matrix = "L,R";

  g_assert (!ao_lyd);
  ao_lyd = lyd;

  ao_initialize ();
  driver_id = ao_default_driver_id ();
  device = ao_open_live (driver_id, &format, NULL);
  if (!device)
    return FALSE;

  g_idle_add (ao_generate_audio, device);

  lyd_set_sample_rate (lyd, format.rate);
  lyd_set_format (lyd, LYD_s16S);

  return TRUE;
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
  guchar *buf;
  int samples = nframes;
  buf = jack_port_get_buffer (output_port, nframes);
  lyd_synthesize (jack_lyd, samples, buf, NULL);
  return 0;
}

gboolean
lyd_audio_init_jack (Lyd *lyd)
{
  const char **ports;
  int i;

  jack_lyd = lyd;
  if ((client = jack_client_open ("picolyd",
                                  JackNoStartServer, NULL)) == 0)
    {
      return FALSE;
    }
  jack_set_process_callback (client, jack_process, 0);
  output_port = jack_port_register (client, "lyd",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput, 0);

  if (jack_activate (client))
    {
      g_warning ("cannot activate jack client\n");
      return FALSE;
    }

  if ((ports = jack_get_ports (client, NULL, NULL,
                               JackPortIsPhysical|JackPortIsInput)) == NULL)
    {
      g_warning ("cannot find a physical capture port\n");
      return FALSE;
    }

  for (i = 0; ports[i]; i++)
    {
      /* brute force connect */
      jack_connect (client, jack_port_name (output_port), ports[i]);
    }
  free (ports);

  lyd_set_sample_rate (lyd, jack_get_sample_rate (client));
  lyd_set_format (lyd, LYD_f32);

  return TRUE;
}
#endif

int
lyd_audio_init (Lyd       *lyd,
                const char *driver)
{
  if (driver == NULL
   || strstr (driver, "none"))
    return TRUE;
  else if (strstr (driver, "auto"))
    {
#ifdef HAVE_JACK
       if (lyd_audio_init_jack (lyd))
         return TRUE;
#endif
#ifdef HAVE_ALSA
       if (lyd_audio_init_alsa (lyd))
         return TRUE;
#endif
#ifdef HAVE_SDL
       if (lyd_audio_init_sdl (lyd))
         return TRUE;
#endif
#ifdef HAVE_AO
       if (lyd_audio_init_ao (lyd))
         return TRUE;
#endif
       return FALSE;
    }
#ifdef HAVE_SDL
  else if (strstr (driver, "alsa"))
    return lyd_audio_init_alsa (lyd);
#endif
#ifdef HAVE_SDL
  else if (strstr (driver, "sdl"))
    return lyd_audio_init_sdl (lyd);
#endif
#ifdef HAVE_AO
  else if (strstr (driver, "ao"))
    return lyd_audio_init_ao (lyd);
#endif
#ifdef HAVE_JACK
  else if (strstr (driver, "jack"))
    return lyd_audio_init_jack (lyd);
#endif
  return FALSE;
}
