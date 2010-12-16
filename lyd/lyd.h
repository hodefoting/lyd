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

#ifndef __LYD_H_
#define __LYD_H_

/* Lyd:
 *
 * The master context/engine instance of lyd, can
 * act as an audio canvas for LydVoices. 
 *
 */
typedef struct _Lyd Lyd;
/**
 * lyd_new:
 *
 * Create a new lyd engine.
 */
Lyd        *lyd_new             (void);
/**
 * lyd_free:
 * @lyd: lyd engine
 *
 * Destroy a lyd engine freeing up resources used by lyd.
 */
void        lyd_free            (Lyd *lyd);

/**
 * lyd_set_voice_count:
 * @lyd: lyd engine
 * @new_count: number of voices at 1.0 to account for when scaling the mix down.
 *
 * Set the number of voices that all levels are adjusted to, the sum of voices
 * is multiplied by (1.0/voice_count). The default voice_count is 5.
 */
void        lyd_set_voice_count (Lyd *lyd, int voice_count);

/**
 * lyd_get_voice_count:
 * @lyd: lyd engine
 *
 * Returns: the current voice count.
 */
int         lyd_get_voice_count (Lyd *lyd);

/**
 * lyd_set_sample_rate:
 * 
 * @lyd: lyd engine
 * @sample_rate: new sample rate
 *
 * Set the sample rate of the lyd engine, this should be called before creating
 * any voices. The value should not be changed after initial setting and use.
 */
void        lyd_set_sample_rate (Lyd *lyd, int sample_rate);
/**
 * lyd_get_sample_rate:
 * @lyd: lyd engine
 *
 * Returns: the sample rate for a lyd engine.
 */
int         lyd_get_sample_rate (Lyd *lyd);
/**
 * LydFormat:
 *
 * The available sample rate representations that lyd can generate.
 */
typedef enum {
  LYD_f32,  /* 32bit floating point mono */
  LYD_f32S, /* 32bit floating point stereo, stream2 is used */
  LYD_s16S  /* 16bit signed integer stereo, interleaved on stream1*/
} LydFormat;
/**
 * lyd_set_format:
 * @lyd: lyd engine
 * @format: the sample format
 *
 * Specify the sample format lyd should produce.
 */
void        lyd_set_format      (Lyd *lyd, LydFormat format);
/**
 * lyd_get_format:
 * @lyd: lyd engine
 *
 * Get the current sample format used by lyd engine.
 */
LydFormat   lyd_get_format      (Lyd *lyd);

/**
 * lyd_synthesize:
 * @lyd: lyd engine
 * @len: number of samples to synthesize
 * @stream: output buffer stream (used for all mono, and 16bit interleaved stereo formats)
 * @stream2: second output buffer for floating point stereo
 *
 * Synthesize a number of samples from lyd writing into the buffers provided. You do not
 * should to call this if an audio output driver is used with lyd_audio_init.
 *
 * Returns: number of samples generated.
 */
long        lyd_synthesize      (Lyd *lyd, int len, void *stream, void *stream2);

/**
 * LydProgram:
 *
 * A compiled lyd program, that efficiently can be instantiated into a processing
 * LydVM for a voice or filter.
 */
typedef struct _LydProgram LydProgram;
/**
 * lyd_compile:
 * @lyd: lyd engine
 * @source: source code
 *
 * Compiles a \0 terminated string to a LydProgram, an intermediate compact binary form
 * that can be instantiated into voices with lyd_voice_new.
 *
 * Returns: a LydProgram if copmpilation was successful, NULL if compilation failed.
 */
LydProgram *lyd_compile         (Lyd *lyd, const char *source);
/**
 * lyd_program_free:
 * @program: a lyd program
 *
 * Frees all the data consumed by a LydProgram
 */
void        lyd_program_free    (LydProgram *program);

/**
 * LydVM:
 *
 * The VM that renders floating point data for voices and filters with lyd.
 */
typedef struct _LydVM LydVM;

/**
 * LydVoice:
 *
 * An voice that generates sound attached to a lyd engine, can be released
 * and have it's variables manipulated.
 */
typedef LydVM LydVoice;
/**
 * lyd_voice_new:
 * @lyd:     lyd engine
 * @program: a compiled lyd program to instantiate
 * @delay:   when to instantiate in seconds, 0.0 to instantiate now 
 * @tag:     an integer tag that can be used to kill many similar
 *           voices in one go
 *
 * Create a new voice, potentially delayed from a compiled LydProgram
 *
 * Returns: a LydVoice a fully opaque handle to a voice.
 */
LydVoice   *lyd_voice_new       (Lyd *lyd, LydProgram *program, double delay, int tag);
/**
 * lyd_voice_release:
 * @lyd: lyd engine
 * @voice: the voice to release
 *
 * Release a voice, this causes all ADSRs to decay, likely fading out the
 * signal. A released voice that goes quiet does not need to be killed.
 */
void        lyd_voice_release   (Lyd *lyd, LydVoice *voice);
/**
 * lyd_kill:
 * @lyd: lyd engine
 * @tag: tag to kill
 *
 * Kills (fully silences and frees up resources) voices that were registered
 * with the provided tag.
 */
void        lyd_kill            (Lyd *lyd, int tag);
/**
 * lyd_voice_kill:
 * @lyd:   lyd engine
 * @voice: voice handle
 *
 * Kill a specific lyd voice, if it has already been autokilled this is a nop.
 */
void        lyd_voice_kill      (Lyd *lyd, LydVoice *voice);
/**
 * lyd_voice_set_delay:
 * @lyd: lyd engine
 * @voice: voice handle
 * @seconds: new delay
 *
 * Specify the delay of the voice, overrides delay specified at creation.
 */

void        lyd_voice_set_delay (Lyd *lyd, LydVoice *voice, double seconds);
/**
 * lyd_voice_set_duration:
 * @lyd: lyd engine
 * @voice: voice handle
 * @duration: duration in seconds.
 *
 * Specifies that the voice should auto-release after the specified duration
 * has been elapsed (the duration is counter from after any potential delay.)
 */
void        lyd_voice_set_duration (Lyd *lyd, LydVoice *voice, double duration);
/**
 * lyd_voice_set_position:
 * @lyd: lyd engine
 * @voice: voice handle
 * @position: panning position between -1.0 and 1.0.
 *
 * Sets the stereo position of a voice, 0.0 is center.
 */
void        lyd_voice_set_position (Lyd      *lyd,
                                    LydVoice *voice,
                                    double    position);

/**
 * lyd_voice_set_param:
 * @lyd: lyd engine
 * @voice: voice handle
 * @param: name of parameter (variable in source code)
 * @value: new value for parameter
 *
 * Sets the value of a named variable in the program running in voice, any
 * non keyword string that appears in the lyd program source is accesible
 * as such a string.
 */
void        lyd_voice_set_param (Lyd        *lyd,   LydVoice *voice,
                                 const char *param, double value);
/**
 * LydInterpolation:
 *
 * The possible interpolation values to specify for lyd_voice_set_param_delayed
 * for interpolating values between specified key values.
 */
typedef enum {
  LYD_GAP,    /* all values in transition are 0.0 */
  LYD_STEP,   /* all values before value have previous value */
  LYD_LINEAR, /* slide linearly between values */
  LYD_CUBIC   /* slide smoothly between values */
} LydInterpolation;
/**
 * lyd_voice_set_param:
 * @lyd: lyd engine
 * @voice: voice handle
 * @param: name of parameter (variable in source code)
 * @time: time (from now)
 * @interpolation: type of interpolation from previous key up to time
 * @value: value at time.
 *
 * Sets the value of a named variable in the program running in voice at
 * a specific time in the future, any non keyword string that appears in the
 * lyd program source is accesible as such a string.
 */
void        lyd_voice_set_param_delayed (Lyd        *lyd,   LydVoice *voice,
                                         const char *param, double    time,
                                         LydInterpolation interpolation,
                                         double      value);

/**
 * lyd_vm_set_param:
 *
 * Similar to lyd_voice_set_param, but operates directly on a vm core
 * with a lock.
 */
void lyd_vm_set_param (LydVM      *vm,
                       const char *param,
                       double      value);

/**
 * lyd_vm_set_param_delayed:
 *
 * Similar to lyd_voice_set_param_delayed, but operates directly on a vm core
 * with a lock.
 */
void lyd_vm_set_param_delayed (LydVM *vm,
                               const char *param_name, double time,
                               LydInterpolation interpolation,
                               double      value);

/**
 * lyd_load_wave:
 * @lyd: lyd engine
 * @wavename: name for wave
 * @samples number of saples,
 * @sample_rate: sample rate of data
 * @data: PCM data
 *
 * Loads PCM data in memory into a the wavetable entry wavename, this
 * pcm data can be used as an oscillator with wave('wavename') and
 * wave_loop('wavename').
 */
void        lyd_load_wave (Lyd *lyd, const char *wavename,
                           int  samples, int sample_rate,
                           float *data);
/**
 * lyd_set_wave_handler:
 * @lyd: lyd engine
 * @wave_handler: function to handle missed wav loads
 * @user_data: user data to pass to the wave handler.
 *
 * Provide a function for lyd to call when non existing wavetable
 * entries are requested that do not exist. The lyd binary contains
 * a simple example of how sndfile can be used to load files from
 * the local file system. The wave handler should use lyd_load_wave
 * to specify the requested wave entry this is done on demand as
 * programs are compiled.
 */
void        lyd_set_wave_handler (Lyd *lyd,
                                  int (*wave_handler) (Lyd *lyd, const char *wave,
                                                       void *user_data),
                                  void *user_data);

/**
 * lyd_set_var_handler:
 * @lyd: lyd engine
 * @var_handler: function to execute when compiler sees a new variable.
 * @user_data: user data to pass to the wave handler.
 *
 * Used to know which variables are available for compiled programs, when
 * setting a new var_handler any previous var handler is removed, setting
 * NULL removes a previously set var handler.
 */
void        lyd_set_var_handler (Lyd *lyd,
                                 void (*var_handler) (Lyd *lyd,
                                                      const char *var,
                                                      double default_value,
                                                      void *user_data),
                                 void *user_data);

/**
 * LydFilter:
 *
 * A type that shares most API with LydVoice, but is used for standalone
 * processing outside the lyd core, this allows using lyd without an
 * attached stereo distribution and format conversion.
 */
typedef LydVM LydFilter;

/**
 * lyd_filter_new:
 * @lyd: lyd engine
 * @program: a compiled lyd program
 *
 * Create a new stand-alone processing element, useful to use the lyd
 * infrastructure to do ad-hoc processing on data.
 */
LydFilter  *lyd_filter_new      (Lyd *lyd, LydProgram *program);

/**
 * lyd_filter_process:
 * @filter: a filter handler
 * @inputs: pointer to array of input buffer pointers
 * @n_inputs: number of input buffers
 * @output: output buffer
 * @samples: number of samples in input/output buffers to process.
 *
 * Run a lyd vm directly on data. This is allows direct use of lyd's
 * processing core without the sound canvas. At the moment only filtering
 * with 1 input and 1 output is possible, the lyd program reads data
 * from input using the input() function and the results are written
 * to output.
 */
void        lyd_filter_process  (LydFilter *filter,
                                 float    **inputs,
                                 int        n_inputs,
                                 float     *output,
                                 int        samples);

/**
 * lyd_filter_free:
 * @filter: a LydFilter
 *
 * frees up a LydFilter instance.
 */
void        lyd_filter_free     (LydFilter *filter);

/**
 * lyd_set_global_filter:
 * @lyd: lyd engine
 * @program: a compiled LydProgram
 *
 * Set a lyd program to process all generated audio data through
 * after the voices have been mixed.
 */
void        lyd_set_global_filter (Lyd *lyd, LydProgram *program);

/**
 * lyd_add_pre_cb:
 * @lyd: lyd engine
 * @cb: callback to be invoked before new data is rendered
 * @data: user data
 *
 * Sets a callback function to be called just before generating
 * new samples. elapsed is the time in seconds elapsed since the
 * last call to lyd_synthesize.
 *
 * Returns: an integer handle that can be used to uninstall the handler.
 */
int          lyd_add_pre_cb (Lyd *lyd,
                             void (*cb)(Lyd *lyd, float elapsed, void *data),
                             void *data);
/**
 * lyd_add_post_cb:
 * @lyd: lyd engine
 * @cb: callback to be invoked after data has been rendered.
 * @data: user data
 *
 * Snoop on the generated data as it is generated.
 * Sets a callback function to be called after data have been
 * synthesized by lyd_synthesize and before control returns.
 *
 * Returns: an integer handle that can be used to uninstall the handler.
 */
int          lyd_add_post_cb (Lyd *lyd,
                              void (*cb)(Lyd *lyd, int samples,
                                         void *stream, void *stream2,
                                         void *data),
                              void *data);

/**
 * lyd_remove_cb:
 * @lyd: lyd engine
 * @id: callback id
 *
 * Remove a previously installed callback handler.
 */
void         lyd_remove_cb (Lyd *lyd, int id);


/**
 * lyd_audio_init:
 * @lyd: lyd engine
 * @driver: audio driver
 *
 * Initialize audio output subsystem, this is a global initialization that binds
 * to a lyd core, leaving driving of lyd_synthesize to the output driver.
 */
int          lyd_audio_init (Lyd *lyd, const char *driver); 


/**
 * lyd_get_patch:
 * @lyd: lyd engine
 * @no: patch number 0-127
 *
 * Get currently used patch.
 *
 * Returns: the patch/source code currently stored at location @no.
 */
const char * lyd_get_patch (Lyd *lyd, int no);

/**
 * lyd_set_patch:
 * @lyd: lyd engine
 * @no: patch number 0-127
 * @patch: new patch code
 *
 * Specify the patch code to use for slot_no no.
 */
void         lyd_set_patch (Lyd *lyd, int no, const char *patch);

/**
 * lyd_note:
 * @lyd: lyd engine
 * @patch: patch no
 * @hz: hz to play at,
 * @volume: volume to play at
 * @duration: duration to play for.
 *
 * Convenience call for music playing, using the built in patch set
 * create a note with the given properties.
 *
 * Returns: a voice handle.
 */
LydVoice *lyd_note (Lyd *lyd, int patch, float hz, float volume, float duration);

/**
 * lyd_note_full:
 * @lyd: lyd engine
 * @patch: patch no
 * @hz: hz to play at,
 * @volume: volume to play at
 * @duration: duration to play for.
 * @pan: position -1.0 .. 1.0  0.0 is center
 * @tag: tag to apply to created notes.
 *
 * Convenience call for music playing, using the built in patch set
 * create a note with the given properties.
 *
 * Returns: a voice handle.
 */
LydVoice *lyd_note_full (Lyd *lyd, int patch, float hz, float volume,
                         float duration, float pan, int tag);




/**
 * lyd_midi_load:
 * @lyd: lyd engine
 * @data: data
 * @length: length
 *
 * Load midi data from memory, this loads the MIDI data into a single
 * instance internal midi decoder in lyd that is derived from allegro,
 * NOTE: timinig of tempo is slightly broken for many midi files.
 */
void         lyd_midi_load (Lyd *lyd, unsigned char *data, int length);
/**
 * lyd_midi_set_playing:
 * @lyd: lyd engine
 * @playing: whether midi playback should be going
 */
void         lyd_midi_set_playing (Lyd *lyd, int playing);
/**
 * lyd_midi_out:
 * @lyd: lyd engine
 * @data: raw midi data
 * @length: length of raw midi data
 *
 * Injects raw midi events into midi decoder, allows changing properties
 * of playback or playing back notes.
 */
void         lyd_midi_out   (Lyd *lyd, unsigned char *data, int length);
/**
 * lyd_midi_seek:
 * @lyd: lyd engine
 * @position: new position in seconds
 *
 * Seek the position of the midi decoding engine to the given position.
 */
void         lyd_midi_seek  (Lyd *lyd, float position);

/**
 * lyd_add_op_program:
 * @lyd: lyd engine
 * @name: function name
 * @argc: number of arguments
 * @program: a LydProgram
 *
 * Adds program as a new op-code / language primitive. This allows
 * extending lyd with lyd.
 */
void         lyd_add_op_program (Lyd *lyd, const char *name, int argc,
                                 LydProgram *program);


#endif
