/*  This file defines the core lyd language, adding a new line here adds
 *  support to both virtual machine and compiler for the new instruction.
 *
 *  The macros used for OUT, ARG etc are documented at the top of lyd-vm.c
 *
 *  Instructions that need to be infix need special treatment in the compiler.
 */

/* LYD_OP(name, OP_CODE, ARGC, CODE, INIT, FREE, DOC, ARGDOC) */


LYD_OP("+", ADD, 2,
       OP_LOOP(OUT = ARG(0) + ARG(1);),;,;,
       "Adds values together <tt>value1 + value2</tt>", "")

LYD_OP("-", SUB, 2,
       OP_LOOP(OUT = ARG(0) - ARG(1);),;,;,
       "Subtracts values <tt>value1 - value2</tt>","")

LYD_OP("*", MUL, 2,
       OP_LOOP(OUT = ARG(0) * ARG(1);),;,;,
       "Multiplies values, useful for scaling amplitude  <tt>expression1 * expression2</tt>","")

LYD_OP("/", DIV, 2,
       OP_LOOP(OUT = ARG(1)!=0.0 ? ARG(0) / ARG(1):0.0;),;,;,
       "Divides values, <tt>value1 / value2</tt>","")

LYD_OP("min", MIN, 2,
       OP_LOOP(
         if (ARG(0) > ARG(1))
         OUT = ARG(1);
          else
         OUT = ARG(0);),;,;,
       "Returns the smallest of two values","(expression1, expression2)")

LYD_OP("max", MAX, 2,
       OP_LOOP(
         if (ARG(0) < ARG(1))
         OUT = ARG(1);
          else
         OUT = ARG(0);),;,;,
       "Returns the largest of two values","(expression1, expression2)")

LYD_OP("rcp", RCP, 1,
       OP_LOOP(OUT = 1.0/ARG(0);),;,;,
       "Returns the reciprocal (1/value)","(expression)")

LYD_OP("sqrt", SQRT, 1,
       OP_LOOP(OUT = sqrt(ARG(0));),;,;,
       "Performs a square root on the input value", "(expression)")

LYD_OP("^", POW, 2,
       OP_LOOP(OUT = powf (ARG(0), ARG(1));),;,;,
       "Raises the value1 to the power of value2, <tt>value1 ^ value2</tt>","") 

LYD_OP("%", MOD, 2,
       OP_LOOP(OUT = fmodf (ARG(0),ARG(1));),;,;,
       "Floating point modulus, <tt>value1 % value2</tt>","")

LYD_OP("abs", ABS, 1,
       OP_LOOP(OUT = fabsf (ARG(0));),;,;,
       "Makes the input value positive","(expression)")

LYD_OP("neg", NEG, 1,
       OP_LOOP(OUT = -ARG(0);),;,;,
       "Negates input value","(expression)") 

/* oscillators */

LYD_OP("sin", SIN, 1,
       OP_LOOP(OUT = sine (PHASE * M_PI * 2);),;,;,
       "Sine wave osicllator","(hz)")

LYD_OP("saw", SAW, 1,
       OP_LOOP(OUT = PHASE * 2 - 1.0;),;,;,
       "Sawtooth oscillator", "(hz)")

LYD_OP("ramp", RAMP, 1,
       OP_LOOP(OUT = -(PHASE * 2 - 1.0);),;,;,
       "Ramp oscillator, opposite of sawtooth.","(hz)")

LYD_OP("square", SQUARE, 1,
       OP_LOOP(OUT = PHASE > 0.5?1.0:-1.0;),;,;,
       "Square wave oscillator equivalent to a pulse with pulse width 0.5, values varying between -1.0 and 1.0","(hz)")

LYD_OP("triangle", TRIANGLE, 1,
       OP_LOOP(float p = PHASE;
               OUT = p < 0.25 ?  0 + p *4 : p < 0.75 ? 2 - p * 4: -4 + p * 4;),;,;,
       "Triangle waveform","(hz)")

LYD_OP("pulse", PULSE, 2,
       OP_LOOP(OUT = PHASE > ARG(1)?1.0:-1.0;),;,;,
       "Pulse oscillator to simulate square wave use a width of 0.5","(hz, duty)cycle)")

LYD_OP("noise", NOISE, 0,
       OP_FUN(op_noise),;,;,
       "Noise generator produces evenly distributed values in the range 0.0 to 1.0","()")

LYD_OP("input", INPUT, 1,
       OP_FUN(op_input),;,;,
       "Used when implementing filters, acts as a signal source", "(buffer_no)")

LYD_OP("inputp", INPUTP, 1,
       OP_FUN(op_inputp),;,;,
       "Used when implementing filters, acts as a signal source", "(buffer_no)")


LYD_OP("time", GTIME, 0,
       OP_LOOP(OUT = TIME;),;,;,
       "current time of sample running, in seconds","()")

LYD_OP("wave", WAVE, 2,
       OP_FUN(op_wave),;,;,
       "PCM data oscillator, first argument is a string, second argument if"
       " present is gives pitch deviation determined by desired playback hz"
       " assuming sample recorded is middle-C note that this does not work well"
       " if the hz deviation is larger than a couple of half notes,. <em>a relative speed would probably be better.</em>, wave('test.wav')"
       " wave('test.wav', 440.0)","('wave-identifier'[, hz])")

LYD_OP("wave_loop", WAVELOOP, 2,
       OP_FUN(op_wave_loop),;,;,
       "Like wave() but loops the given sample, needs to be scaled with an adsr"
       " to be silenced.","('test.wav', hz)")

LYD_OP("abssin", ABSSIN, 1,
       OP_LOOP(OUT = fabsf (sine (PHASE * M_PI * 2));),;,;,
       "OPL2 oscillator","(hz)")

LYD_OP("possin", POSSIN, 1,
       OP_LOOP(OUT = PHASE < 0.5 ? sine (PHASE_PEEK * M_PI * 2) : 0.0;),;,;,
       "OPL2 oscillator","(hz)")

LYD_OP("pulssin", PULSSIN, 1,
       OP_LOOP(OUT = fmodf (PHASE, 0.5) < 0.25 ?
                  fabsf (sine (PHASE_PEEK * M_PI * 2)) :
                  0.0;),;,;,
       "OPL2 oscillator","(hz)")

LYD_OP("evensin", EVENSIN, 1,
       OP_LOOP(OUT = PHASE < 0.5 ? sine (2 * PHASE_PEEK * M_PI * 2) : 0.0;),;,;,
       "OPL3 oscillator","(hz)")

LYD_OP("evenpossin", EVENPOSSIN, 1,
       OP_LOOP(OUT = PHASE < 0.5 ? fabs (sine (2 * PHASE_PEEK * M_PI * 2)) : 0.0;),;,;,
       "OPL3 oscillator","(hz)")

LYD_OP("adsr", ADSR, 4,
       OP_FUN (op_adsr),;,;,
       "ADSR Envelope - provides values in range 0.0-1.0 if oscillators are"
       " multiplied with an ADSR the amplitude will sink to 0.0 after release"
       " and the voice will be automatically freed after release when it no "
       " longer makes audio, <tt>sin(120)*adsr(0.3,0.3,0.8,1.5)</tt>",
       "(attack, decay, sustain, release)")

LYD_OP("ddadsr", DDADSR, 6,
       OP_FUN (op_ddadsr),;,;,
       "DDADSR Envelope - like ADSR, but with delay and duration first",
       "(delay, duration, atack, decay, sustain, release)")

LYD_OP("delay", DELAY, 2,
       OP_FUN (op_delay),;,
       op_free(state);,
       "Delay signal, slows down a signal by amount of time in seconds.",
       "(time, signal)")

LYD_OP("tapped_delay", TDELAY, 8,
       OP_FUN (op_tapped_delay),;,
       op_free(state);,
       "Delay signal, slows down a signal by amount of time in seconds, multiple delays can be done concurrently their results are averaged.",
       "(signal, tap1, [tap2..7])")

LYD_OP("echo", ECHO, 3,
       OP_FUN (op_echo),;,
       op_free(state);,
       "Echo filter, implements a single feedback delay line", "(amount, delay, signal)")

LYD_OP("tapped_echo", TECHO, 8,
       OP_FUN (op_tapped_echo),;,
       op_free(state);,
       "Delay signal, slows down a signal by amount of time in seconds, multiple delays can be done concurrently all their results are averaged for the result, the result is fed back to the delay line used.",
       "(signal, tap1, [tap2..7])")

LYD_OP("pluck", PLUCK, 3,
       OP_FUN (op_pluck),;,
       op_free(state);,
       "Plucked string, implements the decaying of the periodic wave form of a string using karplus strong algorithm, the decay ratio allows extending the duraiton of the decay in the range 1.0..., you can specify a custom waveform that is decayed by specifying a third argument with no third argument white noise is used., v", "(hz, [decayratio, [custom-waveform]])")

/* biquad frequency filters */
LYD_OP("low_pass", LOW_PASS, 4,
       OP_FUN (op_filter),;,op_filter_free(state);,
       "Low pass filter, for performance reasons the parameters of filters are not varying with sample accurate precision but vary per chunk of processed audio (at least for each 128 samples).",
       "(gain, hz, bandwidth, signal)")

LYD_OP("high_pass", HIGH_PASS, 4,
       OP_FUN (op_filter),;,op_filter_free(state);,
       "High pass filter",
       "(gain, hz, bandwidth, signal)")

LYD_OP("band_pass", BAND_PASS, 4,
       OP_FUN (op_filter),;,op_filter_free(state);,
       "Band pass filter",
       "(gain, hz, bandwidth, signal)")

LYD_OP("notch", NOTCH, 4,
       OP_FUN (op_filter),;,op_filter_free(state);,
       "notch filter",
       "(gain, hz, bandwidth, signal)")

LYD_OP("peak_eq", PEAK_EQ, 4,
       OP_FUN (op_filter),;,op_filter_free(state);,
       "peak eq filter",
       "(gain, hz, bandwidth, signal)")

LYD_OP("low_shelf", LOW_SHELF, 4,
       OP_FUN (op_filter),;,op_filter_free(state);,
       "low shelf filter",
       "(gain, hz, bandwidth, signal)")

LYD_OP("high_shelf", HIGH_SHELF, 4,
       OP_FUN (op_filter),;,op_filter_free(state);,
       "high shelf filter",
       "(gain, hz, bandwidth, signal)")

LYD_OP("mix", MIX, LYD_MAX_ARGC,
       OP_FUN (op_mix),;,;,
       "Mixes inputs averaging down amplitude", "(expr1,expr2[, ... expr8])")

LYD_OP("cycle", CYCLE, LYD_MAX_ARGC,
       OP_FUN (op_cycle),;,;,
       "Cycles between provided input streams first argument gives frequency of source hopping.",
       "(frequency, expr1, expr2[, ... expr7])")

LYD_OP("nop", NOP, 2,
       OP_LOOP(OUT = state->literal[0][i];),;,;,
       "returns the first of it's arguments used by the compiler to implement variables", "(value)")

LYD_OP("bar", BAR, 2,
       OP_LYD("square(input(0)) + sin(input(1))"),;,;,
       "", "(freq1, freq2)")
