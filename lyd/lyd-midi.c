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

/* Originally from the core MIDI player in allegro
 *
 * By Shawn Hargreaves, George Foot and Elias Pschernig.
 */

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>

#include "lyd.h"

/* The following are the callbacks issued during midi parsing playback */

void lyd_midi_set_volume (Lyd *lyd, int voice, int volume)
{
  g_print ("set vol %i %i\n", voice, volume);
}
void lyd_midi_set_pitch (Lyd *lyd, int voice, int note, int bend)
{
  g_print ("set pitch %i %i %i\n", voice, note, bend);
}
void lyd_midi_note_on (Lyd *lyd, int inst, int note, int bend,
                       int vol, int pan)
{
  g_print ("note_on %i %i %i %i %i\n", inst, note, bend, vol, pan);
}
void lyd_midi_note_off (Lyd *lyd, int inst, int note)
{
  g_print ("note_off %i %i\n", inst, note);
}

void lyd_midi_key_off (Lyd *lyd, int key)
{
  g_print ("key off %i \n", key);
}

/*******************/

int _midi_volume = 127;

#define TIMERS_PER_SECOND 500000000
#define BPS_TO_TIMER(a)   (TIMERS_PER_SECOND/(a))

/* hacks to make it compile (cruft portability macros from allegro) */
#define install_int_ex(a,b)
#define install_int(a,b)
#define remove_int(a)

/* maximum number of layers in a single voice */
#define MIDI_LAYERS  4

/* how often the midi callback gets called maximally / second */
#define MIDI_TIMER_FREQUENCY 1000

/***XXXX***/
                                       /* Theoretical maximums: */
#define MIDI_VOICES           64       /* actual drivers may not be */
#define MIDI_TRACKS           32       /* able to handle this many */

typedef struct LydMidi                 /* a midi file */
{
   int divisions;                      /* number of ticks per quarter note */
   struct {
      unsigned char *data;             /* MIDI message stream */
      int len;                         /* length of the track data */
   } track[MIDI_TRACKS];
} LydMidi;

void destroy_midi (LydMidi *midi);
int play_midi (LydMidi *midi, int loop);
int play_looped_midi (LydMidi *midi, int loop_start, int loop_end);
void stop_midi (void);
void midi_pause (void);
void midi_resume (void);
int midi_seek (int target);
int get_midi_length (LydMidi *midi);
void midi_out (unsigned char *data, int length);

typedef struct LydMidiTrack                       /* a track in the MIDI file */
{
   unsigned char *pos;                          /* position in track data */
   long timer;                                  /* time until next event */
   unsigned char running_status;                /* last MIDI event */
} LydMidiTrack;

typedef struct LydMidiChannel                     /* a MIDI channel */
{
   int patch;                                   /* current sound */
   int volume;                                  /* volume controller */
   int pan;                                     /* pan position */
   int pitch_bend;                              /* pitch bend position */
   int new_volume;                              /* cached volume change */
   int new_pitch_bend;                          /* cached pitch bend */
   int note[128][MIDI_LAYERS];                  /* status of each note */
} LydMidiChannel;

typedef struct LydMidiVoice                       /* a voice on the soundcard */
{
   int channel;                                 /* MIDI channel */
   int note;                                    /* note (-1 = off) */
   int volume;                                  /* note velocity */
   long time;                                   /* when note was triggered */
} LydMidiVoice;

volatile long midi_pos = -1;                    /* current position in MIDI file */
volatile long midi_time = 0;                    /* current position in seconds */
static volatile long midi_timers;               /* current position in allegro-timer-ticks */
static long midi_pos_counter;                   /* delta for midi_pos */

volatile long _midi_tick = 0;                   /* counter for killing notes */

//static void midi_player(void);                  /* core MIDI player routine */
static void prepare_to_play(LydMidi *midi);

static LydMidi *midifile = NULL;                   /* the file that is playing */

static int midi_loop = 0;                       /* repeat at eof? */

long midi_loop_start = -1;                      /* where to loop back to */
long midi_loop_end = -1;                        /* loop at this position */

static int midi_semaphore = 0;                  /* reentrancy flag */

static long midi_timer_speed;                   /* midi_player's timer speed */
static int midi_pos_speed;                      /* MIDI delta -> midi_pos */
static int midi_speed;                          /* MIDI delta -> timer */
static int midi_new_speed;                      /* for tempo change events */

static int old_midi_volume = -1;                /* stored global volume */

static int midi_alloc_channel;                  /* so _midi_allocate_voice */
static int midi_alloc_note;                     /* knows which note the */
static int midi_alloc_vol;                      /* sound is associated with */

static LydMidiTrack midi_track[MIDI_TRACKS];      /* the active tracks */
static LydMidiVoice midi_voice[MIDI_VOICES];      /* synth voice status */
static LydMidiChannel midi_channel[16];           /* MIDI channel info */

static int midi_seeking;                        /* set during seeks */
static int midi_looping;                        /* set during loops */



static int midlength = 0;
static const char *middata = NULL;
#define puck_feof() ((*mp)-middata >= length)
long puck_fread (void *p, long n, const char **mp)
{
  if ((*mp) - middata + n >= midlength)
    n = midlength - ((*mp) - middata);
  memcpy(p, *mp, n);
  (*mp) += n;
  return n;
}

#define EOF (-1)

int puck_getc (const char **mp)
{
  int ret;
  if ((*mp) - middata >= midlength)
    return EOF;
  ret = *((unsigned char *)*mp);
  (*mp) ++;
  return ret;
}

long puck_mgetl(const char **mp)
{
   int b1, b2, b3, b4;

   if ((b1 = puck_getc(mp)) != EOF)
      if ((b2 = puck_getc(mp)) != EOF)
         if ((b3 = puck_getc(mp)) != EOF)
            if ((b4 = puck_getc(mp)) != EOF)
               return (((long)b1 << 24) | ((long)b2 << 16) |
                       ((long)b3 << 8) | (long)b4);

   return EOF;
}

long puck_igetl(const char **mp)
{
   int b1, b2, b3, b4;

     if ((b1 = puck_getc(mp)) != EOF)
      if ((b2 = puck_getc(mp)) != EOF)
         if ((b3 = puck_getc(mp)) != EOF)
            if ((b4 = puck_getc(mp)) != EOF)
               return (((long)b4 << 24) | ((long)b3 << 16) |
                       ((long)b2 << 8) | (long)b1);
   return EOF;
}

long puck_mgetw(const char **mp)
{
   int b1, b2;

   if ((b1 = puck_getc(mp)) != EOF)
    if ((b2 = puck_getc(mp)) != EOF)
      return (b1 << 8 | b2);
   return EOF;
}

#undef puck_feof
#define puck_feof() (mp-mididata >= length)
#define puck_fseek(pos) mp = mididata + pos


/* load_midi:
 *  Loads a standard MIDI file, returning a pointer to a MIDI structure,
 *  or NULL on error. 
 */
LydMidi *load_midi(const char *mididata, int length)
{
   const char *mp = (void*)mididata;
   int c;
   char buf[4];
   long data;
   LydMidi *midi;
   int num_tracks;
   midlength = length;
   middata = mididata;

   if (!mididata || length == 0)
     return NULL;

   midi = malloc(sizeof(LydMidi));              /* get some memory */
   if (!midi) {
      return NULL;
   }

   for (c=0; c<MIDI_TRACKS; c++) {
      midi->track[c].data = NULL;
      midi->track[c].len = 0;
   }

   puck_fread(buf, 4, &mp); /* read midi header */

   /* Is the midi inside a .rmi file? */
   if (memcmp(buf, "RIFF", 4) == 0) { /* check for RIFF header */
      puck_mgetl(&mp);

      while (!puck_feof()) {
         puck_fread(buf, 4, &mp); /* RMID chunk? */
         if (memcmp(buf, "RMID", 4) == 0) break;

         puck_fseek(puck_igetl(&mp)); /* skip to next chunk */
      }

      if (puck_feof()) goto err;

      puck_mgetl(&mp);
      puck_mgetl(&mp);
      puck_fread(buf, 4, &mp); /* read midi header */
   }

   if (memcmp(buf, "MThd", 4))
      goto err;

   puck_mgetl(&mp);                           /* skip header chunk length */

   data = puck_mgetw(&mp);                    /* MIDI file type */
   if ((data != 0) && (data != 1))
      goto err;

   num_tracks = puck_mgetw(&mp);              /* number of tracks */
   if ((num_tracks < 1) || (num_tracks > MIDI_TRACKS))
      goto err;

   data = puck_mgetw(&mp);                    /* beat divisions */
   midi->divisions = ABS(data);

   for (c=0; c<num_tracks; c++) {            /* read each track */
      puck_fread(buf, 4, &mp);                /* read track header */
      if (memcmp(buf, "MTrk", 4))
	 goto err;

      data = puck_mgetl(&mp);                 /* length of track chunk */
      midi->track[c].len = data;

      midi->track[c].data = malloc(data); /* allocate memory */
      if (!midi->track[c].data)
	 goto err;
					     /* finally, read track data */
      if (puck_fread(midi->track[c].data, data, &mp) != data)
	 goto err;
   }

   return midi;

   /* oh dear... */
   err:
   destroy_midi(midi);
   return NULL;
}


/* destroy_midi:
 *  Frees the memory being used by a MIDI file.
 */
void destroy_midi(LydMidi *midi)
{
   int c;

   if (midi == midifile)
      stop_midi();

   if (midi) {
      for (c=0; c<MIDI_TRACKS; c++) {
	 if (midi->track[c].data) {
	    free(midi->track[c].data);
	 }
      }

      free(midi);
   }
}



/* parse_var_len:
 *  The MIDI file format is a strange thing. Time offsets are only 32 bits,
 *  yet they are compressed in a weird variable length format. This routine 
 *  reads a variable length integer from a MIDI data stream. It returns the 
 *  number read, and alters the data pointer according to the number of
 *  bytes it used.
 */
static unsigned long parse_var_len(const unsigned char **data)
{
   unsigned long val = **data & 0x7F;

   while (**data & 0x80) {
      (*data)++;
      val <<= 7;
      val += (**data & 0x7F);
   }

   (*data)++;
   return val;
}



/* global_volume_fix:
 *  Converts a note volume, adjusting it according to the global 
 *  _midi_volume variable.
 */
static inline int global_volume_fix(int vol)
{
   if (_midi_volume >= 0)
      return (vol * _midi_volume) / 256;

   return vol;
}

/* sort_out_volume:
 *  Converts a note volume, adjusting it according to the channel volume
 *  and the global _midi_volume variable.
 */
static inline int sort_out_volume(int c, int vol)
{
   return global_volume_fix((vol * midi_channel[c].volume) / 128);
}


static Lyd *lyd = NULL;


/* midi_note_off:
 *  Processes a MIDI note-off event.
 */
static void midi_note_off(int channel, int note)
{
   int done = FALSE;
   int voice, layer;

   /* oh well, have to do it the long way... */
   for (layer=0; layer<MIDI_LAYERS; layer++) {
      voice = midi_channel[channel].note[note][layer];
      if (voice >= 0) {
	 //midi_driver->key_off(voice + midi_driver->basevoice);
         lyd_midi_key_off (lyd, voice);
	 midi_voice[voice].note = -1;
	 midi_voice[voice].time = _midi_tick;
	 midi_channel[channel].note[note][layer] = -1; 
	 done = TRUE;
      }
   }
}

/* sort_out_pitch_bend:
 *  MIDI pitch bend range is + or - two semitones. The low-level soundcard
 *  drivers can only handle bends up to +1 semitone. This routine converts
 *  pitch bends from MIDI format to our own format.
 */
static inline void sort_out_pitch_bend(int *bend, int *note)
{
   if (*bend == 0x2000) {
      *bend = 0;
      return;
   }

   (*bend) -= 0x2000;

   while (*bend < 0) {
      (*note)--;
      (*bend) += 0x1000;
   }

   while (*bend >= 0x1000) {
      (*note)++;
      (*bend) -= 0x1000;
   }
}

/* midi_note_on:
 *  Processes a MIDI note-on event. Tries to find a free soundcard voice,
 *  and if it can't either cuts off an existing note, or if 'polite' is
 *  set, just stores the channel, note and volume in the waiting list.
 */
static void midi_note_on(int channel, int note, int vol, int polite)
{
   int layer, inst, bend, corrected_note;

   /* if the note is already on, turn it off */
   for (layer=0; layer<MIDI_LAYERS; layer++) {
      if (midi_channel[channel].note[note][layer] >= 0) {
         printf ("get it off!\n");
	 midi_note_off(channel, note);
	 return;
      }
   }
  
   /* if zero volume and the note isn't playing, we can just ignore it */
   if (vol == 0)
     {
        /* XXX: the above check should really have found the voice */
        lyd_midi_note_off (lyd, midi_channel[channel].patch, note);
        return;
     }

   if (channel != 9) {
   }

   /* drum sound? */
   if (channel == 9) {
      inst = 128+note;
      corrected_note = 60;
      bend = 0;
   }
   else {
      inst = midi_channel[channel].patch;
      corrected_note = note;
      bend = midi_channel[channel].pitch_bend;
      sort_out_pitch_bend(&bend, &corrected_note);
   }

   /* play the note */
   midi_alloc_channel = channel;
   midi_alloc_note = note;
   midi_alloc_vol = vol;

   /*midi_driver->key_on(inst, corrected_note, bend, 
		       sort_out_volume(channel, vol), 
		       midi_channel[channel].pan);
    */
   lyd_midi_note_on (lyd, inst, corrected_note, bend,
                     sort_out_volume(channel, vol),
                     midi_channel[channel].pan);
}

/* all_notes_off:
 *  Turns off all active notes.
 */
static void all_notes_off(int channel)
{
   {
      int note, layer;

      for (note=0; note<128; note++)
	 for (layer=0; layer<MIDI_LAYERS; layer++)
	    if (midi_channel[channel].note[note][layer] >= 0)
	       midi_note_off(channel, note);
   }
}

/* all_sound_off:
 *  Turns off sound.
 */
static void all_sound_off(int channel)
{
}

/* reset_controllers:
 *  Resets volume, pan, pitch bend, etc, to default positions.
 */
static void reset_controllers(int channel)
{
   midi_channel[channel].new_volume = 128;
   midi_channel[channel].new_pitch_bend = 0x2000;

   switch (channel % 3) {
      case 0:  midi_channel[channel].pan = ((channel/3) & 1) ? 60 : 68; break;
      case 1:  midi_channel[channel].pan = 104; break;
      case 2:  midi_channel[channel].pan = 24; break;
   }

}

/* update_controllers:
 *  Checks cached controller information and updates active voices.
 */
static void update_controllers(void)
{
   int c, c2, vol, bend, note;

   for (c=0; c<16; c++) {
      /* check for volume controller change */
      if ((midi_channel[c].volume != midi_channel[c].new_volume) || (old_midi_volume != _midi_volume)) {
	 midi_channel[c].volume = midi_channel[c].new_volume;
	 {
	    for (c2=0; c2<MIDI_VOICES; c2++) {
	       if ((midi_voice[c2].channel == c) && (midi_voice[c2].note >= 0)) {
		  vol = sort_out_volume(c, midi_voice[c2].volume);
		  //midi_driver->set_volume(c2 + midi_driver->basevoice, vol);
                  lyd_midi_set_volume (lyd, c2, vol);
	       }
	    }
	 }
      }

      /* check for pitch bend change */
      if (midi_channel[c].pitch_bend != midi_channel[c].new_pitch_bend) {
	 midi_channel[c].pitch_bend = midi_channel[c].new_pitch_bend;
	 {
	    for (c2=0; c2<MIDI_VOICES; c2++) {
	       if ((midi_voice[c2].channel == c) && (midi_voice[c2].note >= 0)) {
		  bend = midi_channel[c].pitch_bend;
		  note = midi_voice[c2].note;
		  sort_out_pitch_bend(&bend, &note);
                  lyd_midi_set_pitch (lyd, c2, note, bend);
	       }
	    }
	 }
      }
   }

   old_midi_volume = _midi_volume;
}

/* process_controller:
 *  Deals with a MIDI controller message on the specified channel.
 */
static void process_controller(int channel, int ctrl, int data)
{
   switch (ctrl) {
      case 7:                                   /* main volume */
	 midi_channel[channel].new_volume = data+1;
	 break;
      case 10:                                  /* pan */
	 midi_channel[channel].pan = data;
	 break;
      case 120:                                 /* all sound off */
	 all_sound_off(channel);
	 break;
      case 121:                                 /* reset all controllers */
	 reset_controllers(channel);
	 break;
      case 123:                                 /* all notes off */
      case 124:                                 /* omni mode off */
      case 125:                                 /* omni mode on */
      case 126:                                 /* poly mode off */
      case 127:                                 /* poly mode on */
	 all_notes_off(channel);
	 break;
      default:
	 break;
   }
}

/* process_meta_event:
 *  Processes the next meta-event on the specified track.
 */
static void process_meta_event(const unsigned char **pos, long *timer)
{
   unsigned char metatype = *((*pos)++);
   long length = parse_var_len(pos);
   long tempo;

   if (metatype == 0x2F) {                      /* end of track */
      *pos = NULL;
      *timer = LONG_MAX;
      return;
   }

   if (metatype == 0x51) {                      /* tempo change */
      tempo = (*pos)[0] * 0x10000L + (*pos)[1] * 0x100 + (*pos)[2];
      midi_new_speed = (tempo/1000) * (TIMERS_PER_SECOND/1000);
      midi_new_speed /= midifile->divisions;
   }

   (*pos) += length;
}

/* process_midi_event:
 *  Processes the next MIDI event on the specified track.
 */
static void process_midi_event(const unsigned char **pos, unsigned char *running_status, long *timer)
{
   unsigned char byte1, byte2; 
   int channel;
   unsigned char event;
   long l;

   event = *((*pos)++); 

   if (event & 0x80) {                          /* regular message */
      /* no running status for sysex and meta-events! */
      if ((event != 0xF0) && (event != 0xF7) && (event != 0xFF))
	 *running_status = event;
      byte1 = (*pos)[0];
      byte2 = (*pos)[1];
   }
   else {                                       /* use running status */
      byte1 = event; 
      byte2 = (*pos)[0];
      event = *running_status; 
      (*pos)--;
   }

   channel = event & 0x0F;

   switch (event>>4) {
      case 0x08:                                /* note off */
	 midi_note_off(channel, byte1);
	 (*pos) += 2;
	 break;

      case 0x09:                                /* note on */
	 midi_note_on(channel, byte1, byte2, 1);
	 (*pos) += 2;
	 break;

      case 0x0A:                                /* note aftertouch */
	 (*pos) += 2;
	 break;

      case 0x0B:                                /* control change */
	 process_controller(channel, byte1, byte2);
	 (*pos) += 2;
	 break;

      case 0x0C:                                /* program change */
	 midi_channel[channel].patch = byte1;
	 (*pos) += 1;
	 break;

      case 0x0D:                                /* channel aftertouch */
	 (*pos) += 1;
	 break;

      case 0x0E:                                /* pitch bend */
	 midi_channel[channel].new_pitch_bend = byte1 + (byte2<<7);
	 (*pos) += 2;
	 break;

      case 0x0F:                                /* special event */
	 switch (event) {
	    case 0xF0:                          /* sysex */
	    case 0xF7: 
	       l = parse_var_len(pos);
	       (*pos) += l;
	       break;

	    case 0xF2:                          /* song position */
	       (*pos) += 2;
	       break;

	    case 0xF3:                          /* song select */
	       (*pos)++;
	       break;

	    case 0xFF:                          /* meta-event */
	       process_meta_event(pos, timer);
	       break;

	    default:
	       /* the other special events don't have any data bytes,
		  so we don't need to bother skipping past them */
	       break;
	 }
	 break;

      default:
	 /* something has gone badly wrong if we ever get to here */
	 break;
   }
}

static gboolean midi_player(gpointer data);
static guint mplayer = 0;
/* midi_player:
 *  The core MIDI player: to be used as a timer callback.
 */
static gboolean midi_player(gpointer data)
{
   int c;
   long l;
   int active;

   if (!midifile)
      return FALSE;

   if (midi_semaphore) {
      midi_timer_speed += BPS_TO_TIMER(MIDI_TIMER_FREQUENCY);
      install_int_ex(midi_player, BPS_TO_TIMER(MIDI_TIMER_FREQUENCY));
      return FALSE;
   }

   midi_semaphore = TRUE;
   _midi_tick++;

   midi_timers += midi_timer_speed;
   midi_time = midi_timers / TIMERS_PER_SECOND;

   do_it_all_again:

   /* deal with each track in turn... */
   for (c=0; c<MIDI_TRACKS; c++) { 
      if (midi_track[c].pos) {
	 midi_track[c].timer -= midi_timer_speed;

	 /* while events are waiting, process them */
	 while (midi_track[c].timer <= 0) { 
	    process_midi_event((const unsigned char**) &midi_track[c].pos, 
			       &midi_track[c].running_status,
			       &midi_track[c].timer); 

	    /* read next time offset */
	    if (midi_track[c].pos) { 
	       l = parse_var_len((const unsigned char**) &midi_track[c].pos);
	       l *= midi_speed;
	       midi_track[c].timer += l;
	    }
	 }
      }
   }

   /* update global position value */
   midi_pos_counter -= midi_timer_speed;
   while (midi_pos_counter <= 0) {
      midi_pos_counter += midi_pos_speed;
      midi_pos++;
   }

   /* tempo change? */
   if (midi_new_speed > 0) {
      for (c=0; c<MIDI_TRACKS; c++) {
	 if (midi_track[c].pos) {
	    midi_track[c].timer /= midi_speed;
	    midi_track[c].timer *= midi_new_speed;
	 }
      }
      midi_pos_counter /= midi_speed;
      midi_pos_counter *= midi_new_speed;

      midi_speed = midi_new_speed;
      midi_pos_speed = midi_new_speed * midifile->divisions;
      midi_new_speed = -1;
   }

   /* figure out how long until we need to be called again */
   active = 0;
   midi_timer_speed = LONG_MAX;
   for (c=0; c<MIDI_TRACKS; c++) {
      if (midi_track[c].pos) {
	 active = 1;
	 if (midi_track[c].timer < midi_timer_speed)
	    midi_timer_speed = midi_track[c].timer;
      }
   }

   /* end of the music? */
   if ((!active) || ((midi_loop_end > 0) && (midi_pos >= midi_loop_end))) {
      if ((midi_loop) && (!midi_looping)) {
	 if (midi_loop_start > 0) {
	    remove_int(midi_player);
	    midi_semaphore = FALSE;
	    midi_looping = TRUE;
	    if (midi_seek(midi_loop_start) != 0) {
	       midi_looping = FALSE;
	       stop_midi(); 
	       return FALSE;
	    }
	    midi_looping = FALSE;
	    midi_semaphore = TRUE;
	    goto do_it_all_again;
	 }
	 else {
	    for (c=0; c<16; c++) {
	       all_notes_off(c);
	       all_sound_off(c);
	    }
	    prepare_to_play(midifile);
	    goto do_it_all_again;
	 }
      }
      else {
	 stop_midi(); 
	 midi_semaphore = FALSE;
	 return FALSE;
      }
   }

   /* reprogram the timer */
   if (midi_timer_speed < BPS_TO_TIMER(MIDI_TIMER_FREQUENCY))
      midi_timer_speed = BPS_TO_TIMER(MIDI_TIMER_FREQUENCY);

   if (!midi_seeking) 
      install_int_ex(midi_player, midi_timer_speed);

   /* controller changes are cached and only processed here, so we can 
      condense streams of controller data into just a few voice updates */ 
   update_controllers();

   midi_semaphore = FALSE;
   return TRUE;
}

/* midi_init:
 *  Sets up the MIDI player ready for use. Returns non-zero on failure.
 */
static int midi_init(void)
{
   int c, c2, c3;
   static int inited = FALSE;

   if (inited)
     return 0;
   inited = TRUE;

   for (c=0; c<16; c++) {
      midi_channel[c].volume = midi_channel[c].new_volume = 128;
      midi_channel[c].pitch_bend = midi_channel[c].new_pitch_bend = 0x2000;

      for (c2=0; c2<128; c2++)
	 for (c3=0; c3<MIDI_LAYERS; c3++)
	    midi_channel[c].note[c2][c3] = -1;
   }

   for (c=0; c<MIDI_VOICES; c++) {
      midi_voice[c].note = -1;
      midi_voice[c].time = 0;
   }
   return 0;
}

/* prepare_to_play:
 *  Sets up all the global variables needed to play the specified file.
 */
static void prepare_to_play(LydMidi *midi)
{
   int c;
   assert(midi);

   for (c=0; c<16; c++)
      reset_controllers(c);

   update_controllers();

   midifile = midi;
   midi_pos = 0;
   midi_timers = 0;
   midi_time = 0;
   midi_pos_counter = 0;
   midi_speed = TIMERS_PER_SECOND / 2 / midifile->divisions;   /* 120 bpm */
   midi_new_speed = -1;
   midi_pos_speed = midi_speed * midifile->divisions;
   midi_timer_speed = 0;
   midi_seeking = 0;
   midi_looping = 0;

   for (c=0; c<16; c++) {
      midi_channel[c].patch = 0;
   }

   for (c=0; c<MIDI_TRACKS; c++) {
      if (midi->track[c].data) {
	 midi_track[c].pos = midi->track[c].data;
	 midi_track[c].timer = parse_var_len((const unsigned char**) &midi_track[c].pos);
	 midi_track[c].timer *= midi_speed;
      }
      else {
	 midi_track[c].pos = NULL;
	 midi_track[c].timer = LONG_MAX;
      }
      midi_track[c].running_status = 0;
   }
}

int play_midi(LydMidi *midi, int loop)
{
   int c;

   remove_int(midi_player);

   for (c=0; c<16; c++) {
      all_notes_off(c);
      all_sound_off(c);
   }

   if (midi) {
      midi_loop = loop;
      midi_loop_start = -1;
      midi_loop_end = -1;

      prepare_to_play(midi);

      /* arbitrary speed, midi_player() will adjust it */
      install_int(midi_player, 20);
      mplayer = g_timeout_add (10, midi_player, NULL);
   }
   else {
      midifile = NULL;

      if (midi_pos > 0)
	 midi_pos = -midi_pos;
      else if (midi_pos == 0)
	 midi_pos = -1;
   }

   return 0;
}

/* play_looped_midi:
 *  Like play_midi(), but the file loops from the specified end position
 *  back to the specified start position (the end position can be -1 to 
 *  indicate the end of the file).
 */
int play_looped_midi(LydMidi *midi, int loop_start, int loop_end)
{
   if (play_midi(midi, TRUE) != 0)
      return -1;

   midi_loop_start = loop_start;
   midi_loop_end = loop_end;

   return 0;
}

void stop_midi(void)
{
   play_midi(NULL, FALSE);
}

void midi_pause(void)
{
   int c;

   if (!midifile)
      return;

   remove_int(midi_player);

   for (c=0; c<16; c++) {
      all_notes_off(c);
      all_sound_off(c);
   }
}

void midi_resume(void)
{
   if (!midifile)
      return;

   install_int_ex(midi_player, midi_timer_speed);
}


/* midi_seek:
 *  Seeks to the given midi_pos in the current MIDI file. If the target 
 *  is earlier in the file than the current midi_pos it seeks from the 
 *  beginning; otherwise it seeks from the current position. Returns zero 
 *  if successful, non-zero if it hit the end of the file (1 means it 
 *  stopped playing, 2 means it looped back to the start).
 */
int midi_seek(int target)
{
   int old_midi_loop;
   LydMidi *old_midifile;
   int old_patch[16];
   int old_volume[16];
   int old_pan[16];
   int old_pitch_bend[16];
   int c;

   if (!midifile)
      return -1;

   /* first stop the player */
   midi_pause();

   /* store current settings */
   for (c=0; c<16; c++) {
      old_patch[c] = midi_channel[c].patch;
      old_volume[c] = midi_channel[c].volume;
      old_pan[c] = midi_channel[c].pan;
      old_pitch_bend[c] = midi_channel[c].pitch_bend;
   }

   /* save some variables and give temporary values */
   old_midi_loop = midi_loop;
   midi_loop = 0;
   old_midifile = midifile;

   /* set flag to tell midi_player not to reinstall itself */
   midi_seeking = 1;

   /* are we seeking backwards? If so, skip back to the start of the file */
   if (target <= midi_pos)
      prepare_to_play(midifile);

   /* now sit back and let midi_player get to the position */
   while ((midi_pos < target) && (midi_pos >= 0)) {
      int mmpc = midi_pos_counter;
      int mmp = midi_pos;

      mmpc -= midi_timer_speed;
      while (mmpc <= 0) {
	 mmpc += midi_pos_speed;
	 mmp++;
      }

      if (mmp >= target)
	 break;

      midi_player(NULL);
   }

   /* restore previously saved variables */
   midi_loop = old_midi_loop;
   midi_seeking = 0;

   if (midi_pos >= 0) {
      /* if we didn't hit the end of the file, continue playing */
      if (!midi_looping)
	 install_int(midi_player, 20);

      return 0;
   }

   if ((midi_loop) && (!midi_looping)) {  /* was file looped? */
      prepare_to_play(old_midifile);
      install_int(midi_player, 20);
      return 2;                           /* seek past EOF => file restarted */
   }

   return 1;                              /* seek past EOF => file stopped */
}


/* get_midi_length:
 *  Returns the length, in seconds, of the specified midi. This will stop any
 *  currently playing midi. Don't call it too often, since it simulates playing
 *  all of the midi to get the time even if the midi contains tempo changes.
 */
int get_midi_length(LydMidi *midi)
{
    play_midi(midi, 0);
    /* XXX: blocks expecting a thread to consume.. */
    while (midi_pos < 0); /* Without this, midi_seek won't work. */
    midi_seek(INT_MAX);
    return midi_time;
}

/* midi_out: Inserts MIDI command bytes into the output stream, in realtime. */
void midi_out(unsigned char *data, int length)
{
   unsigned char *pos = data;
   unsigned char running_status = 0;
   long timer = 0;
   assert(data);

   midi_semaphore = TRUE;
   _midi_tick++;

   while (pos < data+length)
      process_midi_event((const unsigned char**) &pos, &running_status, &timer);

   update_controllers();

   midi_semaphore = FALSE;
}

/******************************/



static LydMidi *midi = NULL;
void lyd_midi_load  (Lyd *nlyd, void *data, int length)
{
  midi_init ();
  if (!data)
    {
      midi = NULL;
    }
  else
    {
      midi = load_midi (data, length);
      printf ("loaded %i\n", length);
    }
  lyd = nlyd;
}

void lyd_midi_play  (Lyd *lyd)
{
  printf ("%p\n", midi);
  play_midi (midi, 0);
}

void lyd_midi_pause (Lyd *lyd)
{
}

void lyd_midi_set_repeat (Lyd *lyd, float start, float end)
{
}
