/*  This file defines the core lyd language, adding a new line here adds
 *  support to both virtual machine and compiler for the new instruction.
 * 
 *  Resize your editor to avoid scrolling and wrapping
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

LYD_OP("nop",        NOP,        OP_LOOP OUT = state->literal[0][i]; OP_END,
       "Does nothing, used internally by compiler for literal values.",)
LYD_OP("+",          ADD,        OP_LOOP OUT = ARG(0) + ARG(1); OP_END,
       "Adds values together, infix",)
LYD_OP("-",          SUB,        OP_LOOP OUT = ARG(0) - ARG(1); OP_END,
       "Subtracts values, infix",)
LYD_OP("*",          MUL,        OP_LOOP OUT = ARG(0) * ARG(1); OP_END,         "Multiplies values, useful for scaling amplitude",)
LYD_OP("/",          DIV,        OP_LOOP OUT = ARG(1)!=0.0 ? ARG(0) / ARG(1):0.0;  OP_END ,"",)
LYD_OP("%",          MOD,        OP_LOOP OUT = fmod (ARG(0),ARG(1));         OP_END ,"",)
LYD_OP("abs",        ABS,        OP_LOOP OUT = fabs (ARG(0));                OP_END ,"",)
LYD_OP("pow",        POW,        OP_LOOP OUT = pow (ARG(0), ARG(1));         OP_END ,"",)
LYD_OP("sqrt",       SQRT,       OP_LOOP OUT = sqrt(ARG(0));                 OP_END ,"",)
LYD_OP("neg",        NEG,        OP_LOOP OUT = -ARG(0);                      OP_END ,"",)
LYD_OP("mix",        MIX,        OP_LOOP OUT = (ARG(0) + ARG(1))/2;          OP_END ,"",)
LYD_OP("mix3",       MIX3,       OP_LOOP OUT = (ARG(0) + ARG(1) + ARG(2))/3; OP_END ,"",)
LYD_OP("mix4",       MIX4,       OP_LOOP OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3))/4; OP_END ,"",)
/* oscillators */
LYD_OP("sin",        SIN,        OP_LOOP OUT = sin (PHASE * M_PI * 2); OP_END ,"",)
LYD_OP("saw",        SAW,        OP_LOOP OUT = PHASE * 2 - 1.0;        OP_END ,"",)
LYD_OP("ramp",       RAMP,       OP_LOOP OUT = -(PHASE * 2 - 1.0);     OP_END ,"",)
LYD_OP("square",     SQUARE,     OP_LOOP OUT = PHASE > 0.5?1.0:-1.0;   OP_END ,"",)
LYD_OP("triangle",   TRIANGLE,   OP_LOOP OUT = PHASE < 0.25 ? 0 + ARG(2)*4 : ARG(2)  < 0.75 ? 2 - ARG(2)*4 : -4 + ARG(2)*4; OP_END ,"",)
LYD_OP("pulse",      PULSE,      OP_LOOP OUT = PHASE > ARG(1)?1.0:-1.0;  OP_END ,"",)
LYD_OP("noise",      NOISE,      OP_LOOP OUT = g_random_double_range (-1.0, 1.0); OP_END ,"",)
LYD_OP("wave",       WAVE,       OP_LOOP OUT = wave_sample (voice, &ARG0(3), ARG0(0), ARG0(1));  OP_END ,"",)
LYD_OP("wave_loop",  WAVELOOP,   OP_LOOP OUT = wave_sample_loop (voice, &ARG0(3), ARG0(0), ARG0(1));  OP_END ,"",)
/* opl2 oscillators */
LYD_OP("abssin",     ABSSIN,     OP_LOOP OUT = fabs (sin (PHASE * M_PI * 2)); OP_END ,"",)
LYD_OP("possin",     POSSIN,     OP_LOOP OUT = PHASE < 0.5 ? sin (ARG0(2) * M_PI * 2) : 0.0; OP_END ,"",)
LYD_OP("pulssin",    PULSSIN,    OP_LOOP OUT = fmod (PHASE, 0.5) < 0.25 ? fabs (sin (ARG0(2) * M_PI * 2)) : 0.0; OP_END ,"",)
/* opl3 oscillators */
LYD_OP("evensin",    EVENSIN,    OP_LOOP OUT = PHASE < 0.5 ? sin (2 * ARG0(2) * M_PI * 2) : 0.0; OP_END ,"",)
LYD_OP("evenpossin", EVENPOSSIN, OP_LOOP OUT = PHASE < 0.5 ? fabs (sin (2 * ARG0(2) * M_PI * 2)) : 0.0; OP_END ,"",)

/* passing these arguments cause the macros used for implementing ops to be
 * valid also in the functions, convenience macros are provided for operating
 * on individual samples in the to be rendered segment.
 */
LYD_OP("adsr",       ADSR,       OP_LOOP   adsr         (voice, state, i);  OP_END ,"",)
LYD_OP("reverb",     REVERB,     OP_LOOP   voice_reverb (voice, state, i);  OP_END ,"",)
LYD_OP("cycle",      CYCLE,      OP_LOOP   voice_cycle  (voice, state, i);  OP_END ,"",)

/* biquad based frequency filters */
LYD_OP("low_pass",   LOW_PASS,   HANDLE_FILTER,"",)
LYD_OP("high_pass",  HIGH_PASS,  HANDLE_FILTER,"",)
LYD_OP("band_pass",  BAND_PASS,  HANDLE_FILTER,"",)
LYD_OP("notch",      NOTCH,      HANDLE_FILTER,"",)
LYD_OP("peak_eq",    PEAK_EQ,    HANDLE_FILTER,"",)
LYD_OP("low_shelf",  LOW_SHELF,  HANDLE_FILTER,"",)
LYD_OP("high_shelf", HIGH_SHELF, HANDLE_FILTER,"",)

