void welcome2 (Lyd *lyd)
{ 
  LydVoice   *voice;
  LydProgram *program;
  program = lyd_compile (lyd, "(sin(hz=440 + saw(20)*10)) * volume=1");

  voice = lyd_new_voice (lyd, program, 0);
  lyd_voice_set_param (lyd, voice, "volume", 1.2);
  lyd_voice_set_duration (lyd, voice, 10.0);
  lyd_voice_set_delay (lyd, voice, 2.0);

  lyd_voice_set_param_delayed (lyd, voice, "volume", 2.0, LYD_CUBIC, 0.0);
  lyd_voice_set_param_delayed (lyd, voice, "volume", 3.0, LYD_CUBIC, 1.0);
  lyd_voice_set_param_delayed (lyd, voice, "volume", 4.0, LYD_CUBIC, 1.2);
  lyd_voice_set_param_delayed (lyd, voice, "volume", 6.0, LYD_LINEAR, 0.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 2.0, LYD_CUBIC, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 2.1, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 2.2, LYD_CUBIC, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 2.3, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 2.4, LYD_CUBIC, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 3.0, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 4.0, LYD_GAP, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 5.0, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 6.0, LYD_CUBIC, 440.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 7.0, LYD_CUBIC, 880.0);
  lyd_voice_set_param_delayed (lyd, voice, "hz", 8.0, LYD_CUBIC, 440.0);

  lyd_program_free (program);
}

void welcome (Lyd *lyd)
{ 
  LydVoice   *voice;
  LydProgram *program;
  program = lyd_compile (lyd, "reverb (0.2, 0.123, low_pass (1.3, hz=440.0, 0.03, pulse(200 + sin(190) * 1.4, sin(0.1)) * adsr(0.12, 0.12, 0.7, 0.30) * volume=1.0))");

#define Q(delay, duration, frequency) \
  voice = lyd_new_voice (lyd, program, 0);\
  lyd_voice_set_param (lyd, voice, "volume", 0.8);\
  lyd_voice_set_param (lyd, voice, "hz",     frequency);\
  lyd_voice_set_duration (lyd, voice, duration);\
  lyd_voice_set_delay (lyd, voice, delay);
  Q(0.0, 0.3, 440.0);
  Q(0.1, 0.2, 660.0);
  Q(0.2, 0.1, 880.0);
  Q(0.3, 0.1, 440.0);
  Q(0.4, 0.1, 880.0);
  Q(0.5, 0.1, 440.0);
  Q(1.0, 0.15, 440.0);
  Q(1.2, 0.15, 500.0);
  Q(1.4, 0.15, 600.0);
  Q(1.6, 0.15, 700.0);
  Q(1.8, 0.15, 800.0);
#undef Q
  lyd_program_free (program);
  welcome2(lyd);
}
