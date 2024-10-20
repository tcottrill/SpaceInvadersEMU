#include <stdio.h>
#include <stdlib.h>
#include "wavfile.h"
#include "string.h"
#include "log.h"
//#include "stb_vorbis.h"

/*
Note this simple wav file loader taken from
http://forums.qj.net/psp-development-forum/144147-sample-simple-wav-loader.html
*/

//Originally this only handled wav file loading, but i've hacked in non streaming ogg handling. This should be broken out into its own handler.

#pragma warning (disable : 4018 ) //Signed Unsigned mismatch

wavedef Wave;

int WavFileLoadInternal(unsigned char *wavfile, int size)
{
	int i;

	if (memcmp(wavfile, "RIFF", 4) != 0)
	{
		//LOG_ERROR("Not a wav file");
		
		if (memcmp(wavfile, "oggS", 4) == 0)
		{
			int channels;
			int sample_rate;
			short *output;
			//int rc = stb_vorbis_decode_filename("somefile.ogg",  &channels, &sample_rate, &output);
			//if (rc == -1) fprintf(stderr, "oops\n"); 
		
		}
		
		return false;
	}
	

	Wave.channels = *(short *)(wavfile + 0x16);
	Wave.sampleRate = *(unsigned short *)(wavfile + 0x18);
	Wave.blockAlign = *(short *)(wavfile + 0x20);
	Wave.bitPerSample = *(short *)(wavfile + 0x22);

	for (i = 0; memcmp(wavfile + 0x24 + i, "data", 4) != 0; i++)
	{
		if (i == 0xFF)	{ return false;	}
	}

	Wave.dataLength = *(unsigned long *)(wavfile + 0x28 + i);

	if (Wave.dataLength + 0x2c > size)	{return false;} // Invalid header size

	if (Wave.channels != 2 && Wave.channels != 1) {	return false;} // Invalid # of channels

	if (Wave.sampleRate > 100000 || Wave.sampleRate < 2000)	{ return false;	} // Invalid bitrate

	if (Wave.channels == 2) {Wave.sampleCount = Wave.dataLength / (Wave.bitPerSample >> 2);	}
	else { Wave.sampleCount = Wave.dataLength / ((Wave.bitPerSample >> 2) >> 1);}

	if (Wave.sampleCount <= 0) { return false;} //Invalid samplecount

	Wave.data = wavfile + 0x2c;
	return true;
}

 