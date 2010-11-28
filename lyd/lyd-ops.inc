/*  This file defines the core lyd language, adding a new line here adds
 *  support to both virtual machine and compiler for the new instruction.
 *
 *  Instructions that need to be infix need special treatment in the compiler.
 */

LYD_OP("nop", NOP,        
       OP_LOOP(OUT = state->literal[0][i];),
       "Does nothing, used internally by compiler for literal values.",)

LYD_OP("+", ADD,
       OP_LOOP(OUT = ARG(0) + ARG(1);),
       "Adds values together, value1 + value2",)

LYD_OP("-", SUB,
       OP_LOOP(OUT = ARG(0) - ARG(1);),
       "Subtracts values, value1 - value2",)

LYD_OP("*", MUL,
       OP_LOOP(OUT = ARG(0) * ARG(1);),
       "Multiplies values, useful for scaling amplitude, value1 * value2",)

LYD_OP("/", DIV,
       OP_LOOP(OUT = ARG(1)!=0.0 ? ARG(0) / ARG(1):0.0;),
       "Divides values, value1 / value2",)

LYD_OP("%", MOD,
       OP_LOOP(OUT = fmod (ARG(0),ARG(1));),
       "Floating point modulus, value1 % value2",)

LYD_OP("abs", ABS,
       OP_LOOP(OUT = fabs (ARG(0));),
       "Makes the input value positive, abs(value)",)

LYD_OP("^", POW,
       OP_LOOP(OUT = pow (ARG(0), ARG(1));),
       "Raises the value1 to the power of value2, value1 ^ value2 ",) 

LYD_OP("sqrt", SQRT,
       OP_LOOP(OUT = sqrt(ARG(0));),
       "Performs a square root on the input value, sqrt(expression)",)

LYD_OP("neg", NEG,
       OP_LOOP(OUT = -ARG(0);),
       "Negates input value, neg (foo) ",) /* XXX: is this even neccesary, isn't -(expr) enough? */

LYD_OP("mix", MIX,
       OP_LOOP(OUT = (ARG(0) + ARG(1))/2;),
       "Mixes two inputs averaging down amplitude, mix(a,b)",) /* should be made generic like cycle */

LYD_OP("mix3", MIX3,
       OP_LOOP(OUT = (ARG(0) + ARG(1) + ARG(2))/3;),
       "Mixes three inputs evenly",)

LYD_OP("mix4", MIX4,
       OP_LOOP(OUT = (ARG(0) + ARG(1) + ARG(2) + ARG(3))/4;),
       "Mixes four inputs evenly",)

/* oscillators */
LYD_OP("sin", SIN,
       OP_LOOP(OUT = sin (PHASE * M_PI * 2);),
       "Sine wave osicllator, sin(hz)",)

LYD_OP("saw", SAW,
       OP_LOOP(OUT = PHASE * 2 - 1.0;),
       "Sawtooth oscillator, saw(hz)",)

LYD_OP("ramp", RAMP,
       OP_LOOP(OUT = -(PHASE * 2 - 1.0);),
       "Ramp oscillator, opposite of sawtooth.",)

LYD_OP("square", SQUARE,
       OP_LOOP(OUT = PHASE > 0.5?1.0:-1.0;),
       "Square wave oscillator equivalent to a pulse with pulse width 0.5, square(hz)",)

LYD_OP("triangle", TRIANGLE,
       OP_LOOP(OUT = PHASE < 0.25 ?
                       0 + ARG(2)*4 :
                       ARG(2)  < 0.75 ? 2 - ARG(2)*4 : -4 + ARG(2)*4;),
       "Triangle waveform, triangle(hz)",)

LYD_OP("pulse", PULSE,
       OP_LOOP(OUT = PHASE > ARG(1)?1.0:-1.0;),
       "Pulse oscillator to simulate square wave use:, pulse(hz, 0.5) ",)

LYD_OP("noise", NOISE,
       OP_LOOP(OUT = g_random_double_range (-1.0, 1.0);),
       "Noise generator produces evenly distributed values in the range -1.0 to 1.0",)

LYD_OP("wave", WAVE,
       OP_LOOP(OUT = wave_sample (voice, &ARG0(3), ARG0(0), ARG0(1));),
       "PCM data oscillator, first argument is a string, second argument if"
       " present is gives pitch deviation determined by desired playback hz"
       " assuming sample recorded is middle-C, wave('test.wav') or"
       " wave('test.wav', 440.0)",)

LYD_OP("wave_loop", WAVELOOP,
       OP_LOOP(OUT = wave_sample_loop (voice, &ARG0(3), ARG0(0), ARG0(1));),
       "Like wave() but loops the given sample, needs to be scaled with an adsr"
       " be silenced, wave_loop('test.wav') * adsr(0.2,0.8,0.3,0.2)",)

LYD_OP("abssin", ABSSIN,
       OP_LOOP(OUT = fabs (sin (PHASE * M_PI * 2));),
       "OPL2 oscillator",)

LYD_OP("possin", POSSIN,
       OP_LOOP(OUT = PHASE < 0.5 ? sin (ARG0(2) * M_PI * 2) : 0.0;),
       "OPL2 oscillator",)

LYD_OP("pulssin", PULSSIN,
       OP_LOOP(OUT = fmod (PHASE, 0.5) < 0.25 ?
                  fabs (sin (ARG0(2) * M_PI * 2)) :
                  0.0;),
       "OPL2 oscillator",)

LYD_OP("evensin", EVENSIN,
       OP_LOOP(OUT = PHASE < 0.5 ? sin (2 * ARG0(2) * M_PI * 2) : 0.0;),
       "OPL3 oscillator",)

LYD_OP("evenpossin", EVENPOSSIN,
       OP_LOOP(OUT = PHASE < 0.5 ? fabs (sin (2 * ARG0(2) * M_PI * 2)) : 0.0;),
       "OPL3 oscillator",)

/* The rest of the native ops are implemented as functions with
 * void op_fun (voice, state, samples) are implementations that
 * themselves iterate an i or directly access op command state
 */

LYD_OP("adsr", ADSR,
       OP_FUN (op_adsr),
       "ADSR Envelope - provides values in range 0.0-1.0 if oscillators are"
       " multiplied with an ADSR the amplitude will sink to 0.0 after release"
       " and the voice will be automatically freed after release when it no "
       " longer makes audio, adsr(a, d, s, r), sin(120)*adsr(0.3,0.3,0.8,1.5)",)

LYD_OP("reverb", REVERB,
       OP_FUN (op_reverb),
       "Reverb filter",)

LYD_OP("cycle", CYCLE,
       OP_FUN (op_cycle),
       "Cycles between provided input streams first argument gives frequency of source hopping.",)

/* biquad based frequency filters */
LYD_OP("low_pass", LOW_PASS,
       OP_FUN (op_filter),
       "Low pass filter, low_pass(gain, hz, bandwidth)",)

LYD_OP("high_pass", HIGH_PASS,
       OP_FUN (op_filter),
       "High pass filter, high_pass(gain, hz, bandwidth)",)

LYD_OP("band_pass", BAND_PASS,
       OP_FUN (op_filter),
       "Band pass filter, band_pass(gain, hz, bandwidth)",)

LYD_OP("notch", NOTCH,
       OP_FUN (op_filter),
       "notch filter, notch(gain, hz, bandwidth)",)

LYD_OP("peak_eq", PEAK_EQ,
       OP_FUN (op_filter),
       "peak eq filter, peak_eq(gain, hz, bandwidth)",)

LYD_OP("low_shelf", LOW_SHELF,
       OP_FUN (op_filter),
       "low shelf filter, low_shelf(gain, hz, bandwidth)",)

LYD_OP("high_shelf", HIGH_SHELF,
       OP_FUN (op_filter),
       "high shelf filter, high_shelf(gain, hz, bandwidth)",)

