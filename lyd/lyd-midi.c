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

/* Originally based on the core MIDI player in allegro By Shawn Hargreaves,
 * George Foot and Elias Pschernig.
 */

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <lyd/lyd.h>

/* The following are the callbacks issued during midi parsing playback */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define MIDI_CHANNELS  16
#define MIDI_NOTES     128
#define MIDI_TRACKS    48

#include <stdlib.h>
#include <math.h>

static float midi2hz (int midinote)
{
  return (440.0 * pow (2,(midinote-69.0)/12.0));
}

#ifndef ABS
#define ABS(a) (((a)<0)?-(a):(a))
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define TIMERS_PER_SECOND            1000

typedef struct LydMidiTrack          /* a track in the MIDI file */
{
   unsigned char *pos;               /* position in track data */
   long timer;                       /* time until next event */
   unsigned char running_status;     /* last MIDI event */
} LydMidiTrack;

typedef struct LydMidiChannel         /* a MIDI channel */
{
   int patch;                         /* current sound */
   int volume;                        /* volume controller */
   int pan;                           /* pan position */
   int pitch_bend;                    /* pitch bend position */
   int new_volume;                    /* cached volume change */
   int new_pitch_bend;                /* cached pitch bend */
   int note_volume[MIDI_NOTES];       /* -1 == not playing */
   LydVoice *note_voice[MIDI_NOTES];  /* NULL == not existing */
} LydMidiChannel;

typedef struct {
  Lyd *lyd;               /* pointer to lyd instance used by midi player */
  int volume;             /* global playback volume */

  /* loaded midi file */
  int loaded;             /* whether a midi file has been loaded */
  int elapsed;            /* msec elapsed since last update */
  int ticks_per_quarter_note;          /* ticks per quarter note */
  int length;             /* NYI: in seconds, computed and cached on load */
  struct {
     unsigned char *data; /* MIDI message stream */
     int len;             /* length of the track data */
  } file_track[MIDI_TRACKS];

  long pos;                  /* current position in MIDI file */
  long time_seconds;         /* current position in seconds */
  long ticks;                /* current position in ticks */
  long pos_counter;          /* delta for pos */
  int  pos_speed;            /* MIDI delta -> pos */
  float speed;               /* MIDI delta -> timer */
  float new_speed;           /* for tempo change events */

  int loop;                  /* repeat at eof? */
  long loop_start;           /* where to loop back to */
  long loop_end;             /* loop at this position */
  int oldvolume;             /* stored global volume */
  LydMidiTrack active_track[MIDI_TRACKS];  /* the active tracks */
  LydMidiChannel channel[MIDI_CHANNELS]; /* MIDI channel info */
  int seeking;               /* set during seeks */
  int looping;               /* set during loops */
  int playing;
  int started;
} LydMidi;


/* forward declarations for the lyd integrated midi callbacks */
static void lyd_midi_program    (LydMidi *midi, int channel, int preset);
static void lyd_midi_note_on    (LydMidi *midi, int channel, int note, int velocity);
void        lyd_midi_note_off   (LydMidi *midi, int channel, int note);
static void lyd_midi_control    (LydMidi *midi, int channel, int ctrl, int data);
static void lyd_midi_set_volume (LydMidi *midi, int channel, int note, int volume);
static void lyd_midi_set_pitch  (LydMidi *midi, int channel, int note, int bend);

static void lyd_midi_unload(LydMidi *midi);
static int  play_midi (LydMidi *midi, int loop);
static int  midi_seek (LydMidi *midi, int target);
static void stop_midi (LydMidi *midi);
static void pause_midi (LydMidi *midi);
static void lyd_midi_prepare_to_play(LydMidi *midi);

/* XXX the following should be cleaned up */
static int midlength = 0;
static const unsigned char *middata = NULL;
#define stream_eof() ((*mp)-middata >= length)
static long stream_fread (void *p, long n, const unsigned char **mp)
{
  if ((*mp) - middata + n >= midlength)
    n = midlength - ((*mp) - middata);
  memcpy(p, *mp, n);
  (*mp) += n;
  return n;
}

#define EOF (-1)

static int stream_getc (const unsigned char **mp)
{
  int ret;
  if ((*mp) - middata >= midlength)
    return EOF;
  ret = *((unsigned char *)*mp);
  (*mp) ++;
  return ret;
}

static long stream_mgetw(const unsigned char **mp)
{
   int b1, b2;

   if ((b1 = stream_getc(mp)) != EOF)
    if ((b2 = stream_getc(mp)) != EOF)
      return (b1 << 8 | b2);
   return EOF;
}

static long stream_mgetl(const unsigned char **mp)
{
   int b1, b2, b3, b4;

   if ((b1 = stream_getc(mp)) != EOF)
      if ((b2 = stream_getc(mp)) != EOF)
         if ((b3 = stream_getc(mp)) != EOF)
            if ((b4 = stream_getc(mp)) != EOF)
               return (((long)b1 << 24) | ((long)b2 << 16) |
                       ((long)b3 << 8) | (long)b4);

   return EOF;
}

#undef  stream_eof
#define stream_eof() (mp-mididata >= length)
#define stream_seek(pos) mp = mididata + pos

static int load_midi (LydMidi *midi, const unsigned char *mididata, int length)
{
  const unsigned char *mp;
  int c;
  char buf[4];
  long data;
  int num_tracks;
  midlength = length;
  mp = middata = mididata;

  if (!mididata || length == 0)
    return -1;

  for (c=0; c<MIDI_TRACKS; c++) {
     midi->file_track[c].data = NULL;
     midi->file_track[c].len = 0;
  }

  stream_fread(buf, 4, &mp); /* read midi header */

  if (memcmp(buf, "MThd", 4))
     goto err;

  stream_mgetl(&mp);                           /* skip header chunk length */

  data = stream_mgetw(&mp);                    /* MIDI file type */
  if ((data != 0) && (data != 1))
     goto err;

  num_tracks = stream_mgetw(&mp);              /* number of tracks */
  if ((num_tracks < 1) || (num_tracks > MIDI_TRACKS))
     goto err;

  data = stream_mgetw(&mp);                    /* beat divisons / ticks_per_quarter_note */
  midi->ticks_per_quarter_note = ABS(data);

  for (c=0; c<num_tracks; c++) {            /* read each track */
     stream_fread(buf, 4, &mp);                /* read track header */
     if (memcmp(buf, "MTrk", 4))
       goto err;

     data = stream_mgetl(&mp);                 /* length of track chunk */
     midi->file_track[c].len = data;

     midi->file_track[c].data = malloc(data); /* allocate memory */
     if (!midi->file_track[c].data)
       goto err;
                        /* finally, read track data */
     if (stream_fread(midi->file_track[c].data, data, &mp) != data)
       goto err;
  }

  midi->loaded = 1;
  midi->started = 0;
  midi->playing = 0;
  return 0;

  err: /* oh dear... */
  lyd_midi_unload (midi);
  return -1;
}


/* Free the memory being used by a MIDI file. */
static void lyd_midi_unload (LydMidi *midi)
{
   int c;

   stop_midi(midi);
   if (!midi)
     return;

   for (c=0; c<MIDI_TRACKS; c++)
     {
       if (midi->file_track[c].data)
         free(midi->file_track[c].data);
       midi->file_track[c].data=NULL;
     }
}

static unsigned long parse_variable_length (const unsigned char **data)
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

static inline int sort_out_volume(LydMidi *midi, int c, int vol)
{
   return (((vol * midi->channel[c].volume) / 128) * midi->volume) / 256;
}

static void all_notes_off(LydMidi *midi, int channel)
{
  int note;
  for (note=0; note<MIDI_NOTES; note++)
    lyd_midi_note_off (midi, channel, note);
}

static void all_sound_off(LydMidi *midi, int channel)
{
  /* XXX: kill all possible note tags */
}

/* reset_controllers:
 *  Resets volume, pan, pitch bend, etc, to default positions.
 */
static void reset_controllers(LydMidi *midi, int channel)
{
   midi->channel[channel].new_volume = 128;
   midi->channel[channel].new_pitch_bend = 0x2000;

   switch (channel % 3) {
      case 0:  midi->channel[channel].pan = ((channel/3) & 1) ? 60 : 68; break;
      case 1:  midi->channel[channel].pan = 104; break;
      case 2:  midi->channel[channel].pan = 24; break;
   }

}

/* update_controllers:
 *  Checks cached controller information and updates active voices.
 */
static void update_controllers(LydMidi *midi)
{
   int c, note, vol;

   for (c=0; c<MIDI_CHANNELS; c++) {
      /* check for volume controller change */
      if ((midi->channel[c].volume != midi->channel[c].new_volume) || (midi->oldvolume != midi->volume)) {
         midi->channel[c].volume = midi->channel[c].new_volume;
         {
            for (note=0; note<MIDI_NOTES; note++) {
              if (midi->channel[c].note_volume[note] >= 0) {
                vol = sort_out_volume(midi, c, midi->channel[c].note_volume[note]);
                lyd_midi_set_volume (midi, c, note, vol);
              }
            }
         }
      }

      /* check for pitch bend change */
      if (midi->channel[c].pitch_bend != midi->channel[c].new_pitch_bend) {
         midi->channel[c].pitch_bend = midi->channel[c].new_pitch_bend;
         for (note=0; note<MIDI_NOTES; note++) 
           if (midi->channel[c].note_volume[note] >=1) 
             lyd_midi_set_pitch (midi, c, note, midi->channel[c].pitch_bend);
      }
   }
   midi->oldvolume = midi->volume;
}

/* process_controller:
 *  Deals with a MIDI controller message on the specified channel.
 */
static void lyd_midi_control (LydMidi *midi, int channel, int ctrl, int data)
{
   switch (ctrl) {
      case 7: midi->channel[channel].new_volume = data+1; break;
      case 10: midi->channel[channel].pan = data; break;
      case 120: all_sound_off(midi, channel); break;
      case 121: reset_controllers(midi, channel); break;
      case 123: /* all notes off */    case 124: /* omni mode off */
      case 125: /* omni mode on */     case 126: /* poly mode off */
      case 127: /* poly mode on */     all_notes_off(midi, channel);
     break;
      default:
     break;
   }
}

static void process_meta_event(LydMidi *midi, const unsigned char **pos, long *timer)
{
   unsigned char metatype = *((*pos)++);
   long tempo;

   if (metatype == 0x2F) { /* end of track */
      *pos = NULL;
      *timer = LONG_MAX;
      return;
   }
   if (metatype == 0x51) { /* tempo change */
      int bpm;
      tempo = (*pos)[0] * 0x10000L + (*pos)[1] * 0x100 + (*pos)[2];
      bpm = midi->ticks_per_quarter_note * 1000000/tempo;
      midi->new_speed =    ((tempo/1000) * (TIMERS_PER_SECOND/1000.0))
                         / midi->ticks_per_quarter_note;
   }
   (*pos) += parse_variable_length(pos);
}

static void process_midi_event(LydMidi *midi,
                               const unsigned char **pos,
                               unsigned char *running_status,
                               long *timer)
{
  unsigned char byte1, byte2; 
  int channel;
  unsigned char event;
  long l;

  event = *((*pos)++); 

  if (event & 0x80) { /* regular message */
     /* no running status for sysex and meta-events! */
     if ((event != 0xF0) && (event != 0xF7) && (event != 0xFF))
       *running_status = event;
     byte1 = (*pos)[0];
     byte2 = (*pos)[1];
  } else {            /* use running status */
     byte1 = event; 
     byte2 = (*pos)[0];
     event = *running_status; 
     (*pos)--;
  }

  channel = event & 0x0F;
  switch (event>>4) {
    case 0x08: lyd_midi_note_off (midi, channel, byte1); (*pos) += 2;break;
    case 0x09: lyd_midi_note_on (midi, channel, byte1, byte2); (*pos) += 2;break;
    case 0x0A: /* lyd_midi_note_aftertouch () */ (*pos) += 2;break;
    case 0x0B: lyd_midi_control (midi, channel, byte1, byte2); (*pos) += 2;break;
    case 0x0C: lyd_midi_program (midi, channel, byte1); (*pos) += 1;break;
    case 0x0D: /* lyd_midi->channel_aftertouch () */ (*pos) += 1;break;
    case 0x0E: /* lyd_midi->channel_pitchbend */
         midi->channel[channel].new_pitch_bend = byte1 + (byte2<<7);(*pos) +=2;
         break;
    case 0x0F:  /* special event */
    switch (event) {
      case 0xF0: case 0xF7:/* sysex */ l = parse_variable_length(pos);(*pos) += l;break;
      case 0xF2: /* song position */(*pos) += 2; break;
      case 0xF3: /* song select */  (*pos)++; break;
      case 0xFF: /* meta-event */   process_meta_event(midi, pos, timer); break;
      default: break;
    }
    break;
    default: break;
  }
}

/* insert midi events into decoder in real time */
static void midi_out (LydMidi *midi, unsigned char *data, int length)
{
   unsigned char *pos = data;
   unsigned char running_status = 0;
   long timer = 0;
   if (!data)
     return;
   while (pos < data+length)
     process_midi_event(midi, (const unsigned char**) &pos, &running_status, &timer);

   update_controllers(midi);
}

static int midi_main (LydMidi *midi)
{
   int c;
   int active;
   int elapsed_ticks;

   if (!midi->loaded)
      return FALSE;
   if (!midi->playing && !midi->seeking)
      return FALSE;

   elapsed_ticks = midi->elapsed / 2; /* XXX:
                                       * seem to need this to get the timing
                                       * right, a magic constant that might
                                       * indicate we are computing too often?
                                       */

   midi->ticks += midi->elapsed;
   midi->time_seconds = midi->ticks / TIMERS_PER_SECOND;

   do_it_all_again:

   for (c=0; c<MIDI_TRACKS; c++) { 
      if (midi->active_track[c].pos) {
        midi->active_track[c].timer -= elapsed_ticks;

        /* while events are waiting, process them */
        while (midi->active_track[c].timer <= 0) { 
          process_midi_event(midi, (const unsigned char**) &midi->active_track[c].pos, 
          &midi->active_track[c].running_status,
          &midi->active_track[c].timer); 

          /* read next time offset */
          if (midi->active_track[c].pos) { 
             long tmp;
             tmp = parse_variable_length((const unsigned char**) &midi->active_track[c].pos);
             tmp *= midi->speed;
             midi->active_track[c].timer += tmp;
          }
        }
      }
   }

   /* update global position value */
   midi->pos_counter -= elapsed_ticks;
   while (midi->pos_counter <= 0) {
      midi->pos_counter += midi->pos_speed;
      midi->pos++;
   }

   /* tempo change? */
   if (midi->new_speed > 0) {

      for (c=0; c<MIDI_TRACKS; c++)
         if (midi->active_track[c].pos)
            /* adjust the time we are schedule to wait, converting queued up
             * time in old speed to new speed
             */
            midi->active_track[c].timer =
            (midi->active_track[c].timer / midi->speed) * midi->new_speed;

      /* adjust the time we are schedule to wait, converting queued up
       * time in old speed to new speed
       */
      midi->pos_counter = (midi->pos_counter / midi->speed) * midi->new_speed;
      midi->speed = midi->new_speed; midi->new_speed = -1;
      midi->pos_speed = midi->speed * midi->ticks_per_quarter_note;
      midi->new_speed = -1;
   }

   /* figure out how long until we need to be called again */
   active = 0;
   for (c=0; c<MIDI_TRACKS; c++) {
      if (midi->active_track[c].pos) {
        active = 1;
      }
   }

   /* end of the music? */
   if ((!active) || ((midi->loop_end > 0) && (midi->pos >= midi->loop_end))) {
      if ((midi->loop) && (!midi->looping)) {
         if (midi->loop_start > 0) {
            midi->looping = TRUE;
            if (midi_seek(midi, midi->loop_start) != 0) {
               midi->looping = FALSE;
               stop_midi(midi); 
               return FALSE;
            }
            midi->looping = FALSE;
            goto do_it_all_again;
         }
         else {
            for (c=0; c<MIDI_CHANNELS; c++) {
               all_notes_off (midi, c);
               all_sound_off (midi, c);
            }
            lyd_midi_prepare_to_play (midi);
            goto do_it_all_again;
         }
      } else { /* not looping */
         stop_midi (midi); 
         printf ("end of midi!");
         return FALSE;
      }
   }

   /* controller changes are cached and only processed here, so we can 
      condense streams of controller data into just a few voice updates */ 
   if (!midi->seeking)
     update_controllers(midi);

   return TRUE;
}

static void lyd_midi_prepare_to_play (LydMidi *midi)
{
   int c;
   for (c=0; c<MIDI_CHANNELS; c++)
      reset_controllers(midi, c);
   update_controllers(midi);
   midi->pos = 0;
   midi->ticks = 0;
   midi->time_seconds = 0;
   midi->pos_counter = 0;
   midi->speed = TIMERS_PER_SECOND / 2 / midi->ticks_per_quarter_note; /* 120 bpm */
   midi->new_speed = -1;
   midi->pos_speed = midi->speed * midi->ticks_per_quarter_note;
   midi->seeking = 0;
   midi->looping = 0;

   for (c=0; c<MIDI_CHANNELS; c++) {
      midi->channel[c].patch = 0;
   }

   for (c=0; c<MIDI_TRACKS; c++) {
      if (midi->file_track[c].data) {
         midi->active_track[c].pos = midi->file_track[c].data;
         midi->active_track[c].timer = parse_variable_length(
                          (const unsigned char**) &midi->active_track[c].pos);
         midi->active_track[c].timer *= midi->speed;
      } else {
         midi->active_track[c].pos = NULL;
         midi->active_track[c].timer = LONG_MAX;
      }
      midi->active_track[c].running_status = 0;
   }
}

int play_midi(LydMidi *midi, int loop)
{
   int c;

   for (c=0; c<MIDI_CHANNELS; c++) {
      all_notes_off(midi, c);
      all_sound_off(midi, c);
   }

   if (midi->loaded) {
      midi->loop = loop;
      midi->loop_start = -1;
      midi->loop_end = -1;
      lyd_midi_prepare_to_play(midi);
      midi->playing = TRUE;
      midi->started = TRUE;
   }
   else {
      midi->pos = -1;
   }
   return 0;
}


static void stop_midi(LydMidi *midi)
{
   //midi->loaded = FALSE;
   midi->playing = FALSE;
   //play_midi(midi, FALSE);
   midi->playing = FALSE; /* XXX: probably not needed */
}

static void pause_midi (LydMidi *midi)
{
   int c;

   if (!midi->loaded)
     return;
   for (c=0; c<MIDI_CHANNELS; c++) {
      all_notes_off(midi, c);
      all_sound_off(midi, c);
   }
   midi->playing = FALSE;
}


/* midi_seek:
 *  Seeks to the given pos in the current MIDI file. If the target 
 *  is earlier in the file than the current pos it seeks from the 
 *  beginning; otherwise it seeks from the current position. Returns zero 
 *  if successful, non-zero if it hit the end of the file (1 means it 
 *  stopped playing, 2 means it looped back to the start).
 */
static int midi_seek(LydMidi *midi, int target)
{
   int old_loop;
   int old_patch[MIDI_CHANNELS];
   int old_volume[MIDI_CHANNELS];
   int old_pan[MIDI_CHANNELS];
   int old_pitch_bend[MIDI_CHANNELS];
   int c;

   if (!midi->loaded)
      return -1;

   /* first stop the player */
   pause_midi (midi);

   /* store current settings */
   for (c=0; c<MIDI_CHANNELS; c++) {
      old_patch[c] =      midi->channel[c].patch;
      old_volume[c] =     midi->channel[c].volume;
      old_pan[c] =        midi->channel[c].pan;
      old_pitch_bend[c] = midi->channel[c].pitch_bend;
   }

   /* save some variables and give temporary values */
   old_loop = midi->loop;
   midi->loop = 0;
   midi->seeking = 1;
   /* are we seeking backwards? If so, skip back to the start of the file */
   if (target <= midi->pos)
      lyd_midi_prepare_to_play(midi);
   /* now sit back and let midi->midi_player get to the position */

   while ((midi->time_seconds < target) && (midi->pos >= 0)) {
      int mmpc = midi->pos_counter;
      int mmp = midi->pos;

      mmpc -= midi->elapsed;
      while (mmpc <= 0) {
         mmpc += midi->pos_speed;
         mmp++;
         //printf ("%i %i %i %i\n", target, mmpc, mmp, midi->time_seconds);
      }

      if (mmp >= target)
        break;

      midi->elapsed = 250;
      if (!midi_main (midi))
        break;
   }
   /* restore previously saved variables */
   midi->loop = old_loop;
   midi->seeking = 0;

   if (midi->pos >= 0) {
      printf ("lyd-midi:%i!\n", __LINE__);
      return 0;
   }
   if ((midi->loop) && (!midi->looping)) {  /* was file looped? */
      /* XXX */
      printf ("lyd-midi:%i!\n", __LINE__);
      lyd_midi_prepare_to_play(midi);
      return 2;                           /* seek past EOF => file restarted */
   }
   return 1;                              /* seek past EOF => file stopped */
}


static void lyd_midi_set_volume (LydMidi *midi, int channel, int note, int volume)
{
  lyd_voice_set_param (midi->lyd, midi->channel[channel].note_voice[note], "volume",
                       volume / 127.0);
}
static void lyd_midi_set_pitch (LydMidi *midi, int channel, int note, int bend)
{
  printf ("set pitch %i %i %i\n", channel, note, bend);
  lyd_voice_set_param (midi->lyd, midi->channel[channel].note_voice[note], "volume",
                       midi2hz (note) + ((bend-8192) /16384.0) * 2);
}

void lyd_midi_program    (LydMidi *midi, int channel, int preset)
{
  midi->channel[channel].patch = preset;
}

void lyd_midi_note_off (LydMidi *midi, int channel, int note)
{
  LydVoice *voice;

  midi->channel[channel].note_volume[note] = -1; 

  if (midi->seeking)
    return;

  if ((voice = midi->channel[channel].note_voice[note]))
    {
      lyd_voice_release (midi->lyd, voice);
      midi->channel[channel].note_voice[note] = NULL;
    }
}


#define gen_hash(channel, note) ((channel * 256 + note))

void lyd_midi_note_on (LydMidi *midi, int channel, int note, int vol)
{
  int hashkey = gen_hash (channel, note);
  LydVoice *voice;
  int inst, bend, corrected_note;

  if (vol == 0 && midi->channel[channel].note_volume[note] >=0)
    {
      lyd_midi_note_off (midi, channel, note);
      return;
    }

  if (midi->channel[channel].note_volume[note] >= 0)
    lyd_midi_note_off (midi, channel, note);

  if (channel == 9) { /* Adjust values used to create voice if it is a drum sound */
     inst = 128+note;
     corrected_note = 60;
     bend = 0;
  } else {
     inst = midi->channel[channel].patch;
     corrected_note = note;
     bend = midi->channel[channel].pitch_bend;
  }

  midi->channel[channel].note_volume[note] = vol; 
  if (!midi->seeking){
    voice = lyd_note_full (midi->lyd, midi->channel[channel].patch,
                           midi2hz (corrected_note) + ((bend-8192) /16384.0) * 2,
                           sort_out_volume (midi, channel, vol) / 127.0,
                           4.0, /* max duration of 4s to avoid stuck notes */
                           (midi->channel[channel].pan-64)/127.0,
                           hashkey);
    midi->channel[channel].note_voice[note] = voice;
  }
}

/*****************************/

static LydMidi *midi = NULL;
static void midi_init (Lyd *lyd)
{
   int c, c2;

   if (midi)
     return;
   midi = calloc (sizeof (LydMidi), 1);

   for (c=0; c<MIDI_CHANNELS; c++) {
      midi->channel[c].volume = midi->channel[c].new_volume = 128;
      midi->channel[c].pitch_bend = midi->channel[c].new_pitch_bend = 0x2000;
      for (c2=0; c2<MIDI_NOTES; c2++) {
        midi->channel[c].note_volume[c2] = -1;
      }
   }

  midi->volume = 128;
  midi->pos = -1;
  midi->loop_start = -1;
  midi->loop_end = -1;
  midi->oldvolume = -1;

  midi->loaded = 0;
  midi->loop = 0;
  midi->seeking = 0;
  midi->lyd = lyd;
}

/* get_midi_length:
 *  Returns the length, in seconds, of the specified midi. This will stop any
 *  currently playing midi. Don't call it too often, since it simulates playing
 *  all of the midi to get the time even if the midi contains tempo changes.
 */
int get_midi_length(LydMidi *midi)
{
  int ret;
  play_midi(midi, 0);
  midi_seek(midi, 400);//INT_MAX);
  ret = midi->time_seconds;
  lyd_midi_prepare_to_play(midi);
  return ret;
}

void lyd_midi_load  (Lyd *lyd, unsigned char *data, int length)
{
  midi_init (lyd);
  midi->lyd = lyd;
  if (!data)
    {
      /* XXX: free previously loaded */
      midi->loaded = 0;
    }
  else
    {
      load_midi (midi, data, length);
      midi->length = get_midi_length (midi);
      printf ("length: %i\n", midi->length);
    }
}

void lyd_midi_set_playing (Lyd *lyd, int playing)
{
  midi_init (lyd);
  if (playing)
    {
      if (midi->started)
        midi->playing = TRUE;
      else
        play_midi (midi, 0);
    }
  else
    {
      pause_midi (midi);
    }
}

void lyd_midi_seek (Lyd *lyd, float target)
{
  if (!midi)
    return;
  midi_seek (midi, target);
}

void lyd_midi_set_repeat (Lyd *lyd, float start, float end)
{
  midi_init (lyd);
  midi->loop_start = start;
  midi->loop_end = end;
  midi->loop = TRUE;
}

/* midi_out: Inserts MIDI command bytes into the output stream, in realtime. */
void lyd_midi_out (Lyd *lyd, unsigned char *data, int length)
{
  midi_init (lyd);
  midi_out (midi, data, length);
}

void lyd_midi_iterate (Lyd *lyd, float elapsed)
{
  if (!midi || ! midi->loaded || ! midi->playing)
    return;
  midi->elapsed = elapsed * TIMERS_PER_SECOND;
  midi_main (midi);
}
#if 0   /******** UNUSED **************/
/* play_looped_midi:
 *  Like play_midi(), but the file loops from the specified end position
 *  back to the specified start position (the end position can be -1 to 
 *  indicate the end of the file).
 */
static int play_looped_midi(LydMidi *midi, int loop_start, int loop_end)
{
   if (play_midi(midi, TRUE) != 0)
      return -1;

   midi->loop_start = loop_start;
   midi->loop_end = loop_end;

   return 0;
}
void midi_resume(void)
{
   if (!midifile)
      return;

   install_int_ex(midi_player, timer_speed);
}
#endif


#if 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lyd/lyd.h>
#include <stdlib.h>
#include <math.h>

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>

static void
lyd_handle_midi (LydMidi         *midi,
                 snd_seq_event_t *ev)
{
  switch (ev->type)
  {
    case SND_SEQ_EVENT_NOTEON:
      lyd_midi_note_on (midi, ev->data.note.channel, ev->data.note.note,
                        ev->data.note.velocity);break;
    case SND_SEQ_EVENT_NOTEOFF:
      lyd_midi_note_off (midi, ev->data.note.channel,ev->data.note.note);break;
    case SND_SEQ_EVENT_PGMCHANGE:
      lyd_midi_program (midi, ev->data.control.channel,
                              ev->data.control.value);break;
    case SND_SEQ_EVENT_CONTROLLER:
      lyd_midi_control (midi, ev->data.control.channel,
                        ev->data.control.param, ev->data.control.value);break;
    case SND_SEQ_EVENT_SYSEX:
    case SND_SEQ_EVENT_PORT_SUBSCRIBED:
    case SND_SEQ_EVENT_PORT_UNSUBSCRIBED: break;
    case SND_SEQ_EVENT_PITCHBEND:
      midi->channel[ev->data.control.channel].new_pitch_bend = ev->data.control.value;
          break;

    default: printf ("unhandled alsa midi event type %i\n", ev->type);break;
  }
}

#include <pthread.h>
static snd_seq_t *handle;

static int midi_consume ()
{
  do
    {
      snd_seq_event_t *ev;
      if (snd_seq_event_input (handle, &ev) <= 0)
        continue;
      lyd_handle_midi (midi, ev);
      snd_seq_free_event(ev);
    } 
  while (snd_seq_event_input_pending (handle, 0) > 0);
  return 0;
}

extern int lyd_dead;
static void *alsa_midi_start(void *data)
{
  struct pollfd *pfd = data;
  while (1)
    {
      if (lyd_dead)
        break;
      if (poll (pfd, 1, 1000))
        if (pfd->revents > 0)
          midi_consume();
    }
  return NULL;
}

void lyd_midi_init (Lyd *lyd)
{
  int err;
  static struct pollfd pfd;
  int npfd;
  pthread_t tid;

  midi_init (lyd);
  err = snd_seq_open(&handle, "default", SND_SEQ_OPEN_INPUT, 0);
  if (err < 0)
    return;
  snd_seq_set_client_name(handle, "lyd");
  snd_seq_create_simple_port(handle, "in_1",
                      SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                      SND_SEQ_PORT_TYPE_MIDI_GENERIC);

  npfd = snd_seq_poll_descriptors_count(handle, POLLIN);
  if (npfd != 1) {
      snd_seq_close(handle);
      return;
  }

  snd_seq_poll_descriptors (handle, &pfd, 1, POLLIN);
  pthread_create(&tid, NULL, alsa_midi_start, &pfd);
}


#endif
#endif
