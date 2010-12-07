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

static int lyd_op_argca[]=
{
  0
  #define LYD_OP(name, OP_CODE, ARG_COUNT, CODE, DOC, ARG_DOC), ARG_COUNT
  #include "lyd-ops.inc"
  #undef LYD_OP
  , -1
};

#ifdef LYD_EXTENDABLE
static LydOpInfo *lyd_op_info (Lyd *lyd, int op)
{
  SList *iter;
  for (iter = lyd->op_info; iter; iter = iter->next)
    {
      LydOpInfo *info = iter->data;
      if (info->op == op)
        return info;
    }
  return NULL;
}
#endif

static int
lyd_op_argc (Lyd *lyd, int op)
{
  if (op < LydLastOp)
    return lyd_op_argca[op];
#ifdef LYD_EXTENDABLE
  {
    LydOpInfo *info = lyd_op_info (lyd, op);
    if (info)
      return info->argc;
  }
#endif
  return 0;
}

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
    arg_count += lyd_op_argc (lyd, program->commands[opcount].op);
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
      int argc;
      states[i] = state;
      state->op = program->commands[i].op;
      state->argc = program->commands[i].argc;
      state->info = lyd_op_info (lyd, state->op);
      /* these argc's might differ, if we want stricter checking it
       * should happen foremost in the compiler
       */
      argc = lyd_op_argc (lyd, state->op);
      state->next = (LydOpState*)(((char *)state) +
                        sizeof (LydOpState) + (sizeof (LydChunk)) * argc);

      for (j = 0; j < argc; j++)
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

#define LOOKUP_BITS   13   /* simulated number of entires, lookuptable uses
                            * only 25% of the full size, 13 bits is 8192
                            * simulated entries, which would make the sine
                            * exact down to oscillators of 5.85hz for 48khz,
                            * with slower oscillations there would be repeated
                            * values.
                            */
#define LOOKUP_SIZE   (1<<LOOKUP_BITS)
#define LOOKUP_SIZE_REAL   (1<<(LOOKUP_BITS-2))
#define LOOKUP_MASK   (LOOKUP_SIZE-1)

static float lookup_inv_step;
static float sin_lookup[LOOKUP_SIZE_REAL];

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

    for (i = 0; i <= LOOKUP_SIZE_REAL; i++, f += step)
      sin_lookup[i] = sinf(f);
  }
}

/* inline lookuptable based sine function  */
static inline float sine (float a)
{
  int index = (((int)(a * lookup_inv_step)) & LOOKUP_MASK);
  /* reduce index down to first quarter */
  if (index >= LOOKUP_SIZE/2)
    {
      index = index & ((LOOKUP_SIZE/2)-1);
      if (index >= LOOKUP_SIZE/4)
        index = LOOKUP_SIZE/2-index;
      return -sin_lookup [index];
    }
  if (index >= LOOKUP_SIZE/4)
    index = LOOKUP_SIZE/2-index;
  return sin_lookup [index];
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
        case LYD_##OP_CODE: asm("#====LYDOPCODE " name);\
          { CODE } ;        asm("#====OPCODE END " name); \
          break;
        #include "lyd-ops.inc"
        /* the include expands into cases for opcodes and the code to
         * run when the opcode is invoked.
         */
        #undef LYD_OP
        break;
        default:
#ifdef LYD_EXTENDABLE
          if (state->info)
            { /* this lookup is terribly inefficient, a reference to the
             * opinfo should be stored in the state
             */
            if (state->info->process)
              state->info->process (vm, state, samples);
            else if (state->info->filter)
              lyd_filter_process (state->info->filter, state->arg,
                                  state->info->argc, state->out, samples);
          }
#endif
          break;
      }
  vm->sample += samples;
  return last_state->out;
}

#define STREQUAL(str1,str2) (fabs((str1)-(str2))<0.0000001)

void
lyd_vm_set_param (LydVM      *vm,
                  const char *param,
                  double      value)
{
  float hash = str2float (param);
  LydOpState *state;
  /* the variable constants are stored as a sequence of nops at the
   * beginning of the program
   */
  for (state=vm->state; state->op == LYD_NOP; state = state->next)
    {
      if (STREQUAL(state->literal[1 * LYD_CHUNK], hash))
        {
          int k;
          for (k = 0; k < LYD_CHUNK; k++)
            state->literal[k] = value;/* could set the out directly,
                                         and make nops be true no-ops? */
          break;
        }
    }
}

typedef struct _LydParam
{
  LydSample        param_name;   /* string hashed to float */
  long             sample_no;    /* which absolute lyd-time to set this param for*/
  LydSample        value;        /* the value set */
  LydInterpolation interpolation;/* Interpolation to use for this segment */
  LydSample       *ptr;          /* the data location written */
} LydParam;
#define LYD_PARAM(a) ((LydParam*)(a))

void lyd_vm_set_param_delayed (LydVM *vm,
                               const char *param_name, float        time,
                               LydInterpolation interpolation,
                               float       value)
{
  LydParam *param = g_new0 (LydParam, 1);
  int i;
  LydOpState *state;

  /* this should perhaps be different for standalone vms */
  param->sample_no = vm->lyd->sample_no + vm->sample_rate * time;

  param->param_name = str2float (param_name);
  param->value = value;
  param->interpolation = interpolation;

  for (state = vm->state; state->op == LYD_NOP; state = state->next)
    if (STREQUAL (state->literal[LYD_CHUNK * 1], param->param_name))
      {
        param->ptr = &(state->literal[0]);
        break;
      }

  if (vm->params)
    {
      SList *param_i, *param_key, *prev = NULL;

      /* find parameter sublist */
      for (param_i = vm->params;
           param_i && !STREQUAL(LYD_PARAM (((SList*)(param_i->data))->data)->param_name, param->param_name);
           param_i = param_i->next);

      /* find insertion point in sublist */
      for (param_key = param_i?param_i->data:NULL;
           param_key
        && LYD_PARAM (param_key->data)->sample_no < param->sample_no;
           param_key = param_key->next)
        prev = param_key;

      if (prev) /* */
         prev->next = slist_prepend (param_key, param);
      else
        {
          if (param_i)
            param_i->data = slist_prepend (param_key, param);
          else
            vm->params = slist_prepend (vm->params,
                                        slist_prepend (NULL, param));
        }
    }
  else
    vm->params = slist_prepend (NULL, slist_prepend (NULL, param));
}

static float
cubic (const float dx,
       const float prev, 
       const float j,
       const float next,
       const float nextnext)
{
  return (((( - prev + 3 * j - 3 * next + nextnext ) * dx +
            ( 2 * prev - 5 * j + 4 * next - nextnext ) ) * dx +
            ( - prev + next ) ) * dx + (j + j) ) / 2.0;
}

static void lyd_vm_update_params (LydVM *vm,
                                  int    samples)
{
  SList *paramlist;
  int j;

  for (j=0; j<samples; j++)
    {
      for (paramlist = vm->params;
           paramlist;
           paramlist = paramlist->next)
        {
          SList *i;
          LydParam *prev = NULL, *prev_prev = NULL;
          int freefirst = 0;

          for (i = paramlist->data; 
               i && LYD_PARAM (i->data)->sample_no < vm->lyd->sample_no + j;
               i = i->next)
            {
              prev_prev = prev;
              prev = i->data;
            }
          if (prev_prev && prev_prev != ((SList*)paramlist->data)->data)
            freefirst = 1;

          if (i && prev)
            {
              LydParam *curr = i->data;
              float     dt = curr->sample_no == prev->sample_no ? 1.0:
                             ((vm->lyd->sample_no + j) - prev->sample_no)
                            /(curr->sample_no - prev->sample_no * 1.0);

              switch (curr->interpolation)
                {
                  case LYD_LINEAR:
                    curr->ptr[j] = prev->value * (1.0-dt) + curr->value * dt;
                    break;
                  case LYD_GAP:
                    curr->ptr[j] = 0.0;
                    break;
                  case LYD_STEP:
                    curr->ptr[j] = dt < 0.9999?prev->value:curr->value;
                    break;
                  case LYD_CUBIC:
                   {
                    LydParam *next = i->next?i->next->data:curr;

                    if (!prev_prev)
                      prev_prev = prev;
                    curr->ptr[j] = cubic (dt, prev_prev->value, prev->value,
                                              curr->value, next->value);
                    break;
                   }
                }
            }

          if (freefirst) /* we trim away now unneeded items from the list */
            {            /* to speed up subsequent evaluation */
              SList *oldfirst = paramlist->data;
              paramlist->data = oldfirst->next;
              oldfirst->next = NULL;
              slist_free (oldfirst);
            }
        }
    }
}

LydFilter  *lyd_filter_new (Lyd *lyd, LydProgram *program)
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
  if (n_inputs > LYD_MAX_ARGC)
    n_inputs = LYD_MAX_ARGC;
  if (n_inputs > 0 && inputs)
    {
      int i;
      for (i = 0; i < n_inputs; i++)
        {
          filter->input_buf[i] = inputs[i];
          filter->input_pos[i] = 0;
        }
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

      lyd_vm_update_params (filter, chunk);
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
