/*  This file defines the core lyd language, adding a new line here adds
 *  support to both virtual machine and compiler for the new instruction.
 *
 */

#ifndef HANDLE_FILTER
  #define HANDLE_FILTER 
#endif
#ifndef REP
  #define REP
#endif
#ifndef PER
  #define PER
#endif

LYD_OP("nop",        NOP,
    OUT = state->literal[0][INDEX];,
    "Does nothing",)

LYD_OP("+",          ADD,
    OUT = ARG(0) + ARG(1);,
    "Adds values together",)

LYD_OP("-",          SUB,
 REP   OUT = ARG(0) - ARG(1);  PER,,)


LYD_OP("*",          MUL,    OUT = ARG(0) * ARG(1);              ,,)
LYD_OP("/",          DIV,    ARG(1)!=0.0 ? ARG(0) / ARG(1):0.0;  ,,)
LYD_OP("%",          MOD,    OUT = fmod (ARG(0),ARG(1));         ,,)
LYD_OP("abs",        ABS,    OUT = fabs (ARG(0));                ,,)
LYD_OP("pow",        POW,    OUT = pow (ARG(0), ARG(1));         ,,)
LYD_OP("sqrt",       SQRT,   OUT = sqrt(ARG(0));                 ,,)
LYD_OP("neg",        NEG,    OUT = -ARG(0);                      ,,)
LYD_OP("mix",        MIX,    OUT = (ARG(0) + ARG(1))/2;          ,,)
LYD_OP("mix3",       MIX3,   OUT = (ARG(0) + ARG(1) + ARG(2))/3; ,,)
LYD_OP("mix4",       MIX4,   OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3))/4; ,,)

/* oscillators */

LYD_OP("sin",        SIN,      OUT = sin (PHASE * M_PI * 2); ,,)
LYD_OP("saw",        SAW,      OUT = PHASE * 2 - 1.0;        ,,)
LYD_OP("ramp",       RAMP,     OUT = -(PHASE * 2 - 1.0);     ,,)
LYD_OP("square",     SQUARE,   OUT = PHASE > 0.5?1.0:-1.0;   ,,)
LYD_OP("triangle",   TRIANGLE, PHASE < 0.25 ? 0 + ARG(2)*4 : ARG(2)  < 0.75 ? 2 - ARG(2)*4 : -4 + ARG(2)*4; ,,)
LYD_OP("pulse",      PULSE,    OUT = PHASE > ARG(1)?1.0:-1.0;  ,,)
LYD_OP("noise",      NOISE,    OUT = g_random_double_range (-1.0, 1.0); ,,)

LYD_OP("wave",       WAVE,      OUT = wave_sample (voice, &ARG(3), ARG(0), ARG(1));  ,,)
LYD_OP("wave_loop",  WAVELOOP,  OUT = wave_sample_loop (voice, &ARG(3), ARG(0), ARG(1));  ,,)

/* opl2 oscillators */
LYD_OP("abssin",     ABSSIN,   OUT = fabs (sin (PHASE * M_PI * 2)); ,,)
LYD_OP("possin",     POSSIN,   OUT = PHASE < 0.5 ? sin (ARG(2) * M_PI * 2) : 0.0; ,,)
LYD_OP("pulssin",    PULSSIN,  OUT = fmod (PHASE, 0.5) < 0.25 ? fabs (sin (ARG(2) * M_PI * 2)) : 0.0; ,,)
/* opl3 oscillators */

LYD_OP("evensin",    EVENSIN,    OUT = PHASE < 0.5 ? sin (2 * ARG(2) * M_PI * 2) : 0.0; ,,)
LYD_OP("evenpossin", EVENPOSSIN, OUT = PHASE < 0.5 ? fabs (sin (2 * ARG(2) * M_PI * 2)) : 0.0; ,,)

/* passing these arguments cause the macros used for implementing ops to be
 * valid also in the functions, convenience macros are provided for operating
 * on individual samples in the to be rendered segment.
 */
LYD_OP("adsr",       ADSR,      adsr         (voice, state, i); ,,)
LYD_OP("reverb",     REVERB,    voice_reverb (voice, state, i); ,,)
LYD_OP("cycle",      CYCLE,     voice_cycle  (voice, state, i); ,,)

/* biquad based frequency filters */
LYD_OP("low_pass",   LOW_PASS,   HANDLE_FILTER,,)
LYD_OP("high_pass",  HIGH_PASS,  HANDLE_FILTER,,)
LYD_OP("band_pass",  BAND_PASS,  HANDLE_FILTER,,)
LYD_OP("notch",      NOTCH,      HANDLE_FILTER,,)
LYD_OP("peak_eq",    PEAK_EQ,    HANDLE_FILTER,,)
LYD_OP("low_shelf",  LOW_SHELF,  HANDLE_FILTER,,)
LYD_OP("high_shelf", HIGH_SHELF, HANDLE_FILTER,,)

