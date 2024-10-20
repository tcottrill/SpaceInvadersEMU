#define NOMINMAX
#include "framework.h"
#include "mixer.h"
#include "wavfile.h"
#include "fileio.h"
#include "dsoundmain.h"
#include "dsoundstream.h"
#include <cstdint>
#include <math.h>
#include <list>
#include <vector>
#include <stdexcept>        // std::out_of_range

#pragma warning( disable : 4996 4244)

static int BUFFER_SIZE = 0;
static int SYS_FREQ = 22050;
#define SMP_START 0x2c
#define MAX_CHANNELS   12 // 0-7 game, 8-9 system, 10-11 streaming
#define MAX_SOUNDS     130 // max number of sounds loaded in system at once 128 + 2 overloaded for streams
#define SOUND_NULL     0
#define SOUND_LOADED   1
#define SOUND_PLAYING  2
#define SOUND_STOPPED  3
#define SOUND_PCM      4
#define SOUND_STREAM   5

static double dbvolume[101];
short int* soundbuffer;
CHANNEL channel[MAX_CHANNELS];
SAMPLE sound[MAX_SOUNDS];

//List of actively playing samples
std::list<int> audio_list;
//List of loaded samples, so we can track, call by name, and delete when done;
std::vector<SAMPLE> lsamples;

inline float dBToAmplitude(float db)
{
	return pow(10.0f, db / 20.0f);
}

void buildvolramp() //This only goes down, not up. Need to build an up vol to 200 percent as well
{
	dbvolume[0] = 0;
	double k = 0;

	for (int i = 99; i > 0; i--)
	{
		k = k - .44f;
		dbvolume[i] = dBToAmplitude(k);
		//wrlog("Value at %i is %f", i, dbvolume[i]);
	}

	dbvolume[100] = 1.00;
}

unsigned char Make8bit(signed short sample)
{
	sample >>= 8;  // drop the low 8 bits
	sample ^= 0x80;  // toggle the sign bit
	return (sample & 0xFF);
}

short Make16bit(unsigned char sample)
{
	short sample16 = (short)(sample - 0x80) << 8;
	return sample16;
}

static void byteswap(unsigned char& byte1, unsigned char& byte2)
{
	byte1 ^= byte2;
	byte2 ^= byte1;
	byte1 ^= byte2;
}
//static string Functions

std::string remove_extension(const std::string& filename) {
	size_t lastdot = filename.find_last_of(".");
	if (lastdot == std::string::npos) return filename;
	return filename.substr(0, lastdot);
}

std::string base_name(const std::string& path)
{
	return path.substr(path.find_last_of("/\\") + 1);
}

bool ends_with(const std::string& s, const std::string& ending)
{
	return (s.size() >= ending.size()) && equal(ending.rbegin(), ending.rend(), s.rbegin());
}

//This function takes a loaded 8 bit MONO sample and upscales it to 16 bit so it can be mixed.
int sample_up16(int num)
{
	int	old_id = num;              //ID of original sound
	int new_id = -1;               // return val from create sample;
	SAMPLE p;
	SAMPLE n;

	p = sound[old_id];
	//new_id = create_sample(p.name, 16, p.channels, p.sampleRate, p.sampleCount);
	new_id = create_sample(16, p.channels, p.sampleRate, p.sampleCount);
	n = sound[new_id];

	//Copy all the data to the new buffer and upscale it.
	for (unsigned long i = 0; i < sound[old_id].sampleCount; i++)
	{
		sound[new_id].data.u16[i] = (int16_t)(((sound[old_id].data.u8[i] - 128) << 8));
	}
	sound[old_id].channels = 0;
	sound[old_id].sampleRate = 0;
	sound[old_id].bitPerSample = 0;
	sound[old_id].dataLength = 0;
	sound[old_id].sampleCount = 0;
	sound[old_id].state = SOUND_NULL;
	sound[old_id].name = "";
	sound[old_id].num = -1;
	if (sound[old_id].data.buffer)   //Delete the old data
	{
		free(sound[old_id].data.buffer);
	}
	return new_id;
}

int mixer_init(int rate, int fps)
{
	int i = 0;
		 
	BUFFER_SIZE = rate / fps;
	SYS_FREQ = rate;
	soundbuffer = (short*) malloc (BUFFER_SIZE * 2);
	memset(soundbuffer, 0, BUFFER_SIZE * 2);
	HRESULT DSI = dsound_init(rate, 1); //Start the directsound engine

	if (DSI != DS_OK)
	{
		wrlog("Dsound create error , sound system not available.");
		goto error;
	}

	stream_init(rate, 1);  //Start a stream playing for our data
	osd_set_mastervolume(-3); //Set volume

	wrlog("Buffer size is %d", BUFFER_SIZE);

	//Clear and init Sample Channels
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		channel[i].loaded_sample_num = -1;
		channel[i].state = SOUND_STOPPED;
		channel[i].looping = 0;
		channel[i].pos = 0;
		channel[i].vol = 1.0;
		//sample_set_volume(i, 100);
		wrlog("Channel default volume is %f", channel[i].vol);
	}

	//Set all samples to empty for start
	for (i = 0; i < MAX_SOUNDS; i++)
	{
		sound[i].state = SOUND_NULL;
	}
	buildvolramp();
	return 1;
error:
	return 0;
}

int load_sample(const char* archname, const char* filename)
{
	int	sound_id = -1;      // id of sound to be loaded
	int  index;               // looping variable
	// step one: are there any open id's ?
	for (index = 0; index < MAX_SOUNDS; index++)
	{
		// make sure this sound is unused
		if (sound[index].state == SOUND_NULL)
		{
			sound_id = index;
			break;
		} // end if
	} // end for index
	  // did we get a free id? If not,fail.
	if (sound_id == -1) {
		wrlog("No free sound id's for sample %s", filename); return(-1);
	}
	//SOUND

	wrlog("Loading file %s with sound id %d", filename, sound_id);

	unsigned char* sample_temp;
	HRESULT result;
	//LOAD FILE - Please add some error handling here!!!!!!!!!
	if (archname)
	{
		sample_temp = load_generic_zip(archname, filename);
		//Create Wav data
		result = WavFileLoadInternal(sample_temp, get_last_zip_file_size());
	}
	else
	{
		sample_temp = load_file(filename);
		//Create Wav data
		result = WavFileLoadInternal(sample_temp, get_last_file_size());
	}

	//If sample loaded successfully proceed!

	// set rate and size in data structure
	sound[sound_id].sampleRate = Wave.sampleRate;
	sound[sound_id].bitPerSample = Wave.bitPerSample;
	sound[sound_id].dataLength = Wave.dataLength;
	sound[sound_id].sampleCount = Wave.sampleCount;
	sound[sound_id].state = SOUND_LOADED;
	sound[sound_id].name = filename;

	wrlog("File %s loaded with sound id: %d and state is: %d", filename, sound_id, sound[sound_id].state);
	wrlog("Loading WAV #: %d", sound_id);
	wrlog("Channels #: %d", Wave.channels);
	wrlog("Samplerate #: %d", Wave.sampleRate);
	wrlog("Length #: %d", Wave.dataLength);
	wrlog("BPS #: %d", Wave.bitPerSample);
	wrlog("Samplecount #: %d", Wave.sampleCount);

	// Add rate/stereo conversion here.
	sound[sound_id].data.buffer = (unsigned char*)malloc(Wave.dataLength);
	memcpy(sound[sound_id].data.buffer, sample_temp + 0x2c, Wave.dataLength); //Have to cut out the header data from the wave data
	free(sample_temp);

	//Add this sample to the loaded samples list
	lsamples.push_back(sound[sound_id]);
	//Return Sound ID
	wrlog("Loaded sound success");
	return(sound_id);
}

void mixer_update()
{
	int32_t smix = 0;    //Sample mix buffer
	int32_t fmix = 0;   // Final sample mix buffer
	
	
	for (int i = 0; i < BUFFER_SIZE; i++)
	{
		fmix = 0x00; //Set mix buffer to zero (silence)

		for (std::list<int>::iterator it = audio_list.begin(); it != audio_list.end(); ++it)
		{
			SAMPLE p = sound[channel[*it].loaded_sample_num]; //To shorten

			if (channel[*it].pos >= p.sampleCount)//p.dataLength)
			{
				if (channel[*it].looping == 0) 
				{ 
					channel[*it].state = SOUND_STOPPED; 
					audio_list.erase(it); 
				}
				channel[*it].pos = 0; //? Always rewind sample?
			}
			// 16 bit mono
			if (p.bitPerSample == 16)
			{
				smix = (short)p.data.u16[channel[*it].pos];
				smix = smix * channel[*it].vol;
				channel[*it].pos += 1;
			}
			// 8 bit mono
			else if (p.bitPerSample == 8)
			{
				smix = (short)(((p.data.u8[channel[*it].pos] - 128) << 8));
				smix = smix * channel[*it].vol;
				channel[*it].pos += 1;
			}

			smix = static_cast<int32_t> (smix * .25); //Reduce volume to avoid clipping. This number can vary depending on the samples.?
													  //Mix here.
			fmix = fmix + smix;
		}
		if (fmix) //If the mix value is zero (nothing playing) , skip all this.
		{
			//Clip samples
			//16 bit maximum is 32767 (0x7fff)
			//16 bit minimum is -32768 (0x8000)
			if (fmix > 32767) fmix = 32767;
			if (fmix < -32768) fmix = -32768;
		}

		soundbuffer[i] = static_cast<short>(fmix);
	}
	

	osd_update_audio_stream(soundbuffer, BUFFER_SIZE);
}

void mixer_end()
{
	dsound_stop();

	for (std::size_t i = 0; i < lsamples.size(); ++i)

	{
		if (sound[i].data.buffer)
		{
			free(sound[i].data.buffer);
			wrlog("Freeing sample #%d named %s", i, sound[i].name.c_str());
		}
	}
}

void sample_stop(int chanid)
{
	channel[chanid].state = SOUND_STOPPED;
	channel[chanid].looping = 0;
	channel[chanid].pos = 0;
	audio_list.remove(chanid);
}

void sample_start(int chanid, int samplenum, int loop)
{
	//First check that it's a valid sample! **This may not be nessary with the changes to add a vector pushback **
	if (sound[samplenum].state != SOUND_LOADED)
	{
		wrlog("error, attempting to play invalid sample on channel %d state: %d", chanid, channel[chanid].state);
		return;
	}

	if (channel[chanid].state == SOUND_PLAYING)
	{
		wrlog("error, sound already playing on this channel %d state: %d", chanid, channel[chanid].state);
		return;
	}

	channel[chanid].state = SOUND_PLAYING;
	channel[chanid].stream_type = SOUND_PCM;
	channel[chanid].loaded_sample_num = samplenum;
	channel[chanid].looping = loop;
	channel[chanid].pos = 0;
	channel[chanid].vol = 1.0;
	audio_list.emplace_back(chanid);
}

int sample_get_position(int chanid)
{
	return channel[chanid].pos;
}

void sample_set_volume(int chanid, int volume)
{
	/*
	if (volume == 0)
	{
		channel[chanid].vol = 0.526;
	}

	else
	{
		channel[chanid].vol = 1.0;
	}
	*/
	channel[chanid].vol = dbvolume[volume];

	wrlog("Setting channel %i to with volume %i setting bvolume %f", chanid, volume, channel[chanid].vol);

	//channel[chanid].vol = 100 * (1 - (log(volume) / log(0.5)));
	// vol = CLAMP(0, pow(10, (vol/2000.0))*255.0 - DSBVOLUME_MAX, 255);

	/*
	To increase the gain of a sample by X db, multiply the PCM value by pow( 2.0, X/6.014 ). i.e. gain +6dB means doubling the value of the sample, -6dB means halving it.
	inline double amp2dB(const double amp)
{
	// input must be positive +1.0 = 0dB
	if (amp < 0.0000000001) { return -200.0; }
	return (20.0 * log10(amp));
}
inline double dB2amp(const double dB)
{
  // 0dB = 1.0
  //return pow(10.0,(dB * 0.05)); // 10^(dB/20)
  return exp(dB * 0.115129254649702195134608473381376825273036956787109375);
}

0. (init) double max = 0.0; double tmp;
1. convert your integer audio to floating point (i would use double, but whatever)
2. for every sample do:
CODE: SELECT ALL

tmp = amp2dB(fabs(x));
max = (tmp > max ? tmp : max); // store the highest dB peak..
3. at the end, when you have processed the whole audio - max gives the maximum peak level in dB, so to normalize to 0dB you can do this:
0. (precalculate) double scale = dB2amp(max * -1.0);
1. multiply each sample by "scale"
2. convert back to integer if you want..

OR
float volume_control(float signal, float gain) {
	return signal * pow( 10.0f, db * 0.05f );
}

OR
Yes, gain is just multiplying by a factor. A gain of 1.0 makes no change to the volume (0 dB), 0.5 reduces it by a factor of 2 (-6 dB), 2.0 increases it by a factor of 2 (+6 dB).

To convert dB gain to a suitable factor which you can apply to your sample values:

double gain_factor = pow(10.0, gain_dB / 20.0);

OR
inline float AmplitudeTodB(float amplitude)
{
  return 20.0f * log10(amplitude);
}

inline float dBToAmplitude(float dB)
{
  return pow(10.0f, db/20.0f);
}

Decreasing Volume:

dB	Amplitude
-1	0.891
-3	0.708
-6	0.501
-12	0.251
-18	0.126
-20	0.1
-40	0.01
-60	0.001
-96	0.00002
Increasing Volume:

dB	Amplitude
1	1.122
3	1.413
6	1.995
12	3.981
18	7.943
20	10
40	100
60	1000
96	63095.734

	*/
};

int sample_get_volume(int chanid) { return 0; };
void sample_set_position(int chanid, int pos) {};
void sample_set_freq(int channid, int freq) {};

int sample_playing(int chanid)
{
	if (channel[chanid].state == SOUND_PLAYING)
		return 1;
	else return 0;
}

void sample_end(int chanid)
{
	channel[chanid].looping = 0;
}

void stream_start(int chanid, int stream, int bits)
{
	int stream_sample = create_sample(bits, 0, SYS_FREQ, BUFFER_SIZE);

	if (channel[chanid].state == SOUND_PLAYING)
	{
		wrlog("error, sound already playing on this channel %d state: %d", chanid, channel[chanid].state);
		return;
	}

	channel[chanid].state = SOUND_PLAYING;
	channel[chanid].loaded_sample_num = stream_sample;
	channel[chanid].looping = 1;
	channel[chanid].pos = 0;
	channel[chanid].stream_type = SOUND_STREAM;
	audio_list.emplace_back(chanid);
}

void stream_stop(int chanid, int stream)
{
	channel[stream].state = SOUND_STOPPED;
	channel[stream].loaded_sample_num = 0;
	channel[stream].looping = 0;
	channel[stream].pos = 0;
	audio_list.remove(chanid);
	//Warning, This doesn't delete the created sample
}

void stream_update(int chanid, int stream, short* data)
{
	if (channel[chanid].state == SOUND_PLAYING)
	{
		SAMPLE p = sound[channel[chanid].loaded_sample_num];
		memcpy(p.data.buffer, data, p.dataLength);
	}
}

void sample_remove(int samplenum)
{
	//TBD
}

// create_sample:
// *  Constructs a new sample structure of the specified type.

int create_sample(int bits, int stereo, int freq, int len)
{
	int	sound_id = -1;      // id of sound to be loaded
	int  index;               // looping variable
	// step one: are there any open id's ?
	for (index = 0; index < MAX_SOUNDS; index++)
	{
		// make sure this sound is unused
		if (sound[index].state == SOUND_NULL)
		{
			sound_id = index;
			break;
		} // end if
	} // end for index
	  // did we get a free id? If not,fail.
	if (sound_id == -1) {
		wrlog("No free sound id's for creation of new sample?"); return(-1);
	}
	//SOUND
	wrlog("Creating Stream Audio with sound id %d", sound_id);

	int datasz = (len * ((bits == 8) ? 1 : sizeof(short)) * ((stereo) ? 2 : 1));
	wrlog("Creating sample with databuffer size %d", datasz);
	// set rate and size in data structure
	sound[sound_id].sampleRate = freq;
	sound[sound_id].bitPerSample = bits;
	sound[sound_id].dataLength = datasz;
	sound[sound_id].sampleCount = len;
	sound[sound_id].state = SOUND_LOADED;
	sound[sound_id].name = "STREAM";
	sound[sound_id].data.buffer = { (unsigned char*)malloc(datasz) };//(unsigned char*)malloc(BUFFER_SIZE * 2);
	memset(sound[sound_id].data.buffer, 0, datasz);
	lsamples.push_back(sound[sound_id]);
	return sound_id;
}

//Find a loaded sample number in a list.
int snumlookup(int snum)
{
	for (auto i = lsamples.begin(); i != lsamples.end(); ++i)
	{
		if (snum == (i->num)) { return i->num; }
	}

	wrlog("Attempted lookup of sample number, it was not found in loaded samples?");
	return 0;
}

//Be careful that you call this with a real sample->num, not just the loaded sample number
std::string numToName(int num)
{
	try {
		auto it = lsamples.at(num);      // vector::at throws an out-of-range
		return it.name;
	}

	catch (const std::out_of_range& err)
	{
		wrlog("Out of Range error: get loaded sample name: %s \n", err.what());
	}
	return ("notfound");
}

int nameToNum(std::string name)
{
	for (auto i = lsamples.begin(); i != lsamples.end(); ++i)
	{
		//if (name == i->name) { return i - lsamples.begin(); }

		if ((name.compare(i->name) == 0)) { return i->num; }
	}

	wrlog("Sample: %s not found, returning 0\n", name.c_str());

	return -1;
}