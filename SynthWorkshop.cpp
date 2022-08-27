#include "daisysp.h"
#include "daisy_seed.h"

// Shortening long macro for sample rate
#ifndef sample_rate

#endif

// Interleaved audio definitions
#define LEFT (i)
#define RIGHT (i + 1)
#define MAX_DELAY static_cast<size_t>(48000 * 0.75f)


using namespace daisysp;
using namespace daisy;
using namespace daisy::seed;

static DaisySeed  hw;
static AdEnv      env;
static Oscillator osc, osc2, osc3, osc4;
static Metro      tick;
static MoogLadder filter;
static WhiteNoise noise;
static DelayLine<float, MAX_DELAY> del;


// how many pots do we have?
const int num_adc_channels = 10;
enum AdcChannel {
	noise_knob = 0,
	osc_freq_knob,
	osc_knob2, // functionality is not yet defined
	del_time_knob,
	del_fdbk_knob,
	env_atk_knob,
	env_dec_knob,
	filter_freq_knob,
	filter_res_knob,
	main_volume_knob
};

struct IO_VALS{
	float noise = 0.5;
	float osc_freq = 440.0;
	float osc_knob2_val = 1.0;
	float del_time = 0.0;
	float del_fdbk = 0.0;
	float env_atk = 0.1;
	float env_dec = 0.4;
	float filter_freq = 5000.0;
	float filter_res = 0.0;
	float main_volume = 0.5;
} io_vals;

void configADC()
{
	// here's where all mapping of hw pins happen
	AdcChannelConfig adc_config[num_adc_channels];
	adc_config[noise_knob].InitSingle(A9);
	adc_config[osc_freq_knob].InitSingle(A8);
	adc_config[osc_knob2].InitSingle(A7);
	adc_config[del_time_knob].InitSingle(A6);
	adc_config[del_fdbk_knob].InitSingle(A5);
	adc_config[env_atk_knob].InitSingle(A4);
	adc_config[env_dec_knob].InitSingle(A3);
	adc_config[filter_freq_knob].InitSingle(A2);
	adc_config[filter_res_knob].InitSingle(A1);
	adc_config[main_volume_knob].InitSingle(A0);

	hw.adc.Init(adc_config, num_adc_channels);
}

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    float osc_out, env_out, filter_out, del_out, sig_out, feedback;
    for(size_t i = 0; i < size; i += 2)
    {
        // When the metro ticks, trigger the envelope to start.
        tick.SetFreq(io_vals.osc_knob2_val);

        float notes[5] = {130.81, 146.83, 164.81, 174.61, 196.00};
        if(tick.Process())
        {
            int freq = rand() % 5;
            osc.SetFreq(notes[freq] + io_vals.osc_freq);
            osc2.SetFreq(notes[freq] + 110.0 + io_vals.osc_freq);
            osc3.SetFreq(notes[freq] - 110.0 + io_vals.osc_freq);
            osc4.SetFreq(notes[freq] + 220.0 + io_vals.osc_freq);
            env.Trigger();
        }

        // ENV
        env.SetTime(ADENV_SEG_ATTACK, io_vals.env_atk);
        env.SetTime(ADENV_SEG_DECAY, io_vals.env_dec);
        env_out = env.Process();
        
        // OSC
        osc.SetAmp(env_out * io_vals.main_volume);
        osc2.SetAmp(env_out * io_vals.main_volume);
        osc3.SetAmp(env_out * io_vals.main_volume);
        osc4.SetAmp(env_out * io_vals.main_volume);
        // osc.SetFreq(io_vals.osc_freq);
        noise.SetAmp(env_out * io_vals.noise * io_vals.main_volume);
        osc_out = osc.Process() + noise.Process() + osc2.Process() + osc3.Process() + osc4.Process();


        // Read from delay line
        del_out = del.Read();
        // Calculate output and feedback
        sig_out  = del_out + osc_out;
        feedback = (del_out * io_vals.del_fdbk) + osc_out;
        del.SetDelay(48000 * io_vals.del_time);
        // Write to the delay
        del.Write(feedback);

        // FILTER
        filter.SetFreq(io_vals.filter_freq);
        filter.SetRes(io_vals.filter_res);
        filter_out = filter.Process(sig_out);

        // MAIN OUT
        out[LEFT]  = filter_out * io_vals.main_volume;
        out[RIGHT] = filter_out * io_vals.main_volume;
    }
}

int main(void)
{
    // initialize seed hardware and daisysp modules
    float sample_rate;
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    sample_rate = hw.AudioSampleRate();
    
    // OSC setup
    osc.Init(sample_rate);
    osc.SetWaveform(osc.WAVE_SAW);
    osc.SetFreq(io_vals.osc_freq);

    osc2.Init(sample_rate);
    osc2.SetWaveform(osc.WAVE_SAW);

    osc3.Init(sample_rate);
    osc3.SetWaveform(osc.WAVE_SAW);

    osc4.Init(sample_rate);
    osc4.SetWaveform(osc.WAVE_SAW);
    
    // Noise setup
    noise.Init();
    noise.SetAmp(io_vals.noise);

    // ENV setup
    env.Init(sample_rate);
    env.SetTime(ADENV_SEG_ATTACK, io_vals.env_atk);
    env.SetTime(ADENV_SEG_DECAY, io_vals.env_dec);
    env.SetMin(0.0);
    env.SetMax(1.0);
    env.SetCurve(0); // linear

    // Filter setup
    filter.Init(sample_rate);
    filter.SetFreq(io_vals.filter_freq);
    filter.SetRes(io_vals.filter_res);

    // DELAY
    del.SetDelay(sample_rate * 0.75f);

    // temporarily use osc knob while we don't have 
    // dedicated knob for this
    tick.Init(io_vals.osc_knob2_val, sample_rate);

	// config and start adc reading
	configADC();
	hw.adc.Start();

    // start callback
    hw.StartAudio(AudioCallback);
    // hw.StartLog();

    while(1) {
		// read pots and assign to synth parameters
        // OSC
        io_vals.noise = 1.0 - hw.adc.GetFloat(noise_knob);
        io_vals.osc_freq = 
            493.88 - fmap(hw.adc.GetFloat(osc_freq_knob), 0.0, 493.88, Mapping::EXP);
        io_vals.osc_knob2_val = 
            5.0 - fmap(hw.adc.GetFloat(osc_knob2), 0.0001, 5.0, Mapping::EXP);

        // ENV
        io_vals.env_atk = 
            2.0 - fmap(hw.adc.GetFloat(env_atk_knob), 0.01, 2.0, Mapping::EXP);
        io_vals.env_dec = 
            2.0 - fmap(hw.adc.GetFloat(env_dec_knob), 0.01, 2.0, Mapping::EXP);            

        // FILTER
        io_vals.filter_freq = 
            5000.0 - fmap(hw.adc.GetFloat(filter_freq_knob), 0.0, 5000.0, Mapping::EXP);
        io_vals.filter_res = 
            1.0 - fmap(hw.adc.GetFloat(filter_res_knob), 0.0, 1.0, Mapping::EXP);

        // DELAY
        io_vals.del_time = 1.0 - hw.adc.GetFloat(del_time_knob);
        io_vals.del_fdbk = 1.0 - hw.adc.GetFloat(del_fdbk_knob);

        // MAIN VOLUME
		io_vals.main_volume = 1.0 - hw.adc.GetFloat(main_volume_knob);

        //10 msec delay to reduce noisy readings on analog pots
        //System::Delay(10);
	}
}
