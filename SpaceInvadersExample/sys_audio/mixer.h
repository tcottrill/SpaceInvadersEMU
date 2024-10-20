#pragma once

#include <string>

typedef struct
{
	int state;                      // state of the sound ?? Playing/Stopped
	unsigned int pos;
	int loaded_sample_num;
	int id;
	int looping;
	double vol;
	int stream_type;


} CHANNEL;


typedef struct
{
	short int channels;		     //<  Number of channels
	unsigned short sampleRate;	/**<  Sample rate */
	unsigned long sampleCount;	/**<  Sample count */
	unsigned long dataLength;	/**<  Data length */
	short int bitPerSample;	/**<  The bit rate of the WAV */
	int state;                  //Sound loaded or sound empty
	int num;
	std::string name;
	union {
		unsigned char *u8;      /* data for 8 bit samples */
		unsigned short *u16;    /* data for 16 bit samples */
		void *buffer;           /* generic data pointer to the actual wave data*/
	} data;


}SAMPLE;


int mixer_init(int rate, int fps);
void mixer_update();
void mixer_end();
int load_sample(const char *archname, const char *filename);
void sample_stop(int chanid);
void sample_start(int chanid, int samplenum, int loop);
void sample_set_position(int chanid, int pos);
void sample_set_volume(int chan, int volume);
void sample_set_freq(int channid, int freq);
int sample_playing(int chanid);
void sample_end(int chanid);
void sample_remove(int samplenum);
//Streaming audio functions tacked on.
void stream_start(int chanid, int stream, int bits);
void stream_stop(int chanid, int stream);
void stream_update(int chanid, int stream, short *data);
int create_sample(int bits, int stereo, int freq, int len);
//Internal functions
int sample_up16(int num);
int snumlookup(int snum);
int create_sample(const std::string& name, int bits, int channels, int freq, int len);
int nameToNum(std::string name);
std::string numToName(int num);







