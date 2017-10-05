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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int    argc,
          char **argv)
{
  Lyd    *lyd;
  long    length;
  char   *mididata;
  FILE   *file;
  int     i;

  lyd = lyd_new ();

  if (!argv[1])
    {
      argv[1]="/home/pippin/mid/popcorn.mid";
    }

  printf ("initing sound\n");
  if (!lyd_audio_init (lyd, "auto", NULL))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      return -1;
    }

  file = fopen (argv[1], "r");
  fseek (file, 0, SEEK_END);
  length = ftell (file);
  fseek (file, 0, SEEK_SET);
  mididata = malloc (length);
  if (fread (mididata, length, 1, file) != 1)
    {
      printf ("Failed to read midi data from %s\n", argv[1]);
      return -1;
    }

  else
    {
      printf ("Loaded %li bytes midi data from %s\n", length, argv[1]);
      fclose (file);
    }

  lyd_midi_load (lyd, (void*)mididata, length);
  lyd_midi_set_playing (lyd, 1);
  
  sleep (3);
  lyd_midi_set_playing (lyd, 0);

  sleep (2);
  lyd_midi_set_playing (lyd, 1);

  sleep (2);
  lyd_midi_seek (lyd, 0);
  lyd_midi_set_playing (lyd, 1);

  for (i=0; i<40;i ++)  /* speed up sequencer speed using midi meta tempo events */
    {
      static int tempo = 198436;
      unsigned char mididata[] = {0xff, 0x51, tempo >> 16, (tempo >> 8) & 0xff, tempo & 0xff};
      tempo /= 1.2;
      lyd_midi_out (lyd, mididata, 5);
      sleep (1);
    }
  lyd_midi_set_playing (lyd, 1);

  sleep (40);
  lyd_free (lyd);

  return 0;
}
