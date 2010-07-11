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
#include <glib.h>
#include <lyd/lyd.h>

#ifdef HAVE_SDL
#include <SDL.h>
//#include "kiss_fft.h"

//static kiss_fft_cfg cfg;

static void
sdl_generate_audio (void    *data,
                    guchar  *buf,
                    int      len)
{
  int samples = len / 4; /* 16bit stereo = 4 bytes*/
  lyd_synthesize (data, samples, buf, NULL);

  //kiss_fft (cfg, cx_in, cx_out);

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


  /* initialize kiss-fft */
  //cfg = kiss_fft_alloc (nfft, is_invertse_fft, 0, 0,);

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

gboolean
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
