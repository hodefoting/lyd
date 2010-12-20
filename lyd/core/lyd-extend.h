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

/* this header includes the symbols needed for adding processing functions
 * to lyd, at either compile or runtime.
 */

#ifndef _LYD_EXTEND_H_
#define _LYD_EXTEND_H_

#include <lyd.h>

#define LYD_MAX_ARGC                   8   /* maximum number of arguments */
#define LYD_CHUNK                    128   /* ideal internal processing size */

typedef float  LydSample;

//typedef union { LydSample v[LYD_CHUNK]; } LydChunk;
typedef union { LydSample v[LYD_CHUNK]; } 
LydChunk __attribute__ ((__aligned__(LYD_ALIGN)));

typedef struct _LydOpState  LydOpState;
typedef struct _LydOpInfo   LydOpInfo;

struct _LydOpState
{
  int         op;     /* the opcode used */
  void       *data;   /* hook for ops to add their own data structures */
  int         argc;   /* number of arguments actually passed */
  LydSample   phase;  /* phase, used by oscillator ops */
  LydOpState *next;   /* op to run after this one */
  LydSample  *out;
  int         out_is_clone;
#ifdef LYD_EXTENDABLE
  LydOpInfo  *info;
#endif
  LydSample  *arg[LYD_MAX_ARGC]; /* points either to own literals, or other op
                                    outputs */
  LydSample  *literal[LYD_MAX_ARGC];
};

/**
 * lyd_add_op_op:
 * @lyd: lyd engine
 * @name: function name
 * @argc: number of arguments
 * @process: function to do processing.
 *
 * Adds an external op_code function, allowing to
 * extend lyd with new core functionality dynamically.
 */
void         lyd_add_op         (Lyd *lyd, const char *name, int argc,
                                 void (*process) (LydVM *vm, LydOpState *state, int samples),
                                 void (*init) (LydVM *vm, LydOpState *state),
                                 void (*free) (LydVM *vm, LydOpState *state));

  /* Shortcut code used for implementing ops, keeping them short concise
   * and reabable. They assume that local variables arg0 to arg07 contain
   * pointers to aligned memory arrays.
   */

  /* macro used when defining ops, to indicate a callback function to
   * be used for processing
   */
  #define OP_FUN(fun)        fun(vm, state, vm->sample, samples);

  #define OP(CODE) {ALIGNED_ARGS CODE ALIGNED_ARGS_SILENCE;}

  /* define a loop with all it's needed variables running the code provided
   * as an argument
   */
  #define OP_LOOP(CODE) \
    OP(register int i; for (i = 0; i < samples; i++) { CODE } ;)

  /* used to define a lyd that is statically compiled (needs to be sharable)*/
  #define OP_LYD(LYD_CODE)\
  {\
    static LydFilter  *filter = NULL;\
    if (!filter)\
      {\
        LydProgram *program = lyd_compile (vm->lyd, LYD_CODE);\
        if (!program)\
          break;\
        filter = lyd_filter_new (vm->lyd, program);\
        lyd_program_free (program);\
      }\
    lyd_filter_process (filter, state->arg, lyd_op_argca[state->op], state->out, samples);\
  }

  #define ARG0(no) arg##no->v[0]

  /* Get, and advance the phase for an oscillator: uses ARG0 because the state
   * for phase is carried per sample in chunk buffer  */
  #define PHASE_PEEK state->phase

  /* increments, and returns current phase */
  #define PHASE      phase(vm, state, ARG(0))

  /* macros depending on i pointing to right index to work, needs
   * a loop over the samples to work properly
   */
  /* The output sample */
  #define OUT      out->v[i]

  /* the current value of an input stream  */
  #define ARG(no)  arg##no->v[i]

  /* pointer to data extension point */
  #define DATA     state->data

  /* The current sample being computed */
  #define SAMPLE   (vm->sample + i)
  /* get the current time in seconds */
  #define TIME     (1.0 * SAMPLE * vm->i_sample_rate)
  /* the output destination for the current sample */

  /* Macro that expands to the local arrays expected to exist for the macros
   * from the LydState, these variables provide extra information to gcc about
   * the 16 byte alignment of the memory addresses.
   */
  #define ALIGNED_ARGS \
    LydChunk * __restrict__ arg0 = (void*)(state->arg[0]);\
    LydChunk * __restrict__ arg1 = (void*)(state->arg[1]);\
    LydChunk * __restrict__ arg2 = (void*)(state->arg[2]);\
    LydChunk * __restrict__ arg3 = (void*)(state->arg[3]);\
    LydChunk * __restrict__ arg4 = (void*)(state->arg[4]);\
    LydChunk * __restrict__ arg5 = (void*)(state->arg[5]);\
    LydChunk * __restrict__ arg6 = (void*)(state->arg[6]);\
    LydChunk * __restrict__ arg7 = (void*)(state->arg[7]);\
    LydChunk * __restrict__ out  = (void*)(state->out);
  /* to remove warnings about unused vars, gcc optimizes this away for us */
  #define ALIGNED_ARGS_SILENCE \
    arg0=arg1=arg2=arg3=arg4=arg5=arg6=arg7


#ifndef ABS
  #define ABS(a)             ((a)>0?(a):-(a))
#endif

 /* define with the expected arguments to OP_FUN functions,
  * makes other uses terse and allows changing it in one place */
#define OP_ARGS LydVM *vm, LydOpState *state, int sample, int samples

#endif
