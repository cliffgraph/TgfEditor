#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型

static const int NUM_REG_OPLL = 0x39;
static const int NUM_REG_PSG = 0x10;
static const int NUM_REG_SCC = 0x100;
struct SOUND_REGISTERS
{
	uint8_t opll[NUM_REG_OPLL];
	uint8_t psg[NUM_REG_PSG];
	uint8_t scc[NUM_REG_SCC];
	uint8_t sccplus[NUM_REG_SCC];
	void clear()
	{
		for(int t = 0; t < NUM_REG_OPLL; ++t){
			opll[t] = 0x00;
		}
		for(int t = 0; t < NUM_REG_PSG; ++t){
			psg[t] = 0x00;
		}
		for(int t = 0; t < NUM_REG_SCC; ++t){
			scc[t] = sccplus[t] = 0x00;
		}
		return;
	}
};

struct SOUND_REGISTERS_MOD
{
	bool opll[NUM_REG_OPLL];
	bool psg[NUM_REG_PSG];
	bool scc[NUM_REG_SCC];
	bool sccplus[NUM_REG_SCC];

	void clear()
	{
		for(int t = 0; t < NUM_REG_OPLL; ++t){
			opll[t] = false;
		}
		for(int t = 0; t < NUM_REG_PSG; ++t){
			psg[t] = false;
		}
		for(int t = 0; t < NUM_REG_SCC; ++t){
			scc[t] = sccplus[t] = false;
		}
		return;
	}
};

