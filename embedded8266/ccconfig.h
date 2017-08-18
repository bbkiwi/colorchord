#ifndef _CCCONFIG_H
#define _CCCONFIG_H

#include <c_types.h>

#define HPABUFFSIZE 512

#define CCEMBEDDED
#define NUM_LIN_LEDS 18
#define DFREQ 16000

#define memcpy ets_memcpy
#define memset ets_memset

#define ROOT_NOTE_OFFSET	CCS.gROOT_NOTE_OFFSET
#define DFTIIR				CCS.gDFTIIR
#define FUZZ_IIR_BITS  		CCS.gFUZZ_IIR_BITS
#define MAXNOTES  12 //MAXNOTES cannot be changed dynamically.
#define FILTER_BLUR_PASSES	CCS.gFILTER_BLUR_PASSES
#define SEMIBITSPERBIN		CCS.gSEMIBITSPERBIN
#define MAX_JUMP_DISTANCE	CCS.gMAX_JUMP_DISTANCE
#define MAX_COMBINE_DISTANCE CCS.gMAX_COMBINE_DISTANCE
#define AMP_1_IIR_BITS		CCS.gAMP_1_IIR_BITS
#define AMP_2_IIR_BITS		CCS.gAMP_2_IIR_BITS
#define MIN_AMP_FOR_NOTE	CCS.gMIN_AMP_FOR_NOTE
#define MINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR CCS.gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR
#define NOTE_FINAL_AMP		CCS.gNOTE_FINAL_AMP
#define NERF_NOTE_PORP		CCS.gNERF_NOTE_PORP
#define USE_NUM_LIN_LEDS	CCS.gUSE_NUM_LIN_LEDS
#define COLORCHORD_OUTPUT_DRIVER	CCS.gCOLORCHORD_OUTPUT_DRIVER
#define COLORCHORD_ACTIVE	CCS.gCOLORCHORD_ACTIVE
#define COLORCHORD_SHIFT_INTERVAL	CCS.gCOLORCHORD_SHIFT_INTERVAL
#define COLORCHORD_FLIP_ON_PEAK	CCS.gCOLORCHORD_FLIP_ON_PEAK
#define COLORCHORD_SHIFT_DISTANCE	CCS.gCOLORCHORD_SHIFT_DISTANCE
#define COLORCHORD_SORT_NOTES	CCS.gCOLORCHORD_SORT_NOTES
#define COLORCHORD_LIN_WRAPAROUND	CCS.gCOLORCHORD_LIN_WRAPAROUND

struct CCSettings
{
	uint8_t gSETTINGS_KEY;
	uint8_t gROOT_NOTE_OFFSET; //Set to define what the root note is.  0 = A.
	uint8_t gDFTIIR;                            //=6
	uint8_t gFUZZ_IIR_BITS;                     //=1
	uint8_t gFILTER_BLUR_PASSES;                //=2
	uint8_t gSEMIBITSPERBIN;                    //=3
	uint8_t gMAX_JUMP_DISTANCE;                 //=4
	uint8_t gMAX_COMBINE_DISTANCE;              //=7
	uint8_t gAMP_1_IIR_BITS;                    //=4
	uint8_t gAMP_2_IIR_BITS;                    //=2
	uint8_t gMIN_AMP_FOR_NOTE;                  //=80
	uint8_t gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR; //=64
	uint8_t gNOTE_FINAL_AMP;                    //=12
	uint8_t gNERF_NOTE_PORP;                    //=15
	uint8_t gUSE_NUM_LIN_LEDS;                  // = NUM_LIN_LEDS
	uint8_t gCOLORCHORD_ACTIVE;
	uint8_t gCOLORCHORD_OUTPUT_DRIVER;
	uint8_t gCOLORCHORD_SHIFT_INTERVAL; // ==0 controls speed of shifting if 0 no shift
	uint8_t gCOLORCHORD_FLIP_ON_PEAK; //==0 if non-zero gives flipping at peaks of shift direction, 0 no flip
	int8_t gCOLORCHORD_SHIFT_DISTANCE; //==0 distance of shift
	uint8_t gCOLORCHORD_SORT_NOTES; //==0  0 no sort, 1 inc freq, 2 dec amps, 3 dec amps2
	uint8_t gCOLORCHORD_LIN_WRAPAROUND; //==0  0 no adjusting, else current led display has minimum deviation to prev
};

extern struct CCSettings CCS;




#endif

