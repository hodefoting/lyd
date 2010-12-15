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

/* This example shows how a single lyd core can be used to generate a
 * grayscale video signal. Vaiarbles X and Y are interpolated in the range
 * -1.0 .. 1.0 and ready for use in expressions run by the code, in the
 *  output -1.0 is mapped to black and 1.0 is mapped to white, values
 *  outside the range wraps around the pallette.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <lyd/lyd.h>
#include <SDL.h>

static char *examples[]={
    "",
    "mix(sin(5),   sin(256 * 4))/2 + sin(6/256) + noise() * (ramp(1)/2)",
    "ramp(256)^2 + ramp(1)^2", /* perfect sync using only oscillators */
    "reverb(1.8,0.24531,low_pass(1,10000, 1, ((y^2+x^2) * 30 * sin(0.008)) % 0.2))",

    "low_pass(1, 20000 * (ramp(1) + 1), 1, mix(square(5)-sin(0.001), square(256 * 5 + sin(6/256) * triangle(4)*10))/2) + (ramp(1)/2)",


    "low_pass(1,12000, 1, cycle(0.01 * sin(0.05), "
                                "((y^2+x^2) * 30 * sin(0.005)) % 0.5,"
                                "(14 * sqrt(x^2+y^2) - 0.5) + noise() * 0.2,"
                                "(4 * sqrt(x^2+y^2) - 0.5) + noise() * 0.2,"
    "sin(256)^2 + sin(1)^2," /* perfect sync using only oscillators */
                                "(x*(y-0.6)*40),"
                                "sin(pi)"
                                ")"
  " + noise() * 0.5  * sin(pi^2))",

"0.75*low_pass(1,6000, 1, ((y^2+x^2) * 64 * sin(0.01)) % 0.5))",

 NULL
};

static long get_ticks (void); /* utility function for measuring wall time */

/* defines controlling the dimensions of framebuffer/periods for x,y variables
 */
#define WIDTH     256
#define HEIGHT    256
#define scanlines HEIGHT
#define SIZE      (WIDTH*HEIGHT)

float outbuf[SIZE];

static void
init_sdl (void)
{
  static int inited = 0;

  if (!inited)
    {
      inited = 1;

      if (SDL_Init (SDL_INIT_VIDEO) < 0)
        {
          fprintf (stderr, "Unable to init SDL: %s\n", SDL_GetError ());
          return;
        }
      atexit (SDL_Quit);
      SDL_EnableUNICODE (1);
    }
}

int main (int    argc,
          char **argv)
{
  Lyd        *lyd;
  LydFilter  *filter;
  LydProgram *instrument;
  SDL_Surface *screen;

  const char *code = NULL;

  int         i;
  long tick_start;

  if (argv[1])
    {
      int no;
      if((no = atoi(argv[1])))
        {
          for (i = 0; i<=no && examples[i]; i++)
            if (i == no)
              {
                code = examples[i];
                printf ("@ay\n");
                break;
              }
        }
      else
        {
          code = argv[1];
        }
    }
  else
    {
      printf ("Usage: %s <\"program code to run\"|integer>\n"
              "if an integer is passed one of the example programs will be used\n", argv[0]);
      code = examples[1];
    }
  if (!code)
    return 0;

  init_sdl ();
  lyd = lyd_new ();

  screen = SDL_SetVideoMode (WIDTH, HEIGHT, 32, SDL_SWSURFACE);

  lyd_set_sample_rate (lyd, SIZE); /* XXX: should have a dummy sample rate */

  printf ("{%s}\n", code);
  instrument = lyd_compile (lyd, code);
  if (!instrument)
    return 0;
  filter = lyd_filter_new (lyd, instrument);

  float rowstride = 1.0/scanlines;   /* rowstride in seconds */

#define PX2TIME(x,y)    (y * 1) + (x * rowstride) /* converts a pixel
                                                       coordinate to time */
  for (;;)
    {
      int j;
      int *pixels = screen->pixels;

      /* create */
#define PX 0.00001
#define PX2 0.01
      lyd_vm_set_param_delayed (filter, "y", PX2TIME(0, 0)+PX2,   LYD_LINEAR, -1.0);
      lyd_vm_set_param_delayed (filter, "y", PX2TIME(1.0, 1.0)-PX2, LYD_LINEAR, 1.0);

      for (j = 0;j<scanlines; j++)
        {
          lyd_vm_set_param_delayed (filter, "x", PX2TIME(0, (1.0 * j/scanlines))+PX, LYD_LINEAR, -1.0);
          lyd_vm_set_param_delayed (filter, "x", PX2TIME(1, (1.0 * j/scanlines))-PX, LYD_LINEAR,  1.0);
        }

      tick_start = get_ticks ();

      lyd_filter_process (filter, NULL, 0, outbuf, SIZE);

      for (j = 0; j < SIZE; j++)
        {
          int val = (outbuf[j] + 1.0) * 128; /* +0.5 to make -0.5 negative numbers be the lower half of the signal */
          val = val & 0xff;
          pixels[j] = val + (val << 8) + (val << 16);
        }

      long nticks = get_ticks ();
      double elapsed = (nticks - tick_start)/1000000.0;
      fprintf (stderr, "\r%f fps  ", 1.0/elapsed);

      SDL_UpdateRect (screen, 0, 0, 0, 0);

      SDL_Event event;
      while (SDL_PollEvent  (&event))
        {
          switch (event.type)
            {
              case SDL_QUIT:
                exit (0);
            }
         }
    }

  lyd_program_free (instrument);

  lyd_free (lyd);
  return 0;
}


static struct timeval start_time;
#define usecs(time)    ((time.tv_sec - start_time.tv_sec) * 1000000 + time.tv_usec)

#include <sys/time.h>
static void
init_ticks (void)
{
  static int done = 0;

  if (done)
    return;
  done = 1;
  gettimeofday (&start_time, NULL);
}
static long get_ticks (void)
{
  struct timeval measure_time;
  init_ticks ();
  gettimeofday (&measure_time, NULL);
  return usecs (measure_time) - usecs (start_time);
}
