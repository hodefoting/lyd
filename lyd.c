#include <glib.h>
#include "core/lyd.h"
#include <stdlib.h>
#include <string.h>

#include "welcome.h"

gboolean lyd_audio_init   (Lyd         *lyd,
                           const gchar *driver);

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
  g_thread_init (NULL);
  lyd = lyd_new ();

  if (!lyd_audio_init (lyd, "auto"))
    {
      g_free (lyd);
      g_error ("failed to initialize lyd (audio output)\n");
    }

#ifdef HAVE_OSC
  lyd_osc_init (lyd);
#endif
#ifdef HAVE_ALSA
  lyd_midi_init (lyd);
#endif

  welcome (lyd);

  g_main_loop_run (g_main_loop_new (NULL, FALSE));
  return 0;
}
