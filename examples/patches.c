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
#include <math.h>

static float midi2hz (int midinote)
{
  return (440.0 * pow (2,(midinote-69.0)/12.0));
}

static int scale[]={0,2,4,7,9,12,9,7,4,2,0};

int main (int    argc,
          char **argv)
{
  Lyd        *lyd = lyd_new ();
  int         i;
  int         start = 0;
  int         end = 127;

  if (!lyd_audio_init (lyd, "auto"))
    {
      lyd_free (lyd);
      printf ("failed to initialize lyd (audio output)\n");
      return -1;
    }

  if (argv[1])
    {
      start = atoi (argv[1]);
      if (argv[2])
        end = atoi (argv[2]);
    }

  for (i = start; i<= end; i++)
    {
      int n;
      printf ("%i: %s\n", i, lyd_get_patch (lyd, i));
      for (n=0;n<5;n++)
        {
          lyd_note (lyd, i, midi2hz(49+scale[n]), 1.0, 0.3);
          usleep (500000);
        }
    }

  lyd_free (lyd);
  return 0;
}

#undef NOTE
