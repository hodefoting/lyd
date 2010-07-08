#include "core/lyd.h"

#ifdef HAVE_ALSA
#define MIDI_CHANNELS 16

#include <stdlib.h>
#include <glib.h>
#include <math.h>
#include <alsa/asoundlib.h>

/* should use instruments defined as strings */

#define NONE    "sin(hz) * adsr(0.0,0.0,0.8,0.2) * volume"
#define PIANO   "sin(hz) * adsr(0.0,0.0,0.8,0.2) * volume"
#define STRINGS "sin(hz) * adsr(0.0,0.0,0.8,0.2) * volume"
//#define PIANO   "saw(hz) * adsr(0.1,0.1,0.8,0.2) * volume"
//#define STRINGS "saw(hz) * adsr(0.1,0.1,0.8,0.2) * volume"
#define FOO STRINGS

/* Mapping of General MIDI instruments to lyd patches */
static char *midi_patches[128]=
  {
   /*  Piano:  */
   NONE, /*  0 Acoustic Grand Piano  */
   NONE, /*  1 Bright Acoustic Piano  */
   NONE, /*  2 Electric Grand Piano  */
   NONE, /*  3 Honky-tonk Piano  */
   NONE, /*  STRINGS Electric Piano 1  */
   NONE, /*  5 Electric Piano 2  */
   NONE, /*  6 Harpsichord  */
   NONE, /*  7 Clavinet  */
   /*  Chromatic Percussion:  */
   NONE, /*  8 Celesta  */
   NONE, /*  9  Glockenspiel  */
   NONE, /*  10 Music Box  */
   NONE, /*  11 Vibraphone  */
   NONE, /*  12 Marimba  */
   NONE, /*  13 Xylophone  */
   NONE, /*  1STRINGS Tubular Bells  */
   NONE, /*  15 Dulcimer  */
   /*  Organ:  */
   NONE, /*  16 Drawbar Organ  */
   NONE, /*  17 Percussive Organ  */
   NONE, /*  18 Rock Organ  */
   NONE, /*  19 Church Organ  */
   NONE, /*  20 Reed Organ  */
   NONE, /*  21 Accordion  */
   NONE, /*  22 Harmonica  */
   NONE, /*  23 Tango Accordion  */
   /*  Guitar:  */
   NONE, /*  2STRINGS Acoustic Guitar (nylon)  */
   NONE, /*  25 Acoustic Guitar (steel)  */
   NONE, /*  26 Electric Guitar (jazz)  */
   NONE, /*  27 Electric Guitar (clean)  */
   NONE, /*  28 Electric Guitar (muted)  */
   NONE, /*  29 Overdriven Guitar  */
   NONE, /*  30 Distortion Guitar  */
   NONE, /*  31 Guitar harmonics  */
   /*  Bass:  */
   NONE, /*  32 Acoustic Bass  */
   NONE, /*  33 Electric Bass (finger)  */
   NONE, /*  3STRINGS Electric Bass (pick)  */
   NONE, /*  35 Fretless Bass  */
   NONE, /*  36 Slap Bass 1  */
   NONE, /*  37 Slap Bass 2  */
   NONE, /*  38 Synth Bass 1  */
   NONE, /*  39 Synth Bass 2  */
   /*  strings:  */
   STRINGS, /*  4NONE violin  */
   STRINGS, /*  41 viola  */
   STRINGS, /*  42 cello  */
   STRINGS, /*  43 contrabass  */
   STRINGS, /*  44 tremolo strings  */
   STRINGS, /*  45 pizzicato strings  */
   STRINGS, /*  46 orchestral harp  */
   STRINGS, /*  47 timpani  */
   /*  Strings (continued):  */
   STRINGS, /*  48 String Ensemble 1  */
   NONE, /*  STRINGS9 String Ensemble 2  */
   STRINGS, /*  5NONE Synth Strings 1  */
   STRINGS, /*  51 Synth Strings 2  */
   NONE, /*  52 Choir Aahs  */
   NONE, /*  53 Voice Oohs  */
   NONE, /*  5STRINGS Synth Voice */
   NONE, /*  55 Orchestra Hit  */
   /*  Brass:  */
   NONE, /*  56 Trumpet  */
   STRINGS, /*  57 Trombone  */
   NONE, /*  58 Tuba  */
   NONE, /*  59 Muted Trumpet  */
   NONE, /*  60 French Horn  */
   NONE, /*  61 Brass Section  */
   NONE, /*  62 Lyd Brass 1  */
   NONE, /*  63 Lyd Brass 2  */
   /*  Reed:  */
   NONE, /*  6STRINGS Soprano Sax  */
   NONE, /*  65 Alto Sax  */
   NONE, /*  66 Tenor Sax  */
   NONE, /*  67 Baritone Sax  */
   NONE, /*  68 Oboe  */
   NONE, /*  69 English Horn  */
   STRINGS, /*  7NONE Bassoon  */
   STRINGS, /*  71 Clarinet  */
   /*  Pipe:  */
   STRINGS, /*  72 Piccolo  */
   STRINGS, /*  73 Flute  */
   NONE, /*  7STRINGS Recorder  */
   STRINGS, /*  75 Pan Flute  */
   NONE, /*  76 Blown Bottle  */
   NONE, /*  77 Shakuhachi  */
   NONE, /*  78 Whistle  */
   NONE, /*  79 Ocarina  */
   /*  Synth Lead:  */
   NONE, /*  80 Lead 1 (square)  */
   NONE, /*  81 Lead 2 (sawtooth)  */
   NONE, /*  82 Lead 3 (calliope)  */
   NONE, /*  83 Lead STRINGS (chiff)  */
   NONE, /*  8STRINGS Lead 5 (charang)  */
   NONE, /*  85 Lead 6 (voice)  */
   NONE, /*  86 Lead 7 (fifths)  */
   NONE, /*  87 Lead 8 (bass + lead)  */
   /*  Synth Pad:  */
   NONE, /*  88 Pad 1 (new age)  */
   FOO, /*  89 Pad 2 (warm)  */
   NONE, /*  90 Pad 3 (polylyd)  */
   NONE, /*  91 Pad STRINGS (choir)  */
   NONE, /*  92 Pad 5 (bowed)  */
   NONE, /*  93 Pad 6 (metallic)  */
   NONE, /*  9STRINGS Pad 7 (halo)  */
   NONE, /*  95 Pad 8 (sweep)  */
   /*  Synth Effects:  */
   NONE, /*  96 FX 1 (rain)  */
   NONE, /*  97 FX 2 (soundtrack)  */
   NONE, /*  98 FX 3 (crystal)  */
   NONE, /*  99  FX STRINGS (atmosphere)  */
   NONE, /*  100 FX 5 (brightness)  */
   NONE, /*  101 FX 6 (goblins)  */
   NONE, /*  102 FX 7 (echoes)  */
   NONE, /*  103 FX 8 (sci-fi)  */
   /*  Ethnic:  */
   NONE, /*  10STRINGS Sitar  */
   NONE, /*  105 Banjo  */
   NONE, /*  106 Shamisen  */
   NONE, /*  107 Koto  */
   NONE, /*  108 Kalimba  */
   NONE, /*  109 Bag pipe  */
   NONE, /*  110 Fiddle  */
   NONE, /*  111 Shanai  */
   /*  Percussive:  */
   NONE, /*  112 Tinkle Bell  */
   NONE, /*  113 Agogo  */
   NONE, /*  114 Steel Drums  */
   NONE, /*  115 Woodblock  */
   NONE, /*  116 Taiko Drum  */
   NONE, /*  117 Melodic Tom  */
   NONE, /*  118 Lyd Drum  */
   /*  Sound effects:  */
   NONE, /*  119 Reverse Cymbal  */
   NONE, /*  120 Guitar Fret Noise  */
   NONE, /*  121 Breath Noise  */
   NONE, /*  122 Seashore  */
   NONE, /*  123 Bird Tweet  */
   NONE, /*  124 Telephone Ring  */
   NONE, /*  125 Helicopter  */
   NONE, /*  126 Applause  */
   NONE  /*  127 Gunshot  */
};

static int         midi_mapping  [MIDI_CHANNELS] = {};
static float       midi_position [MIDI_CHANNELS] = {};
static float       midi_volume   [MIDI_CHANNELS] = {};
static GHashTable *voice_ht = NULL;

static float midi2hz (int midinote)
{
  return (440.0 * pow (2,(midinote-69.0)/12.0));
}

static void
midi_note_off (Lyd *lyd,
               gint   channel,
               gint   note,
               gint   velocity)
{
  int hashkey = channel * MIDI_CHANNELS + note;
  LydVoice *voice;
  if (!voice_ht)
    return;
  voice = g_hash_table_lookup (voice_ht, GINT_TO_POINTER (hashkey));
  if (voice)
    {
      lyd_voice_release (lyd, voice);
      g_hash_table_remove (voice_ht, GINT_TO_POINTER (hashkey));
    }
}

static void
midi_note_on (Lyd *lyd,
              gint   channel,
              gint   note,
              gint   velocity)
{
  int       hashkey = channel * MIDI_CHANNELS + note;
  LydVoice *voice;

  if (velocity == 0)
    {
      midi_note_off (lyd, channel, note, velocity);
      g_print ("velocity 0 note off\n");
      return;
    }

  lyd_kill (lyd, hashkey);

  {
    /* could keep this cached per channel, to avoid recompiling */
    LydProgram *program = lyd_compile (lyd, midi_patches[channel]);
    voice = lyd_new_voice (lyd, program, hashkey);

    lyd_voice_set_param (lyd, voice, "volume", (velocity / 127.0) * midi_volume[channel]);
    lyd_voice_set_param (lyd, voice, "hz", midi2hz (note));
    lyd_voice_set_position (lyd, voice, midi_position[channel]);

  }
#if XXX
  voice = lyd_note (lyd, midi_patches[midi_mapping[channel]],
                    midi2hz (note),
                    (velocity / 127.0) * midi_volume[channel],
                    5.0, /* 5 second "timeout" */
                    midi_position[channel], 0.0, hashkey, NULL, NULL);
#endif
  g_assert (voice);

  if (!voice_ht)
    {
      voice_ht = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
  g_hash_table_insert (voice_ht, GINT_TO_POINTER(hashkey), voice);
}

static void
midi_program (Lyd *lyd,
              gint   channel,
              gint   patch_no)
{
  g_print ("channel %d = %d\n", channel, patch_no);
  midi_mapping [channel] = 0;// patch_no;
}

static void
midi_controller (Lyd  *lyd,
                 gint  channel,
                 gint  param,
                 gint  value)
{
  switch (param)
    {
      case 0: /* bank select */
        break;
      case 7: /* channel program volume */
        g_print ("setting %d\n", value);
        midi_volume[channel] = value/127.0;
        break;
      case 10: /* pan */
          midi_position[channel] = (value-64)/128.0;
        break;
      case 11: /* expression, should set volume of currently playing notes.. */
          { static int done = 0; if (!done) {
            g_print ("unhandled expression=%d on channel %d\n", value, channel); done = 1; } }
        break;
      case 1: /* modulation depth */
          { static int done = 0; if (!done) {
            g_print ("modulation depth=%d on channel %d\n", value, channel); done = 1; } }
        break;
      case 64: /* hold pedal*/
          { static int done = 0; if (!done) {
            g_print ("hold pedal=%d on channel %d\n", value, channel); done = 1; } }
        break;
      case 91: /* reverb level */
      case 93: /* chorus level */
        break;
      case 120: /* all sound off XXX: not tags are used for individual keys, */
        {
          int i;
          for (i=0;i<127;i++)
              lyd_kill (lyd, channel * MIDI_CHANNELS + i);
        }
        break;
      case 128: /* all notes off */
        break;  /* not channels */
      case 99: case 98:/* RPN? */
      case 6: /* data entry */
      case 32: /* data entry */
      case 121: /* reset all*/
        break;
      default:
        g_print ("Controller change channel: %i param: %i value: %i\n",
                 channel, param, value);
    }
}

static void
lyd_handle_midi (Lyd           *lyd,
                   snd_seq_event_t *ev)
{
  switch (ev->type)
    {
      case SND_SEQ_EVENT_NOTEON:
        midi_note_on (lyd, ev->data.note.channel, ev->data.note.note,
                      ev->data.note.velocity);
        break;
      case SND_SEQ_EVENT_NOTEOFF:
        midi_note_off (lyd, ev->data.note.channel, ev->data.note.note,
                       ev->data.note.velocity);
        break;
      case SND_SEQ_EVENT_PGMCHANGE:
        midi_program (lyd, ev->data.control.channel,
                             ev->data.control.value);
        break;
      case SND_SEQ_EVENT_CONTROLLER:
        midi_controller (lyd, ev->data.control.channel,
                         ev->data.control.param, ev->data.control.value);
        break;
      case SND_SEQ_EVENT_SYSEX:
      case SND_SEQ_EVENT_PORT_SUBSCRIBED:
      case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
        break;
      default:
        g_print ("unhandled alsa midi event type %i\n", ev->type);
        break;
    }
}

static snd_seq_t *handle;

static gboolean midi_consume (GIOChannel  *source,
                              GIOCondition condition,
                              gpointer     lyd)
{
  do
    {
      snd_seq_event_t *ev;
      if (snd_seq_event_input (handle, &ev) <= 0)
        continue;
      lyd_handle_midi (lyd, ev);
      snd_seq_free_event(ev);
    } 
  while (snd_seq_event_input_pending (handle, 0) > 0);
  return TRUE;
}

void lyd_midi_init (Lyd *lyd)
{
  int porthandle;
  int err;
  struct pollfd pfd;
  int npfd;
  int i;


  err = snd_seq_open(&handle, "default", SND_SEQ_OPEN_INPUT, 0);
  if (err < 0)
    return;
  snd_seq_set_client_name(handle, "lyd");
  porthandle = snd_seq_create_simple_port(handle, "in_1",
                      SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                      SND_SEQ_PORT_TYPE_MIDI_GENERIC);

  npfd = snd_seq_poll_descriptors_count(handle, POLLIN);
  if (npfd != 1) {
      snd_seq_close(handle);
      return;
  }

  snd_seq_poll_descriptors (handle, &pfd, 1, POLLIN);
  g_io_add_watch (g_io_channel_unix_new (pfd.fd),
                  G_IO_IN, midi_consume, lyd);
  

  for (i=0; i<MIDI_CHANNELS; i++)
    {
      midi_mapping  [i] = 0;
      midi_position [i] = 0.0;
      midi_volume   [i] = 1.0;
    }
}
#endif
