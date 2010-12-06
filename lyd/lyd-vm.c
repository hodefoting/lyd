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

#include "biquad.c"

typedef union { LydSample v[LYD_CHUNK]; }
LydChunk __attribute__ ((__aligned__(LYD_ALIGN)));

#include <stdint.h>

/* create a new vm from a program */
static LydVM * lyd_vm_create (Lyd *lyd, LydProgram *program)
{
  LydVM *vm;
  static LydSample nul = 0.0;
  int i, j;
  LydOpState *state;
  LydOpState *states[LYD_MAX_ELEMENTS];

  int opcount;
  int arg_count = 0;
  int codesize;
  for (opcount = 0; program->commands[opcount].op; opcount++)
    arg_count += lyd_op_argc[program->commands[opcount].op];
  opcount++;

  codesize = sizeof (LydOpState) * opcount + sizeof (LydChunk) * arg_count;
  vm = g_malloc0 (sizeof (LydVM) + codesize + LYD_ALIGN);
  vm->state = (LydOpState*)(((char *)vm) + sizeof (LydVM));

  /* ensure 16 byte alignment of LydOpStates */
  {
    char *tmp = (void*)vm->state;
    int offset = LYD_ALIGN - ((uintptr_t) tmp) % LYD_ALIGN;
    tmp += offset;
    vm->state = (void*)tmp;
  }

  vm->lyd = lyd;
  state = vm->state;

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

/* we store a phase incrementer in each op, allowing us to change
 * the hz of a signal generator on the fly without sudden phase shifts.
 */
static inline float phase (LydVM *vm, LydOpState *state, float hz)
{
  float old = state->phase;
  float new = old + hz * vm->i_sample_rate;
  int newi = new;
  state->phase = new - newi;
  return old;
}


static inline float input_sample (LydVM *vm,
                                  int    no)
{
  float ret = 0.0;
  if (!vm->input_buf)
    return 0.0;
  if (vm->input_pos < vm->input_buf_len)
    ret = vm->input_buf[no][vm->input_pos];
  vm->input_pos ++;
  return ret;
}

#define MIDDLE_C 261.625565

static inline float wave_sample (LydVM *vm, LydOpState *state, int no, float hz)
{
  LydWave *wave = vm->lyd->wave[no];
  float old = state->phase;
  float delta = vm->i_sample_rate * (hz>0.001?hz/MIDDLE_C:1.0);
  float new = old + delta;
  int sample_pos = new * wave->sample_rate;
  state->phase = new;
  if (sample_pos < wave->samples)
    return wave->data[sample_pos];
  return 0.0;
}

static inline float wave_sample_loop  (LydVM *vm, LydOpState *state, int no, float hz)
{
  LydWave *wave = vm->lyd->wave[no];
  float old = state->phase;
  float delta = vm->i_sample_rate * (hz>0.001?hz/MIDDLE_C:1.0);
  float new = old + delta;
  int sample_pos = new * wave->sample_rate;
  state->phase = new;
  if (sample_pos < wave->samples)
    return wave->data[sample_pos];
  state->phase = 0.0;
  return 0.0;
}

  #define OUT      out->v[i]  
  #define ARG(no)  arg##no->v[i]
  #define ARG0(no) arg##no->v[0]
  /* Shortcut code used for implementing ops, keeping them short concise
   * and reabable.
   */

  /* Use only the first slot for argument, used for storing persistent
   * state for the op in the data of the arguments.
   */

  /* Get, and advance the phase for an oscillator: uses ARG0 because the state
   * for phase is carried per sample in chunk buffer  */
  #define PHASE_PEEK state->phase

  /* increments, and returns current phase */
  #define PHASE      phase(vm, state, ARG(0))

  /* pointer to data extension point */
  #define DATA               state->data

  /* macro used when defining ops, to indicate a callback function to
   * be used for processing
   */
  #define OP_FUN(fun)        fun(vm, state, samples);

  /* macro used to define looping code, this defines the variable i
   * used in some of the other macros.
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

  #define OP_LOOP(CODE) \
    ALIGNED_ARGS \
    register int i;\
    for (i = 0; i < samples; i++) { CODE } ; \
    ALIGNED_ARGS_SILENCE;

  /* macros depending on i pointing to right index to work, needs
   * a loop over the samples to work properly
   */
  /* The current sample being computed */
  #define SAMPLE             (vm->sample + i)
  /* get the current time in seconds */
  #define TIME               (1.0 * SAMPLE * vm->i_sample_rate)
  /* the output destination for the current sample */

#ifndef ABS
  #define ABS(a)             ((a)>0?(a):-(a))
#endif

#define OP_ARGS LydVM *vm, LydOpState *state, int samples

static inline void op_filter (OP_ARGS)
{
  ALIGNED_ARGS;
  int i = 0;
  if (G_UNLIKELY (!DATA))
    DATA = BiQuad_new(state->op-LYD_LOW_PASS,ARG0(0),ARG0(1), vm->sample_rate, ARG0(2));\
  
  /* always updating the filter is expensive, so we do it once per chunk  */
  BiQuad_update (DATA,state->op-LYD_LOW_PASS,ARG0(0),ARG0(1),vm->sample_rate,ARG0(2));
  for (i=0; i<samples; i++)
    {
      OUT = BiQuad(ARG(3), DATA);
    }
  ALIGNED_ARGS_SILENCE;
}


static inline void op_ddadsr (OP_ARGS)
{
  ALIGNED_ARGS;
  int i;
  LydSample delay = ARG0(0),
            duration = ARG0(1),
            a = ARG0(2),
            d = ARG0(3),
            s = ARG0(4),
            r = ARG0(5);
  for (i=0; i<samples; i++)
    {
      int sample = SAMPLE - delay;

      if (sample < 0)
        {
          OUT = 0.0;
        }
      else if (sample > duration)
        {
          int released = sample - duration;
          if (released > r) /* after end of release */
            {
              OUT = 0.0;
            }
          else
            {
              float released_val;
              if ((sample - released) <= a)  /* release in attack*/
                released_val = (((vm->sample - delay)- released) / a) * ((sample - released) / a);
              else if (sample - released < (a+d))/*release in decay */
                released_val = 1.0 + (s-1) * (((sample - released) - a) / d);
              else                                      /*release in sustain */
                released_val = s;
              OUT = released_val * (1.0 - (released) / r);
            }
        }
      else if (sample <= a)                        /* in attack */
        OUT = (sample / a) * (sample / a);
      else if (sample < a + d)                     /* in decay */
        OUT = 1.0 + (s-1) * ((sample - a) / d);
      else                                         /* in sustain */
        OUT = s; 
    }
}

static inline void op_adsr (OP_ARGS)
{
  ALIGNED_ARGS;
  int i;
  LydSample a = ARG0(0),
            d = ARG0(1),
            s = ARG0(2),
            r = ARG0(3);
  for (i=0; i<samples; i++)
    {

      if (vm->released)
        {
          if (vm->released > r) /* after end of release */
            {
              OUT = 0.0;
            }
          else
            {
              float released_val;
              if ((SAMPLE - vm->released) <= a)     /* release in attack*/
                released_val = ((vm->sample - vm->released) / a) * ((SAMPLE - vm->released) / a);
              else if (SAMPLE - vm->released < (a+d))/*release in decay */
                released_val = 1.0 + (s-1) * (((SAMPLE - vm->released) - a) / d);
              else                                      /*release in sustain */
                released_val = s;
              OUT = released_val * (1.0 - (vm->released) / r);
            }
        }
      else if (SAMPLE <= a)                        /* in attack */
        OUT = (SAMPLE / a) * (SAMPLE / a);
      else if (SAMPLE < a + d)                     /* in decay */
        OUT = 1.0 + (s-1) * ((SAMPLE - a) / d);
      else                                         /* in sustain */
        OUT = s;                               
    }
}

typedef struct _ReverbData 
{
   int    pos;
   int    size;
   LydSample *old;
} ReverbData;

static inline void op_reverb (OP_ARGS)
{
  ReverbData *data   = state->data;
  ALIGNED_ARGS;
  int i;
  for (i=0; i<samples; i++)
    {
      LydSample   reverb = ARG(0),
                  length = ARG(1),
                  sample = ARG(2);
      int         size   = length * vm->sample_rate;

      if (size <=0)
        return;

      if (G_UNLIKELY (size > LYD_MAX_REVERB_SIZE))
        size = LYD_MAX_REVERB_SIZE;

      if (G_UNLIKELY (data == NULL ||
          size != data->size))
        {
          data = state->data = g_malloc0 (sizeof (LydSample) *size + sizeof(ReverbData));
          data->size = size;
          data->old = (void*)((  ((char*)(data)) + sizeof (ReverbData)));
        }

      sample = sample + data->old[data->pos] * reverb;
      data->old[data->pos++] = sample / (1.0 + reverb);
      if (G_UNLIKELY (data->pos >= size))
        data->pos = 0;
      OUT = sample;
    }
  ALIGNED_ARGS_SILENCE;
}

static inline void op_cycle (OP_ARGS)
{
  ALIGNED_ARGS;
  int i, pos, count;
  LydSample freq = ARG0(0);

  if (state->argc < 2)
    {
      OUT = 0;
      return;
    }

  count = state->argc - 1;

  for (i = 0; i < samples; i++)
    {
      pos   = fmod (freq * count * SAMPLE / vm->sample_rate, count);

      switch (1 + (pos+count) % count)
        {
          case 1: OUT = ARG(1); break;
          case 2: OUT = ARG(2); break;
          case 3: OUT = ARG(3); break;
          case 4: OUT = ARG(4); break;
          case 5: OUT = ARG(5); break;
          case 6: OUT = ARG(6); break;
          case 7: OUT = ARG(7); break;
        }
    }
}

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

static inline float noise (void)
{
  static int seed = 1996;
  float rand;
  const int ia = 853, im = 981287;
  seed = (seed*ia)%im;
  rand = ((float) seed - 0.5)/((float) (im - 1));
  return rand;
}

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
