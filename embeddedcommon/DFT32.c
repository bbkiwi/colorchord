//Copyright 2015 <>< Charles Lohr under the ColorChord License.

#include "DFT32.h"
#include <string.h>

#ifndef CCEMBEDDED
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
static float * goutbins;
#endif

uint16_t embeddedbins32[FIXBINS]; 

//NOTES to self:
//
// Let's say we want to try this on an AVR.
//  24 bins, 5 octaves = 120 bins.
// 20 MHz clock / 4.8k sps = 4096 IPS = 34 clocks per bin = :(
//  We can do two at the same time, this frees us up some 

static uint8_t Sdonefirstrun;

//A table of precomputed sin() values.  Ranging -1500 to +1500
//If we increase this, it may cause overflows elsewhere in code.
//Ssinonlytable[i] = (int16_t)((sinf( i / 256.0 * 6.283 ) * 1500.0));
const int16_t Ssinonlytable[256] = {
             0,    36,    73,   110,   147,   183,   220,   256,
           292,   328,   364,   400,   435,   470,   505,   539,
           574,   607,   641,   674,   707,   739,   771,   802,
           833,   863,   893,   922,   951,   979,  1007,  1034,
          1060,  1086,  1111,  1135,  1159,  1182,  1204,  1226,
          1247,  1267,  1286,  1305,  1322,  1339,  1355,  1371,
          1385,  1399,  1412,  1424,  1435,  1445,  1455,  1463,
          1471,  1477,  1483,  1488,  1492,  1495,  1498,  1499,
          1500,  1499,  1498,  1495,  1492,  1488,  1483,  1477,
          1471,  1463,  1455,  1445,  1435,  1424,  1412,  1399,
          1385,  1371,  1356,  1339,  1322,  1305,  1286,  1267,
          1247,  1226,  1204,  1182,  1159,  1135,  1111,  1086,
          1060,  1034,  1007,   979,   951,   922,   893,   863,
           833,   802,   771,   739,   707,   674,   641,   607,
           574,   539,   505,   470,   435,   400,   364,   328,
           292,   256,   220,   183,   147,   110,    73,    36,
             0,   -36,   -73,  -110,  -146,  -183,  -219,  -256,
          -292,  -328,  -364,  -399,  -435,  -470,  -505,  -539,
          -573,  -607,  -641,  -674,  -706,  -739,  -771,  -802,
          -833,  -863,  -893,  -922,  -951,  -979, -1007, -1034,
         -1060, -1086, -1111, -1135, -1159, -1182, -1204, -1226,
         -1247, -1267, -1286, -1305, -1322, -1339, -1355, -1371,
         -1385, -1399, -1412, -1424, -1435, -1445, -1454, -1463,
         -1471, -1477, -1483, -1488, -1492, -1495, -1498, -1499,
         -1500, -1499, -1498, -1495, -1492, -1488, -1483, -1477,
         -1471, -1463, -1455, -1445, -1435, -1424, -1412, -1399,
         -1385, -1371, -1356, -1339, -1322, -1305, -1286, -1267,
         -1247, -1226, -1204, -1182, -1159, -1135, -1111, -1086,
         -1060, -1034, -1007,  -979,  -951,  -923,  -893,  -863,
          -833,  -802,  -771,  -739,  -707,  -674,  -641,  -608,
          -574,  -540,  -505,  -470,  -435,  -400,  -364,  -328,
          -292,  -256,  -220,  -183,  -147,  -110,   -73,   -37,};


/** The above table was created using the following code:
#include <math.h>
#include <stdio.h>
#include <stdint.h>

int16_t Ssintable[256]; //Actually, just [sin].

int main()
{
	int i;
	for( i = 0; i < 256; i++ )
	{
		Ssintable[i] = (int16_t)((sinf( i / 256.0 * 6.283 ) * 1500.0));
	}

	printf( "const int16_t Ssinonlytable[256] = {" );
	for( i = 0; i < 256; i++ )
	{
		if( !(i & 0x7 ) )
		{
			printf( "\n\t" );
		}
		printf( "%6d," ,Ssintable[i] );
	}
	printf( "};\n" );
} */


/*
This routine correlates an input stream of sound samples (reals) to a number of sinusoidals (complex) at geometrically spaced frequencies.
The ratio of successives frequencies is fixed starting with a base frequency upto a maximum a number of octaves higher.
The magnitues of the correlation is stored in bins associated with each frequency.

For example (using common defaults), frequencies start at 55 Hz and are successively multiplied
by 2^(1/24). Musically this starts at A 3 octaves below middle C and step by 1/4 tones (ie 1/2 a semitone).
This produces 120 bins covering 5 octaves with 24 bins per octave. The sample rate is 16000 Hz.

Most of the calculation is done by the HandleInt() routine.
The method handles the bins for each octave differently.
It does 3 things.

   1. If applies simple filters to the input sequence.
   2. It keeps track and updates a processing schedule.
   3. According to the schedule, it updates the correlation computation for a scheduled octave
      or saves the current correlation computation and applies a IIR filter to all correlations.

The bins for the highest octave are computed using 16 samples.
The bins for next lower octave uses 8 filtered samples at rate 8000 Hz.
The bins for the next uses 4 filtered samples at rate 4000 Hz.
Then 2 filtered samples at rate 2000 Hz, and for lowest octave 1 filtered sample at 1000 Hz.

Note no matter how many octaves we have, we only need to update FIXBPERO*2 DFT bins.
*/

// The sinusoidals can be viewed as rotating vectors (viewed as complex on unit circle initial all equal 1).
// For each of the FIXBINS bins we need to store how much the vector rotates (advances) for each sample (ie frequency)
//                          and the current position (places) of the vector.
//                          Note  full revolution is 256. 16 bits interpeted as 8bits integer part 8bit fractional
uint16_t Sdatspace32A[FIXBINS*2];  //(advances,places)

// The correlation is obtained by adding the real signal and the complex numbers associated with the sinusodal.
// Pairs for each bin here (real part, imag part)
int32_t Sdatspace32B[FIXBINS*2];  //(isses,icses)

// Every time the schedule indicates the above is saved here.
// HandleInt() does this 1 out of (1<<OCTAVES) times which is (1<<(OCTAVES-1)) samples
int32_t Sdatspace32BOut[FIXBINS*2];  //(isses,icses)

//Sdo_this_octave is a scheduling state
static uint8_t Sdo_this_octave[BINCYCLE];
// Index for above
static uint8_t Swhichoctaveplace;

// Used for filtering each octave
static int32_t Saccum_octavebins[OCTAVES];

// TODO Needed? Does not seem to be used
uint16_t embeddedbins[FIXBINS]; 

//From: http://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
//  for sqrt approx but also suggestion for quick norm approximation that would work in this DFT

#if APPROXNORM != 1
/**
 * \brief    Fast Square root algorithm, with rounding
 *
 * This does arithmetic rounding of the result. That is, if the real answer
 * would have a fractional part of 0.5 or greater, the result is rounded up to
 * the next integer.
 *      - SquareRootRounded(2) --> 1
 *      - SquareRootRounded(3) --> 2
 *      - SquareRootRounded(4) --> 2
 *      - SquareRootRounded(6) --> 2
 *      - SquareRootRounded(7) --> 3
 *      - SquareRootRounded(8) --> 3
 *      - SquareRootRounded(9) --> 3
 *
 * \param[in] a_nInput - unsigned integer for which to find the square root
 *
 * \return Integer square root of the input value.
 */
static uint16_t SquareRootRounded(uint32_t a_nInput)
{
    uint32_t op  = a_nInput;
    uint32_t res = 0;
    uint32_t one = 1uL << 30; // The second-to-top bit is set: use 1u << 14 for uint16_t type; use 1uL<<30 for uint32_t type


    // "one" starts at the highest power of four <= than the argument.
    while (one > op)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (op >= res + one)
        {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }

    /* Do arithmetic rounding to nearest integer */
    if (op > res)
    {
        res++;
    }

    return res;
}
#endif

// Computes Magnitude of Correlation. This is the DFT histogram
//    which gets fuzzed, smoothed, folded etc.
void UpdateOutputBins32()
{
	int i;
	int32_t * ipt = &Sdatspace32BOut[0];
	for( i = 0; i < FIXBINS; i++ )
	{
		int32_t isps = *(ipt++); //keep 32 bits
		int32_t ispc = *(ipt++);
		// take absolute values
		isps = isps<0? -isps : isps;
		ispc = ispc<0? -ispc : ispc;
		int octave = i / FIXBPERO;

		//TODO remove or move down
		//If we are running DFT32 on regular ColorChord, then we will need to
		//also update goutbins[]... But if we're on embedded systems, we only
		//update embeddedbins32.
#ifndef CCEMBEDDED
		// convert 32 bit precision isps and ispc to floating point
		float mux = ( (float)isps * (float)isps) + ((float)ispc * (float)ispc);
		goutbins[i] = sqrtf(mux)/65536.0; // scale by 2^16
		//reasonable (but arbitrary attenuation)
		goutbins[i] /= (78<<DFTIIR)*(1<<octave);
		//TODO note this is output by DoDFTProgressive32
		//TODO A: may be able to use if change HandleInt
		//goutbins[i] /= (78<<DFTIIR);
#else
//TODO check only needed
#if APPROXNORM == 1
		// using full 32 bit precision for isps and ispc
		uint32_t rmux = isps>ispc? isps + (ispc>>1) : ispc + (isps>>1);
		rmux = rmux>>16; // keep most significant 16 bits
#else
		// use the most significant 16 bits of isps and ispc when squaring
		// since isps and ispc are non-negative right bit shifing is well defined
		uint32_t rmux = ( (isps>>16) * (isps>>16)) + ((ispc>16) * (ispc>>16));
		rmux = SquareRootRounded( rmux );
#endif

		//bump up all outputs here, so when we nerf it by bit shifting by
		//octave we don't lose a lot of detail.
		rmux = rmux << 1;

		embeddedbins32[i] = rmux >> octave;
		// TODO A: may be able to use if change HandleInt
		// embeddedbins32[i] = rmux;
#endif
	}
}

// Follows processing schedule to maintain correlation calculations,
// See discussion at beginning of code
// NOTE this is called twice for each sample
static void HandleInt( int16_t sample )
{
	int i;
	uint16_t adv;
	uint8_t localipl;
	int16_t filteredsample;

	// Get number of octave to work on, or 255 to signal special
	uint8_t oct = Sdo_this_octave[Swhichoctaveplace];
	// Update schedule index
	Swhichoctaveplace ++;
	Swhichoctaveplace &= BINCYCLE-1;

    // Builds up filtered input
	// After oct is processed below Saccum_octavebins[oct] is set to zero.
	for( i = 0; i < OCTAVES;i++ )
	{
		Saccum_octavebins[i] += sample;
	}

	if( oct > 128 )
	{
		//Special: This is when we can update everything.
		//This gets run once out of every (1<<OCTAVES) times.
		// which is half as many samples
		// It saves the current correlations, and applies IIR to running correlation.
		//     Note if DFTIIR is 0, the running correlation will be set to zero.
		// It should happen at the first call the two that
		// HandleInit makes with a given sample

		// Set pointers to start of these locations
		int32_t * bins = &Sdatspace32B[0];
		int32_t * binsOut = &Sdatspace32BOut[0];

		for( i = 0; i < FIXBINS; i++ )
		{
			// First for the SIN then the COS.
			int32_t val = *(bins);
			*(binsOut++) = val;   // copy to correct location in Sdatspace32BOut
			*(bins++) -= val>>DFTIIR; // apply IIR to correct location in Sdatspace32B

			val = *(bins);
			*(binsOut++) = val;   // copy to correct location in Sdatspace32BOut
			*(bins++) -= val>>DFTIIR; // apply IIR to correct location in Sdatspace32B
		}
		return;
	}

	// process a filtered sample for one of the octaves
	// Set up pointer to where oct storage starts
	uint16_t * dsA = &Sdatspace32A[oct*FIXBPERO*2];
	int32_t * dsB = &Sdatspace32B[oct*FIXBPERO*2];

    // Scale back to produce filter.
	filteredsample = Saccum_octavebins[oct]>>(OCTAVES-oct);
	// TODO A: may be able to use if change HandleInt
	// filteredsample = Saccum_octavebins[oct];
	Saccum_octavebins[oct] = 0;

	for( i = 0; i < FIXBPERO; i++ )
	{
		adv = *(dsA++);
		localipl = *(dsA) >> 8;
		*(dsA++) += adv;

		*(dsB++) += (Ssinonlytable[localipl] * filteredsample);
		//Get the cosine (1/4 wavelength out-of-phase with sin)
		localipl += 64;
		*(dsB++) += (Ssinonlytable[localipl] * filteredsample);
	}
}

// Sets of schedule
// Called from DoDFTProgressive32 when used by colorchord2
// Called from InitColorChord for embedded colorchord
int SetupDFTProgressive32()
{
	int i;
	int nTerminalZeros;

	Sdonefirstrun = 1;
	// First state is special
	Sdo_this_octave[0] = 0xff;
	for( i = 1; i < BINCYCLE; i++ )
	{
		// Sdo_this_octave =
		// 255 4 3 4 2 4 3 4 1 4 3 4 2 4 3 4 0 4 3 4 2 4 3 4 1 4 3 4 2 4 3 4 is case for 5 octaves.
		// Initial state is special one, then at step i do octave = Sdo_this_octave with filtered samples

		// for i > 0, state is OCTAVES -1 - the number of terminal zeros that the binary representaion of i.
		// search for "first" zero which will be the number of terminal zeros
		for( nTerminalZeros = 0; nTerminalZeros <= OCTAVES; nTerminalZeros++ )
		{
			if( ((1<<nTerminalZeros) & (i-1)) == 0 ) break;
		}
		if( nTerminalZeros > OCTAVES )
		{
#ifndef CCEMBEDDED
			fprintf( stderr, "Error: algorithm fault.\n" );
			exit( -1 );
#endif
			return -1;
		}
		Sdo_this_octave[i] = OCTAVES - nTerminalZeros -1;
	}
	return 0;
}


// TODO check only used for CCEMBEDDED
// called by UpdateFreqs( fbins ) in embeddednf.c when USE_32DFT defined
//   65536 / sample rate * 2^(imod/FIXBPERO) * BASEFREQ * 2^(OCTAVES-1)
void UpdateBins32( const uint16_t * frequencies )
{
	int i;
	int imod = 0;
	for( i = 0; i < FIXBINS; i++, imod++ )
	{
		if (imod >= FIXBPERO) imod=0;
		uint16_t freq = frequencies[imod];
		Sdatspace32A[i*2] = freq;// 65536 * cycles/sample
	}
}

// Used for embedded CC on esp8266, stm32f303 and 407, embeddedlinux
void PushSample32( int16_t dat )
{
	HandleInt( dat );
	HandleInt( dat );
}


#ifndef CCEMBEDDED
// TODO seems only for colorchord2 called by DoDFTProgressive32
void UpdateBinsForDFT32( const float * frequencies )
{
	int i;	
	for( i = 0; i < FIXBINS; i++ )
	{
		// Get freq (which here is a period) from index for same note in highest octave
		float freq = frequencies[(i%FIXBPERO) + (FIXBPERO*(OCTAVES-1))];
		// Convert to freq as 65536 * cycles/sample
		Sdatspace32A[i*2] = (65536.0/freq);
	}
}

#endif


#ifndef CCEMBEDDED
// Called from colorchord2/notefinder.c where frequencies computed as periods
void DoDFTProgressive32( float * outbins, float * frequencies, int bins, const float * databuffer, int place_in_data_buffer, int size_of_data_buffer, float q, float speedup )
{
	static float backupbins[FIXBINS];
	int i;
	static int last_place;

	memset( outbins, 0, bins * sizeof( float ) );
	goutbins = outbins;

	memcpy( outbins, backupbins, FIXBINS*4 );

	if( FIXBINS != bins )
	{
		fprintf( stderr, "Error: Bins was reconfigured.  skippy requires a constant number of bins (%d != %d).\n", FIXBINS, bins );
		return;
	}

	if( !Sdonefirstrun )
	{
		SetupDFTProgressive32();
		Sdonefirstrun = 1;
	}

	UpdateBinsForDFT32( frequencies );

	for( i = last_place; i != place_in_data_buffer; i = (i+1)%size_of_data_buffer )
	{
		int16_t ifr1 = (int16_t)( ((databuffer[i]) ) * 4095 );
		HandleInt( ifr1 );
		HandleInt( ifr1 );
	}

	UpdateOutputBins32();

	last_place = place_in_data_buffer;

	memcpy( backupbins, outbins, FIXBINS*4 );
}

#endif




