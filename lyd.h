#ifndef __SYNTH_H_
#define __SYNTH_H_

/* what we have is hard coded for now, * simply comment one of these out to * disable that part
 */
#define HAVE_OSC 
#define HAVE_SDL
//#define HAVE_AO
#define HAVE_ALSA
//#define HAVE_JACK

#define NIH

//#include <glib.h>

typedef struct _LydVoice   LydVoice;
typedef struct _Lyd        Lyd;
typedef struct _LydProgram LydProgram;
typedef enum {
  LYD_f32, /* 32bit floating point mono */
  LYD_f32S,/* 32bit floating point stereo, stream2 is used */
  LYD_s16S,/* 16bit signed integer stereo, interleaved on stream1*/
} LydFormat;

Lyd        *lyd_new             (void);

void        lyd_set_sample_rate (Lyd *lyd, int sample_rate);

void        lyd_set_format      (Lyd *lyd, LydFormat format);

void        lyd_free            (Lyd *lyd);

long        lyd_synthesize      (Lyd *lyd, int len, void *stream,void *stream2);

LydProgram *lyd_compile         (Lyd *lyd, const char *source);

/* should perhaps be lyd_voice_new */
LydVoice   *lyd_new_voice       (Lyd *lyd, LydProgram *program, int tag);

void        lyd_kill            (Lyd *lyd, int tag);


void        lyd_program_free    (LydProgram *program);


void        lyd_voice_release   (Lyd *lyd, LydVoice *voice);

void        lyd_voice_kill      (Lyd *lyd, LydVoice *voice);

void        lyd_voice_set_delay (Lyd *lyd, LydVoice *voice, double seconds);

/* the voice will be automatically released after it has been playing
 * for voice seconds
 */
void        lyd_voice_set_duration (Lyd *lyd, LydVoice *voice, double seconds);

void        lyd_program_set_param (LydProgram *program, 
                                   const char *param, double    value);

void        lyd_voice_set_param (Lyd        *lyd,   LydVoice *voice,
                                 const char *param, double    value);

double      lyd_voice_get_param (Lyd        *lyd, LydVoice *voice,
                                 const char *param);

void        lyd_voice_set       (Lyd        *lyd,         LydVoice *voice,
                                 const char *first_param, double    first_value,
                                 ...);/* param, value, ..., NULL */

/* NYI */
void        lyd_wav_from_data   (Lyd *lyd,         const char *name,
                                 int  length,      LydFormat   format,
                                 int  sample_rate, void       *buf);

void         lyd_voice_set_position (Lyd         *lyd,
                                     LydVoice    *voice,
                                     double       position);
#endif
