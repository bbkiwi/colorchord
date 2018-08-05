#include "commonservices.h"
#include <gpio.h>
#include <ccconfig.h>
#include <eagle_soc.h>
#include "esp82xxutil.h"
#include <DFT32.h>
#include <embeddednf.h>
#include <embeddedout.h>

extern volatile uint8_t sounddata[HPABUFFSIZE];
extern volatile uint16_t soundhead;
extern uint8_t sounddatacopy[HPABUFFSIZE];
extern uint8_t glitch_count_max;
extern uint8_t glitch_drop;

#define CONFIGURABLES 33 //(actual number plus 1)
#define NUMBER_STORED_CONFIGURABLES 16

struct SaveLoad
{
	uint8_t configs[NUMBER_STORED_CONFIGURABLES][CONFIGURABLES];
	uint16_t saved_max_bins[FIXBINS];
	uint8_t current_config;
	uint8_t SaveLoadKey; //Must be 0xaa to be valid.
} settings __attribute__ ((aligned (16)));

struct CCSettings CCS;

uint8_t gConfigDefaults[NUMBER_STORED_CONFIGURABLES][CONFIGURABLES] = {
//               i  o d f e   f l  s j  c    a d m    a d m    m  m b   s     p  u                       s  a d    s  f    d      s w   c  n
//               a  f i i q   b c  b m  o    1 1 1    2 2 2    a  a r   a     r  s                       y  c r    h  l    i      r r   f  l
//                  f r r u   p o  b p  m                      n  d i   t     o  e                       m  t v    f  p    s      t p   #  d
		{64,0,6,3,0,  5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255,  25,DEFAULT_NUM_LEDS,       0, 1,0,   0, 0,   0,     1,1,  0, DEFAULT_NUM_LEDS,   0},
		{32,0,6,3,0,  5,14,3,80,76,  0,0,16,  0,0,16,  45,1,100,255,  55,DEFAULT_NUM_LEDS-2,     0, 1,8,   4, 255, 1,     0,0,  1, DEFAULT_NUM_LEDS,   0},
		{32,0,5,3,0,  5,14,3,85,85,  4,4,16,  7,7,16,  82,1,255,255,  15,(DEFAULT_NUM_LEDS-1)/3, 2, 1,0,   1, 0,   2,     0,0,  2, DEFAULT_NUM_LEDS,   0},
		{16,0,6,6,0,  5,21,3,42,44,  2,2,16,  4,4,16,  45,1,100,255,  10,DEFAULT_NUM_LEDS,       0, 1,254, 20,0,   0,     0,0,  3, DEFAULT_NUM_LEDS,   0},
		{16,0,5,3,0,  5,14,3,85,85,  4,4,16,  6,6,16,  82,1,75, 255,  55,(DEFAULT_NUM_LEDS-1)/3, 2, 1,8,   1, 0,   1,     0,0,  4, DEFAULT_NUM_LEDS,   0},
		{16,0,6,3,0,  5,14,3,42,44,  2,2,16,  4,4,16,  45,1,200,255,  0, DEFAULT_NUM_LEDS,       0, 1,0,   5, 1,   1,     1,0,  5, DEFAULT_NUM_LEDS,   0},
		{32,0,6,3,0,  5,14,3,80,76,  0,0,16,  9,9,16,  45,1,100,255,  55,DEFAULT_NUM_LEDS-2,     0, 1,8,   1, 0,   1,     0,0,  6, DEFAULT_NUM_LEDS,   0},
		{16,0,5,4,0,  0,37,3,42,44,  2,2,16,  4,4,16,  45,1,200,255,  15,DEFAULT_NUM_LEDS,       0, 1,255, 0, 0,   0,     0,0,  7, DEFAULT_NUM_LEDS,   0},
		{32,0,6,3,0,  5,51,3,255,0,  3,3,16,  8,8,16,  45,1,100,255,  55,DEFAULT_NUM_LEDS-2,     0, 1,8,   0, 1,   1,     0,0,  8, DEFAULT_NUM_LEDS,   0},
		{16,0,6,3,0,  5,14,3,42,44,  2,2,16,  4,4,16,  45,1,200,255,  15,DEFAULT_NUM_LEDS/2,     1, 1,0,   0, 0,   0,     0,0,  9, DEFAULT_NUM_LEDS,   0},
		{16,0,6,3,0,  5,14,3,42,44,  2,2,16,  4,4,16,  45,1,200,255,  15,DEFAULT_NUM_LEDS/3,     2, 1,0,   0, 0,   0,     0,0,  10,DEFAULT_NUM_LEDS,   0},
		{16,0,6,3,0,  5,14,3,42,44,  2,2,16,  4,4,16,  45,1,200,255,  15,DEFAULT_NUM_LEDS/4,     3, 1,0,   0, 0,   0,     0,0,  11,DEFAULT_NUM_LEDS,   0},
		{64,0,6,3,0,  5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255,  25,DEFAULT_NUM_LEDS,       0, 1,1,   0, 0,   0,     1,1,  12,DEFAULT_NUM_LEDS,   0},
		{64,0,6,3,0,  5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255,  25,DEFAULT_NUM_LEDS,       0, 1,8,   0, 0,   0,     1,1,  13,DEFAULT_NUM_LEDS,   0},
		{64,0,6,3,0,  5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 101,DEFAULT_NUM_LEDS/8,     0, 1,0,   1, 31,  1,     1,0,  14,DEFAULT_NUM_LEDS,   0},
		{64,0,6,3,0,  5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 102,DEFAULT_NUM_LEDS,       0, 1,0,   0, 0,   0,     1,1,  15,DEFAULT_NUM_LEDS,   0}
	};

uint8_t * gConfigurables[CONFIGURABLES]={ &CCS.gINITIAL_AMP, &CCS.gROOT_NOTE_OFFSET, &CCS.gDFTIIR, &CCS.gFUZZ_IIR_BITS,
	&CCS.gEQUALIZER_SET,  &CCS.gFILTER_BLUR_PASSES, &CCS.gLOWER_CUTOFF, &CCS.gSEMIBITSPERBIN, &CCS.gMAX_JUMP_DISTANCE, &CCS.gMAX_COMBINE_DISTANCE,
	&CCS.gAMP1_ATTACK_BITS, &CCS.gAMP1_DECAY_BITS, &CCS.gAMP_1_MULT, &CCS.gAMP2_ATTACK_BITS, &CCS.gAMP2_DECAY_BITS, &CCS.gAMP_2_MULT, &CCS.gMIN_AMP_FOR_NOTE,
	&CCS.gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR, &CCS.gNOTE_FINAL_AMP, &CCS.gNOTE_FINAL_SATURATION,
	&CCS.gNERF_NOTE_PORP, &CCS.gUSE_NUM_LIN_LEDS, &CCS.gSYMMETRY_REPEAT, &CCS.gCOLORCHORD_ACTIVE, &CCS.gCOLORCHORD_OUTPUT_DRIVER, &CCS.gCOLORCHORD_SHIFT_INTERVAL,
	&CCS.gCOLORCHORD_FLIP_ON_PEAK, &CCS.gCOLORCHORD_SHIFT_DISTANCE, &CCS.gCOLORCHORD_SORT_NOTES, &CCS.gCOLORCHORD_LIN_WRAPAROUND, &CCS.gCONFIG_NUMBER,
	&CCS.gNUM_LIN_LEDS, 0 };

char * gConfigurableNames[CONFIGURABLES] = { "gINITIAL_AMP", "gROOT_NOTE_OFFSET", "gDFTIIR", "gFUZZ_IIR_BITS", "gEQUALIZER_SET",
	"gFILTER_BLUR_PASSES", "gLOWER_CUTOFF", "gSEMIBITSPERBIN", "gMAX_JUMP_DISTANCE", "gMAX_COMBINE_DISTANCE", "gAMP1_ATTACK_BITS", "gAMP1_DECAY_BITS",
	"gAMP_1_MULT", "gAMP2_ATTACK_BITS", "gAMP2_DECAY_BITS", "gAMP_2_MULT", "gMIN_AMP_FOR_NOTE", "gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR", "gNOTE_FINAL_AMP",
	"gNOTE_FINAL_SATURATION", "gNERF_NOTE_PORP", "gUSE_NUM_LIN_LEDS", "gSYMMETRY_REPEAT", "gCOLORCHORD_ACTIVE", "gCOLORCHORD_OUTPUT_DRIVER",
	"gCOLORCHORD_SHIFT_INTERVAL","gCOLORCHORD_FLIP_ON_PEAK", "gCOLORCHORD_SHIFT_DISTANCE", "gCOLORCHORD_SORT_NOTES", "gCOLORCHORD_LIN_WRAPAROUND",
	"gCONFIG_NUMBER", "gNUM_LIN_LEDS", 0 };

void ICACHE_FLASH_ATTR SaveSettingsToFlash( )
{
	settings.SaveLoadKey = 0xAA;
	EnterCritical();
	ets_intr_lock();
	spi_flash_erase_sector( 0x3D000/4096 );
	spi_flash_write( 0x3D000, (uint32*)&settings, ((sizeof( settings )-1)&(~0xf))+0x10 );
	ets_intr_unlock();
	ExitCritical();
}


void ICACHE_FLASH_ATTR CustomStart( )
{
	int i,j;
	spi_flash_read( 0x3D000, (uint32*)&settings, sizeof( settings ) );
	if( settings.SaveLoadKey != 0xaa ) // set settings to defaults and save to flash
	{
		settings.current_config = 0;
		for( j = 0; j < NUMBER_STORED_CONFIGURABLES; j++)
		for( i = 0; i < CONFIGURABLES; i++ )
		{
			settings.configs[j][i] = gConfigDefaults[j][i];
		}
		for( i = 0; i < FIXBINS; i++ )
		{
			settings.saved_max_bins[i] = 1;
		}
		SaveSettingsToFlash();
	}
	// load settings
	for( i = 0; i < CONFIGURABLES-1; i++ )
	{
		if( gConfigurables[i] )
		{
			*gConfigurables[i] = settings.configs[settings.current_config][i];
		}
	}
	for( i = 0; i < FIXBINS; i++ )
	{
		max_bins[i] = settings.saved_max_bins[i];
	}
}

int ICACHE_FLASH_ATTR CustomCommand(char * buffer, int retsize, char *pusrdata, unsigned short len)
{
	char * buffend = buffer;

	switch( pusrdata[1] )
	{


	case 'b': case 'B': //bins
	{
		int i;
		int whichSel = ParamCaptureAndAdvanceInt( );

		uint16_t * which = 0;
		uint16_t qty = FIXBINS;
		switch( whichSel )
		{
		case 0:
			which = embeddedbins32; break;
		case 1:
			which = fuzzed_bins; break;
		case 2:
			qty = FIXBPERO;
			which = folded_bins; break;
		case 3:
			qty = OCTAVES;
			which = octave_bins; break;
		case 4:
			which = max_bins; break;
		default:
			buffend += ets_sprintf( buffend, "!CB" );
			return buffend-buffer;
		}

		buffend += ets_sprintf( buffend, "CB%d\t%d\t", whichSel, qty );
		for( i = 0; i < FIXBINS; i++ )
		{
			uint16_t samp = which[i];
			*(buffend++) = tohex1( samp>>12 );
			*(buffend++) = tohex1( samp>>8 );
			*(buffend++) = tohex1( samp>>4 );
			*(buffend++) = tohex1( samp>>0 );
		}
		return buffend-buffer;
	}


	case 'l': case 'L': //LEDs
	{
		int i, it = 0;
		buffend += ets_sprintf( buffend, "CL\t%d\t", NUM_LIN_LEDS );
		uint16_t toledsvals = NUM_LIN_LEDS*3;
		if( toledsvals > 600 ) toledsvals = 600;
		for( i = 0; i < toledsvals; i++ )
		{
			uint8_t samp = ledOut[it++];
			*(buffend++) = tohex1( samp>>4 );
			*(buffend++) = tohex1( samp&0x0f );
		}
		return buffend-buffer;
	}


	case 'm': case 'M': //Oscilloscope
	{
		buffend += ets_sprintf( buffend, "CM\t512\t" );
//		buffend += ets_sprintf( buffend, "CM\t128\t" );
#if PROTECT_OSOUNDDATA
		EnterCritical();
#endif
//		int i, it = soundhead;
		int i, it = 0;
		for( i = 0; i < 512; i++ )
//		for( i = 0; i < 128; i++ )
		{
//TODO  if replace below with uint8_t samp = 127; to test why oscope causes interferance
//      get wdt resets coninually. Why?? should have used 0x7f instead of 127
			uint8_t samp = sounddatacopy[it++];
			it = it & (HPABUFFSIZE-1);
			*(buffend++) = tohex1( samp>>4 );
			*(buffend++) = tohex1( samp&0x0f );
		}
#if PROTECT_OSOUNDDATA
		ExitCritical();
#endif
		return buffend-buffer;
	}

	case 'n': case 'N': //Notes
	{
		int i;
		buffend += ets_sprintf( buffend, "CN\t%d\t", MAXNOTES );
		for( i = 0; i < MAXNOTES; i++ )
		{
			uint16_t dat;
			dat = note_peak_freqs[i];
			*(buffend++) = tohex1( dat>>12 );
			*(buffend++) = tohex1( dat>>8 );
			*(buffend++) = tohex1( dat>>4 );
			*(buffend++) = tohex1( dat>>0 );
			dat = note_peak_amps[i];
			*(buffend++) = tohex1( dat>>12 );
			*(buffend++) = tohex1( dat>>8 );
			*(buffend++) = tohex1( dat>>4 );
			*(buffend++) = tohex1( dat>>0 );
			dat = note_peak_amps2[i];
			*(buffend++) = tohex1( dat>>12 );
			*(buffend++) = tohex1( dat>>8 );
			*(buffend++) = tohex1( dat>>4 );
			*(buffend++) = tohex1( dat>>0 );
			dat = note_jumped_to[i];
			*(buffend++) = tohex1( dat>>4 );
			*(buffend++) = tohex1( dat>>0 );
		}
		return buffend-buffer;
	}


	case 's': case 'S':
	{
		switch (pusrdata[2] )
		{

		case 'd': case 'D': //Restore default configuration or max_bins (if CONFIG_NUMBER not less than NUMBER_STORED_CONFIGURABLES)
		{
			int i;
			if (CONFIG_NUMBER < NUMBER_STORED_CONFIGURABLES) {
				for( i = 0; i < CONFIGURABLES-1; i++ ) {
					if( gConfigurables[i] ) *gConfigurables[i] = gConfigDefaults[CONFIG_NUMBER][i];
				}
			} else {
				for( i = 0; i < FIXBINS; i++ ) max_bins[i] = 1;
				maxallbins=1;
			}
			buffend += ets_sprintf( buffend, "CD" );
			return buffend-buffer;
		}

		case 'r': case 'R': //Revert to CONFIG_NUMBER stored configuration
		{
			int i;
			if (CONFIG_NUMBER < NUMBER_STORED_CONFIGURABLES) {
				for( i = 0; i < CONFIGURABLES-1; i++ ) {
					if( gConfigurables[i] ) *gConfigurables[i] = settings.configs[CONFIG_NUMBER][i];
				}
			} else { //max_bins
				maxallbins=1;
				for( i = 0; i < FIXBINS; i++ )
				{
					max_bins[i] = settings.saved_max_bins[i];
					if (max_bins[i]>maxallbins) maxallbins = max_bins[i];
				}
			}
			buffend += ets_sprintf( buffend, "CSR" );
			return buffend-buffer;
		}

		case 's': case 'S': //Save
		{
			int i;

			if (CONFIG_NUMBER < NUMBER_STORED_CONFIGURABLES)
			{
				settings.current_config = CONFIG_NUMBER;
				for( i = 0; i < CONFIGURABLES-1; i++ )
				{
					if( gConfigurables[i] )
						settings.configs[CONFIG_NUMBER][i] = *gConfigurables[i];
				}
			} else {
				for( i = 0; i < FIXBINS; i++ )
				{
					settings.saved_max_bins[i] = max_bins[i];
				}
			}

			SaveSettingsToFlash();


			buffend += ets_sprintf( buffend, "CS" );
			return buffend-buffer;
		}
		}
		buffend += ets_sprintf( buffend, "!CS" );
		return buffend-buffer;
	}



	case 'v': case 'V': //ColorChord Values
	{
		if( pusrdata[2] == 'R' || pusrdata[2] == 'r' )
		{
			int i;

			buffend += ets_sprintf( buffend, "CVR\t" );

			i = 0;
			while( gConfigurableNames[i] )
			{
				buffend += ets_sprintf( buffend, "%s=%d\t", gConfigurableNames[i], *gConfigurables[i] );
				i++;
			}

			buffend += ets_sprintf( buffend, "rBASE_FREQ=%d\trDFREQ=%d\trOCTAVES=%d\trFIXBPERO=%d\trNOTERANGE=%d\t", 
				(int)BASE_FREQ, (int)DFREQ, (int)OCTAVES, (int)FIXBPERO, (int)(NOTERANGE) );
			buffend += ets_sprintf( buffend, "rMAXNOTES=%d\t",(int)MAXNOTES);
			return buffend-buffer;
		}
		else if( pusrdata[2] == 'W' || pusrdata[2] == 'w' )
		{
			parameters+=2;
			char * name = ParamCaptureAndAdvance();
			int val = ParamCaptureAndAdvanceInt();
			int i = 0;

			while( gConfigurableNames[i] )
			{
				if( strcmp( name, gConfigurableNames[i] ) == 0 )
				{
					*gConfigurables[i] = val;
					buffend += ets_sprintf( buffend, "CVW" );
					return buffend-buffer;
				}
				i++;
			}

			buffend += ets_sprintf( buffend, "!CV" );
			return buffend-buffer;
		}
		else if( pusrdata[2] == 'G' || pusrdata[2] == 'g' )
		{
			parameters+=2;
			int val = ParamCaptureAndAdvanceInt();
			buffend += ets_sprintf( buffend, "CVG glitch_count_max changed from %d to %d", glitch_count_max, val );
			glitch_count_max = val;
			return buffend-buffer;
		}
		else if( pusrdata[2] == 'H' || pusrdata[2] == 'h' )
		{
			parameters+=2;
			int val = ParamCaptureAndAdvanceInt();
			buffend += ets_sprintf( buffend, "CVH glitch_drop changed from %d to %d", glitch_drop, val );
			glitch_drop = val;
			return buffend-buffer;
		}
		else if( pusrdata[2] == 'C' || pusrdata[2] == 'c' ) //dump stored configs
		{
			int i, j;
			buffend += ets_sprintf( buffend, "CVC\n" );
			for (i=0; i< NUMBER_STORED_CONFIGURABLES;i++) {
				for (j=0;j<CONFIGURABLES-1;j++) {
				buffend += ets_sprintf( buffend, "%02x", settings.configs[i][j] );
				}
			buffend += ets_sprintf( buffend, "\n");
			}
			return buffend-buffer;
		}
		else
		{
			buffend += ets_sprintf( buffend, "!CV" );
			return buffend-buffer;
		}
		
	}


	}
	return -1;
}

