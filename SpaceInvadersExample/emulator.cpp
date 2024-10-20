#include "framework.h"
#include "glew.h"
#include "wglew.h"
#include "log.h"
#include "rawinput.h"
#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <process.h>
#include <memory.h>
#include "cpu_z80.h"
#include "fast_poly.h"
#include "mixer.h"

#pragma warning( disable : 4996 4244)

//Mem Image
unsigned char* gameImage = NULL;
//Context
cpu_z80* CPU;
Fpoly* sc;

//Game Variables
static int iInvadersShiftData1, iInvadersShiftData2, iInvadersShiftAmount;
UINT8 port0 = 0x40;
UINT8 port1 = 0x81;
UINT8 port2 = 0x00;
UINT8 z80dip1 = 0x00;
static UINT8 bInvadersPlayer = 0x00;
static UINT8 bInvadersDipswitch = 0x00;

UINT8 flip;
int screenmem[224][256];
UINT32 dwResult = 0;
UINT32 dwDisplayInterval = 0;
UINT32 dwIntTotal = 0;
static int horiz = 0;
int player2 = 0;

const char* invaders_samples[] = {
	"invaders.zip",
	"0.wav",
	"1.wav",
	"2.wav",
	"3.wav",
	"4.wav",
	"5.wav",
	"6.wav",
	"7.wav",
	"8.wav",
	"9.wav",
	 "NULL"
};

struct roms
{
	const char* filename;
	UINT16 loadAddr;
	UINT16 romSize;
};

struct roms invaders[] =
{
	{"roms\\invaders\\invaders.h", 0x0000, 0x800},
	{"roms\\invaders\\invaders.g", 0x0800, 0x800 },
	{"roms\\invaders\\invaders.f", 0x1000, 0x800 },
	{"roms\\invaders\\invaders.e", 0x1800, 0x800 },
	{NULL, 0, 0}
};

int screenw;
int screenh;
int windowed;

UINT32 frames = 0;
UINT32 dwElapsedTicks = 0;
struct roms* RomLoad;

// Mame compatible generic RAM/ROM structures
//Read
UINT8 MRA_RAM(UINT32 address, struct MemoryReadByte* psMemRead)
{
	return gameImage[address + psMemRead->lowAddr];
}

UINT8 MRA_ROM(UINT32 address, struct MemoryReadByte* psMemRead)
{
	return gameImage[address + psMemRead->lowAddr];
}

//Write
void MWA_RAM(UINT32 address, UINT8 data, struct MemoryWriteByte* pMemWrite)
{
	gameImage[address + pMemWrite->lowAddr] = data;
}

void MWA_ROM(UINT32 address, UINT8 data, struct MemoryWriteByte* pMemWrite)
{
	//If logging add here
}

void Load_Roms()
{
	FILE* fp;
	int loop = 0;
	RomLoad = invaders;

	while (RomLoad[loop].filename)
	{
		fp = fopen(RomLoad[loop].filename, "rb");

		if (fp == NULL)
		{
			allegro_message("Cannot find %s !\n Is it in the roms directory?", RomLoad[loop].filename);
			exit(0);
		}
		fread(gameImage + RomLoad[loop].loadAddr, 1, RomLoad[loop].romSize, fp);
		fclose(fp);
		++loop;
	}
}

void ProgRom(UINT32 address, UINT8 data, struct MemoryWriteByte* psMemWrite)
{}

void Drawscreen()
{
	int i, w;

	for (i = 0; i != 224; i++)
	{
		for (w = 0; w != 256; w++)
		{
			if (screenmem[i][w] != 0)
			{
				sc->addPoly(i, w, 3.0f, RGB_WHITE);
			}
		}
	}
}

UINT16 InvadersPlayerRead(UINT16 port, struct z80PortRead* pPR)
{
	if (port == 0) { return(port0); }
	if (port == 1)
	{
		bInvadersPlayer = port1;
		if (key[KEY_1]) bInvadersPlayer |= 0x04;//04
		if (key[KEY_2]) bInvadersPlayer |= 0x02;//02
		if (key[KEY_5]) bInvadersPlayer &= ~0x01;//01
		if (key[KEY_LEFT]) bInvadersPlayer |= 0x20;
		if (key[KEY_RIGHT]) bInvadersPlayer |= 0x40;
		if (key[KEY_LCONTROL]) bInvadersPlayer |= 0x10;
		return(bInvadersPlayer);
	}
	if (port == 2)
	{
		bInvadersPlayer = port2;

		if (key[KEY_LEFT]) bInvadersPlayer |= 0x20;
		if (key[KEY_RIGHT]) bInvadersPlayer |= 0x40;
		if (key[KEY_LCONTROL]) bInvadersPlayer |= 0x10;
		return(bInvadersPlayer | z80dip1);
	}

	return(bInvadersPlayer);//
}

void InvadersSoundPort1Write(UINT16 port, UINT8 data, struct z80PortWrite* pPW)
{
	static UINT8 bSound = 0;
}

void InvadersSoundPort3Write(UINT16 port, UINT8 data, struct z80PortWrite* pPW)
{
	
	static unsigned char Sound = 0;

	if (data & 0x01 && ~Sound & 0x01)
		sample_start(0, 0, 1);

	if (~data & 0x01 && Sound & 0x01)
	   sample_stop(0);

	if (data & 0x02 && ~Sound & 0x02)
	   sample_start(1, 1, 0);

	if (~data & 0x02 && Sound & 0x02)
		sample_stop(1);

	if (data & 0x04 && ~Sound & 0x04)
		sample_start(2, 2, 0);

	if (~data & 0x04 && Sound & 0x04)
		sample_stop(2);

	if (data & 0x08 && ~Sound & 0x08)
		sample_start(3, 3, 0);

	if (~data & 0x08 && Sound & 0x08)
		sample_stop(3);

	//if ((data & 0x04) && screen_support_red) { screen_red = 1; }
	//if (!(data & 0x04) && screen_support_red) { screen_red = 0; }
	Sound = data;
	
}

void InvadersSoundPort5Write(UINT16 port, UINT8 data, struct z80PortWrite* pPW)
{
	static unsigned char Sound = 0;
	static int lastp2 = 0;
	
	if (data & 0x01 && ~Sound & 0x01)
		sample_start(4, 4, 0);

	if (data & 0x02 && ~Sound & 0x02)
		sample_start(5, 5, 0);

	if (data & 0x04 && ~Sound & 0x04)
		sample_start(6, 6, 0);

	if (data & 0x08 && ~Sound & 0x08)
		sample_start(7, 7, 0);

	if (data & 0x10 && ~Sound & 0x10)
		sample_start(8, 8, 0);

	if (~data & 0x10 && Sound & 0x10)
		sample_stop(5);
	
	player2 = (data & 0x20);
	if (player2) player2 = 1; else player2 = 0;
	Sound = data;
}

void InvadersVram(UINT32 addr, UINT8 data, struct MemoryWriteByte* pMemWrite)
{
	int b, x, y;
	gameImage[addr + 0x2400] = data;
	x = (addr) >> 5;
	y = (addr) & 0x1F;

	int c = 7;

	if (horiz)
		for (b = 0; b < 8; b++)
		{
			if (data & 0x01)
				screenmem[x][255 - ((y << 3) + b)] = c;
			else
				screenmem[x][255 - ((y << 3) + b)] = 0;
			data = data >> 1;
		}
	else
		for (b = 0; b < 8; b++)
		{
			if (data & 0x01)
				screenmem[x][((y << 3) + b)] = c;
			else
				screenmem[x][((y << 3) + b)] = 0;
			data = data >> 1;
		}
}

void reset_all()
{
	// Reset the virtual processor and load in defaults
	CPU->mz80reset();
	// WARNING: mz80Reset wipes out the interrupt addresses to the defaults!

	CPU->z80intAddr = 0x08;		// Interrupt address, this will change every cycle
}

static void InvadersInterrupt()
{
	if (flip)
	{
		CPU->z80intAddr = 0xcf;		// Interrupt address opcode for Rst(0x08)
		CPU->mz80int(0xcf);
	}
	else
	{
		CPU->z80intAddr = 0xd7;		//  Interrupt address opcode for Rst(0x10)
		CPU->mz80int(0xd7);
	}

	flip ^= 1;
}

///////////////////////  MAIN LOOP /////////////////////////////////////
void emu_update()
{
	UINT32 dwElapsedTicks = 0;

	mixer_update();

	// Two interrupts per frame
	dwResult = CPU->mz80exec(2000000 / 60 / 2);	// Execute roughly 40000 cycles
	dwIntTotal += CPU->mz80GetElapsedTicks(0);	// Get how much emulated time passed?
	dwDisplayInterval += CPU->mz80GetElapsedTicks(1);	// Reset our internal counter
	InvadersInterrupt();

	dwResult = CPU->mz80exec(2000000 / 60 / 2);	// Execute roughly 40000 cycles
	dwIntTotal += CPU->mz80GetElapsedTicks(0);	// Get how much emulated time passed?
	dwDisplayInterval += CPU->mz80GetElapsedTicks(1);	// Reset our internal counter
	InvadersInterrupt();

	glClear(GL_COLOR_BUFFER_BIT);
	Drawscreen();
	sc->Render();
	swap_buffers();
	
}

static UINT16 InvadersDipswitchRead(UINT16 port, struct z80PortRead* pPR)
{
	return port2;//bInvadersDipswitch;
}

static void InvadersShiftAmountWrite(UINT16 port, UINT8 data, struct z80PortWrite* pPW)
{
	iInvadersShiftAmount = data;// & 0x07;
}

static void InvadersShiftDataWrite(UINT16 port, UINT8 data, struct z80PortWrite* pPW)
{
	iInvadersShiftData2 = iInvadersShiftData1;
	iInvadersShiftData1 = data;
}

UINT16 InvadersShiftDataRead(UINT16 port, struct z80PortRead* pPR)
{
	return (((((iInvadersShiftData1 << 8) | iInvadersShiftData2) << (iInvadersShiftAmount)) >> 8) & 0xff);
}

struct MemoryReadByte sInvadersRead[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x3fff, MRA_RAM },
	{ 0x4000, 0x57ff, MRA_ROM },
	{(UINT32)-1, (UINT32)-1, NULL}
};

struct MemoryWriteByte sInvadersWrite[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x23ff, MWA_RAM },
	{ 0x2400, 0x3fff, InvadersVram },
	{ 0x4000, 0x57ff, MWA_ROM },
	{(UINT32)-1, (UINT32)-1, NULL}
};

struct z80PortRead sInvadersPortRead[] =
{
	{ 0x0000, 0x0002, InvadersPlayerRead },
	{ 0x0002, 0x0002, InvadersDipswitchRead },
	{ 0x0003, 0x0003, InvadersShiftDataRead },
	{(UINT16)-1,	 (UINT16)-1,	  NULL}
};

struct z80PortWrite sInvadersPortWrite[] =
{
	{ 0x01, 0x02, InvadersShiftAmountWrite },
	{ 0x03, 0x03, InvadersSoundPort3Write },
	{ 0x02, 0x04, InvadersShiftDataWrite },
	{ 0x05, 0x05, InvadersSoundPort5Write },
	{(UINT16)-1,	(UINT8)-1,	NULL}
};

void emu_init()
{
	wglSwapIntervalEXT(1);
	
	// Setup Graphics
	sc = new Fpoly();
	//LOAD SAMPLES
	mixer_init(22050, 60);

	load_sample(0, "samples\\invaders\\0.wav");
	load_sample(0, "samples\\invaders\\1.wav");
	load_sample(0, "samples\\invaders\\2.wav");
	load_sample(0, "samples\\invaders\\3.wav");
	load_sample(0, "samples\\invaders\\4.wav");
	load_sample(0, "samples\\invaders\\5.wav");
	load_sample(0, "samples\\invaders\\6.wav");
	load_sample(0, "samples\\invaders\\7.wav");
	load_sample(0, "samples\\invaders\\8.wav");
	load_sample(0, "samples\\invaders\\9.wav");
	

	//Init memory for Game Image
	gameImage = (unsigned char*)malloc(0x10000);
	if (gameImage == NULL)
	{
		allegro_message("Can't allocate system ram!", "Fail");
		exit(1);
	}
	
	// load romsets:
	Load_Roms();

	CPU = new cpu_z80(gameImage, sInvadersRead, sInvadersWrite, sInvadersPortRead, sInvadersPortWrite, 0xffff, 0);
	CPU->z80intAddr = 0x08;
	CPU->mz80reset();
	// F7 - C7 = 30
	wrlog("TESTING: F7 %x", ((0xf7 >> 3) & 7) << 3);
	wrlog("TESTING: CF again  %x", ((0xcf >> 3) & 7) << 3);
}

void emu_end()
{
	wrlog("Calling Exit");

	//free(gameImage);
	iInvadersShiftData1 = 0;
	iInvadersShiftData2 = 0;
	iInvadersShiftAmount = 0;
}