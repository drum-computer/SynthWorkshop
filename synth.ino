////////////////////////////////////////////////////////////////
///////////////////// VARIABLES & LIBRARIES ////////////////////

#include "DaisyDuino.h"

static Oscillator osc1, osc2, osc3;
static MoogLadder filter;
static AdEnv env; 
static WhiteNoise noise;

// OSC block knobs
const int osc_freq_coarse_knob =  A8; // 38 || Implemented
float osc1_freq_coarse = 0.0;
float osc2_freq_coarse = 0.0;
float osc3_freq_coarse = 0.0;
const int osc_freq_fine_knob =    A7; // 37
float osc_freq_fine = 0.0;
const int osc_freq_env_amt_knob = A9; // 39
float osc_freq_env_amt = 0.0;
float noise_level = 0.0;

// LFO block
const int lfo_freq_knob =         A6; // 36
float lfo_freq = 0.0;
const int lfo_speed_knob =        A5; // 35
float lfo_speed = 0.0;

// ENV block
const int env_atk_knob =          A4; // 34
float env_atk = 0.0;
const int env_dec_knob =          A3; // 33
float env_dec = 0.0;
uint64_t env_retrigger_period = 3000; // milliseconds
uint64_t current_time = 0; 

// Filter block
const int filt_freq_knob =        A2; // 32
float filt_freq = 0.0;
const int filt_res_knob =         A1; // 31
float filt_res = 0.0;


// Main output
const int main_vol_knob =         A0; // 30 || Implemented
float main_vol = 0.0;

////////////////////////////////////////////////////////////////
///////////////////// START SYNTH SETUP ////////////////////////
float simpleAnalogRead(const int pin) {
  return (1023.0 - (float)analogRead(pin)) / 1023.0;
}

void setup() {
  // DAISY SETUP
  DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  float sample_rate = DAISY.get_samplerate();

  // OSCILLATOR SETUP
  osc1.Init(sample_rate);
  osc1.SetFreq(200);
  osc1.SetAmp(0.3);
  osc1.SetWaveform(Oscillator::WAVE_SAW);

  osc2.Init(sample_rate);
  osc2.SetFreq(200);
  osc2.SetAmp(0.3);
  osc2.SetWaveform(Oscillator::WAVE_SAW);
  
  osc3.Init(sample_rate);
  osc3.SetFreq(200);
  osc3.SetAmp(0.3);
  osc3.SetWaveform(Oscillator::WAVE_SAW);

  // NOISE SETUP
  noise.Init();
  
  // FILTER SETUP
  filter.Init(sample_rate);

  // ENV SETUP
  env.Init(sample_rate);
  //env.SetTime(ADENV_SEG_ATTACK, 1.0);
  env.SetTime(ADENV_SEG_DECAY, 0.5);  

  // DAISY SETUP
  DAISY.begin(ProcessAudio);
}


///////////////////// END SYNTH SETUP //////////////////////////
////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////
///////////////////// PROCESSING AUDIO SAMPLES (LOOP) //////////


void ProcessAudio(float **in, float **out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    float osc_mix = (osc1.Process() + osc2.Process() + osc3.Process() + noise.Process());
    float osc_mix_env = osc_mix * env.Process();
    float sample = filter.Process(osc_mix_env);
    out[0][i] = sample;
    out[1][i] = sample;
  }
}


////////////////////////////////////////////////////////////////
///////////////////// START CONTROLS LOOP //////////////////////


void loop() {


  
  if ((millis() - current_time) >= env_retrigger_period)
  {
    current_time = millis();
    env.Trigger();
  }
  // read and map values
  // main volume 0.0 - 1.0
  main_vol = simpleAnalogRead(main_vol_knob);
  // filter frequesncy 0.0 - 5000.0
  filt_freq = fmap(simpleAnalogRead(filt_freq_knob), 0, 5000, Mapping::EXP);
  // filter resonance 0.0 - 0.8
  filt_res = simpleAnalogRead(filt_res_knob) / 1.25;
  // env_retrigger_period
  env_retrigger_period = fmap(simpleAnalogRead(osc_freq_fine_knob), 50, 3000, Mapping::EXP);
  // env 0-2 secs
  env_atk = fmap(simpleAnalogRead(env_atk_knob), 0, 2, Mapping::EXP);
  env_dec = fmap(simpleAnalogRead(env_dec_knob), 0, 2, Mapping::EXP);
  // osc freq
  osc1_freq_coarse = fmap(simpleAnalogRead(osc_freq_coarse_knob), 0, 2500, Mapping::EXP);
  osc2_freq_coarse = fmap(simpleAnalogRead(osc_freq_coarse_knob), 0, 2200, Mapping::EXP);  
  osc3_freq_coarse = fmap(simpleAnalogRead(osc_freq_coarse_knob), 0, 2000, Mapping::EXP);
  // noise level
  noise_level = fmap(simpleAnalogRead(osc_freq_env_amt_knob), 0, 1);
  
  // OSC control
  osc1.SetFreq(osc1_freq_coarse);
  osc1.SetAmp(main_vol);
  osc2.SetFreq(osc2_freq_coarse);
  osc2.SetAmp(main_vol);
  osc3.SetFreq(osc3_freq_coarse);
  osc3.SetAmp(main_vol);

  // Noise control
  noise.SetAmp(noise_level);
  
  // Filter control
  filter.SetFreq(filt_freq);
  filter.SetRes(filt_res < 0.0 ? 0.0 : filt_res);

  // ENV control
  env.SetTime(ADENV_SEG_ATTACK, env_atk);
  env.SetTime(ADENV_SEG_DECAY, env_dec);
  
}