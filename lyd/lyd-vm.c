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

/* this file is included from lyd.c, but it could be compiled indepentenly
 * from it exporting a small set of symbols.
 */

  /* Shortcut code used for implementing ops, keeping them short concise
   * and reabable. They assume that local variables arg0 to arg07 contain
   * aligned memory arrays, LydChunks.
   */
typedef union { LydSample v[LYD_CHUNK]; }
LydChunk __attribute__ ((__aligned__(LYD_ALIGN)));


  /* macro used when defining ops, to indicate a callback function to
   * be used for processing
   */
  #define OP_FUN(fun)        fun(vm, state, samples);

  /* define a loop with all it's needed variables running the code provided
   * as an argument
   */
  #define OP_LOOP(CODE) \
    ALIGNED_ARGS \
    register int i;\
    for (i = 0; i < samples; i++) { CODE } ; \
    ALIGNED_ARGS_SILENCE;


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
  #define DATA               state->data

  /* The current sample being computed */
  #define SAMPLE   (vm->sample + i)
  /* get the current time in seconds */
  #define TIME     (1.0 * SAMPLE * vm->i_sample_rate)
  /* the output destination for the current sample */

  /* Macro that expands to the local arrays expected to exist
   * for the macros from the LydState
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
    LydChunk * __restrict__ out  = (void*)(state->out);\
  /* to remove warnings about unused vars, gcc optimizes this away for us */
  #define ALIGNED_ARGS_SILENCE \
    arg0=arg1=arg2=arg3=arg4=arg5=arg6=arg7



#ifndef ABS
  #define ABS(a)             ((a)>0?(a):-(a))
#endif

 /* define with the expected arguments to OP_FUN functions,
  * makes other uses terse and allows changing it in one place */
#define OP_ARGS LydVM *vm, LydOpState *state, int samples

#include <stdint.h>

static int lyd_op_argc[]=
{
  0
  #define LYD_OP(name, OP_CODE, ARG_COUNT, CODE, DOC, ARG_DOC), ARG_COUNT
  #include "lyd-ops.inc"
  #undef LYD_OP
  , -1
};

/* create a new vm from a program */
static LydVM * lyd_vm_create (Lyd *lyd, LydProgram *program)
{
  LydVM *vm;
  static LydSample nul = 0.0;
  int i, j;
  LydOpState *state;
  LydOpState *states[LYD_MAX_ELEMENTS];

  /* compute size of allocation */
  int opcount;
  int arg_count = 0;
  int codesize;
  for (opcount = 0; program->commands[opcount].op; opcount++)
    arg_count += lyd_op_argc[program->commands[opcount].op];
  opcount++;
  codesize = sizeof (LydOpState) * opcount + sizeof (LydChunk) * arg_count;

  /* allocate memory */
  vm = g_malloc0 (sizeof (LydVM) + codesize + LYD_ALIGN);
  vm->state = (LydOpState*)(((char *)vm) + sizeof (LydVM));

  /* ensure 16 byte alignment of LydOpState array */
  {
    char *tmp = (void*)vm->state;
    int offset = LYD_ALIGN - ((uintptr_t) tmp) % LYD_ALIGN;
    tmp += offset;
    vm->state = (void*)tmp;
  }

  vm->lyd = lyd;
  state = vm->state;

  /* fill in opstate from program, initializing
   * everything needed to start running the vm */
  for (i = 0; program->commands[i].op; i++)
    {
      states[i] = state;
      state->op = program->commands[i].op;
      state->argc = program->commands[i].argc;
      state->next = 
          (LydOpState*)(((char *)state) +
                        sizeof (LydOpState) +
                        (sizeof (LydChunk)) * lyd_op_argc[state->op]);

      for (j = 0; j < lyd_op_argc[state->op]; j++)
        {
          int offset  = program->commands[i].arg[j];
          int k;

          for (k = 0; k < LYD_CHUNK; k++)
            state->literal[j * LYD_CHUNK + k] = program->commands[i].arg[j];
          if (offset >= 0) /* direct pointer */
            {
              state->arg[j] = &state->literal[j * LYD_CHUNK];
              for (k = 0; k < LYD_CHUNK; k++)
                state->out[k] = state->arg[j][0];
            }
          else if (i + offset >= 0)
            {
              state->arg[j] = &states[i + offset]->out[0];
            }
          else
            state->arg[j] = &nul;
          assert (state->arg[j]);
        }

      switch (program->commands[i].op)
        {
          case LYD_ADSR:
          { /* premultiply with sample rate, simplifying runtime behavior  */
            int k;
            for (k = 0; k < LYD_CHUNK; k++)
              {
                state->literal[0 * LYD_CHUNK + k] *= lyd->sample_rate;
                state->literal[1 * LYD_CHUNK + k] *= lyd->sample_rate;
                state->literal[3 * LYD_CHUNK + k] *= lyd->sample_rate;
              }
            break;
          }
          case LYD_DDADSR:
          { /* premultiply with sample rate, simplifying runtime behavior  */
            int k;
            for (k = 0; k < LYD_CHUNK; k++)
              {
                state->literal[0 * LYD_CHUNK + k] *= lyd->sample_rate;
                state->literal[1 * LYD_CHUNK + k] *= lyd->sample_rate;
                state->literal[2 * LYD_CHUNK + k] *= lyd->sample_rate;
                state->literal[3 * LYD_CHUNK + k] *= lyd->sample_rate;
                state->literal[5 * LYD_CHUNK + k] *= lyd->sample_rate;
              }
            break;
          }
        }
      state = state->next;
    }
  vm->position = 0.0;
  return vm;
}

static void
lyd_vm_free (LydVM *vm)
{
  LydOpState *state;
  for (state = vm->state; state->op; state=state->next)
    if (state->data)
      switch (state->op)
        {
          case LYD_LOW_PASS:  case LYD_HIGH_PASS: case LYD_BAND_PASS:
          case LYD_NOTCH:     case LYD_PEAK_EQ:
          case LYD_LOW_SHELF: case LYD_HIGH_SHELF:
            free (state->data);
            break;
          case LYD_REVERB:
          default:
            g_free (state->data);
        }
  {
    SList *l1, *l2;
    for (l1 = vm->params; l1; l1 = l1->next)
      {
        for (l2 = l1->data; l2; l2 = l2->next)
          g_free (l2->data);
        slist_free (l1->data);
      }
    slist_free (vm->params);
  }
  g_free (vm);
}

/* sine function lookup function, not very precise at the moment */

#define LOOKUP_BITS   11   /* configuration of size of lookup-table */
#define LOOKUP_SIZE   (1<<11)
#define LOOKUP_MASK   (LOOKUP_SIZE-1)

static float lookup_inv_step;
static float sin_lookup[LOOKUP_SIZE];

void lyd_init_lookup_tables (void)
{
  static int done = 0;

  if (done)
    return;
  done = 1;

  {
    unsigned int i;
    float step;
    float f = 0;
    step = M_PI * 2.0f / (float)LOOKUP_SIZE;
    lookup_inv_step = 1.0f / step;

    for (i = 0; i <= LOOKUP_SIZE; i++, f += step)
      sin_lookup[i] = sinf(f);
  }
}
/* inline lookuptable based versions version */
static inline float sine (float a)
{
  /* reduce size of sin lookup table by folding/mirroring the index value
   * with further arithmetic exploiting symmetry.
   */
  return sin_lookup [((int)(a * lookup_inv_step)) & LOOKUP_MASK];
}

static inline float phase (LydVM *vm, LydOpState *state, float hz)
{
  float old = state->phase;
  float new = old + hz * vm->i_sample_rate;
  int newi = new;
  state->phase = new - newi;
  return old;
}

#include "lyd-ops.c" /* include lyd-ops.c directly from the C file so
                        that the static functions can be compiled directly
                        into lyd_vm_compute() */

/* The core virtual machine, it computes maximum LYD_CHUNK
 * samples in one go, a pointer to the result is returned.
 */
static inline LydSample *
lyd_vm_compute (LydVM  *vm,
                int     samples)
{
  LydOpState *state, *last_state = NULL;
  for (state = vm->state; state->op; last_state = state, state=state->next)
    switch (state->op)
      {
        case LYD_NONE: break;
#define LYD_OP(name, OP_CODE, ARGC, CODE, DOC, BAZ) \
        case LYD_##OP_CODE: asm("#====LYDOPCODE " name);{ CODE } ; asm("#=====OPCODE END " name); break;
        #include "lyd-ops.inc"
        /* the include expands into cases for opcodes and the code to
         * run when the opcode is invoked.
         */
        #undef LYD_OP
      }
  vm->sample += samples;
  return last_state->out;
}


LydFilter  *lyd_filter_new      (Lyd *lyd, LydProgram *program)
{
  LydVM *filter;
  filter = lyd_vm_create (lyd, program);
  filter->sample_rate = lyd->sample_rate;
  filter->i_sample_rate = 1.0/lyd->sample_rate;
  return filter;
}

void
lyd_filter_process (LydFilter  *filter,
                    LydSample **inputs,
                    int         n_inputs,
                    LydSample  *output,
                    int         samples)
{
  int left = samples;
  int pos = 0;

  filter->sample_rate = filter->lyd->sample_rate;
  filter->i_sample_rate = 1.0/filter->lyd->sample_rate;
  if (n_inputs > 1 && inputs)
    {
      filter->input_buf[0] = inputs[0];
      filter->input_pos = 0;
      filter->input_buf_len = samples;
    }

  while (left)
    {
      int i;
      LydSample *result = NULL;
      int chunk = LYD_CHUNK;
      if (chunk > left)
        chunk = left;
      left -= chunk;

      lyd_voice_update_params (filter, chunk);
      result = lyd_vm_compute (filter, chunk);
      for (i = 0; i< chunk; i++)
        output[pos+i] = result[i];
      pos += chunk;
    }
}

void lyd_filter_free (LydFilter *filter)
{
  lyd_vm_free (filter);
}
