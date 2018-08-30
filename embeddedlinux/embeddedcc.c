//
// This is the teststrap for the Embedded ColorChord System.
// It is intended as a minimal scaffolding for testing Embedded ColorChord.
//
//
// Copyright 2015 <>< Charles Lohr, See LICENSE file for license info.


#include "embeddednf.h"
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include "embeddedout.h"
#include <math.h>
#include <limits.h>
struct sockaddr_in servaddr;
int sock;

#define expected_lights NUM_LIN_LEDS
int toskip = 1;
int gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0;
int gROTATIONSHIFT = 0; //Amount of spinning of pattern around a LED ring
//TODO explore relation of NUM_LIN_LEDS and following two params
// ratio of COLORCHORD_SHIFT_INTERVAL / COLORCHORD_SHIFT_DISTANCE when less than 1 has interesting effect

void NewFrame()
{
	int i;
	char buffer[3000];
        gFRAMECOUNT_MOD_SHIFT_INTERVAL++;
	if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL >= COLORCHORD_SHIFT_INTERVAL ) gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0;
	//printf("MOD FRAME %d ******\n", gFRAMECOUNT_MOD_SHIFT_INTERVAL);
	switch( COLORCHORD_OUTPUT_DRIVER )
	{
	case 254:
		PureRotatingLEDs();
		break;
	case 255:
		DFTInLights();
		break;
	default:
		UpdateLinearLEDs(); // have variety of display options and uses COLORCHORD_OUTPUT_DRIVER to select them
	};

	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 0;

	for( i = 0; i < expected_lights * 3; i++ )
	{
		buffer[i+toskip*3] = ledOut[i];
//printf("In NF: %d\n", ledOut[i]);

	}

	int r = send(sock,buffer,expected_lights*3+3,0);
}


int main( int argc, char ** argv )
{
/*
	int32_t imax = INT_MAX;
	int32_t imin = INT_MIN;
	fprintf( stderr, "intmax = %d intmax + 1 =  %d intmax << 1 = %d intmax >> 1 = %d \n",imax, imax+1, imax<<1, imax>>1 );
	fprintf( stderr, "intmax = %x intmax + 1 =  %x intmax << 1 = %x intmax >> 1 = %x \n",imax, imax+1, imax<<1, imax>>1 );
	fprintf( stderr, "intmin = %d intmin - 1 =  %d intmin << 1 = %d intmin >> 1 = %d\n", imin, imin-1, imin<<1, imin>>1 );
	fprintf( stderr, "intmin = %x intmin - 1 =  %x intmin << 1 = %x intmin >> 1 = %x\n", imin, imin-1, imin<<1, imin>>1 );
	return 0;
*/
	int wf = 0;
	int wh = 0;
	int samplesPerFrame = 128;
	int samplesPerHandleInfo = 1;
	int ci;
	int i;
	// remember 1st argument is name of program
	if( argc < 2 )
	{
		fprintf( stderr, "Error: usage: ./embeddedcc [ip address] [num to skip, default 0]\n" );
		return -1;
	}
	printf( "%d\n", argc );
        for (int i = 0; i < argc; i++)
        {     printf( "%s\n", argv[i] );
        }
	toskip = (argc > 2)?atoi(argv[2]):0;
        fprintf( stderr, "   toskip = %d \n", toskip );

	sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);


	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(argv[1]);
	servaddr.sin_port=htons(7777);
	fprintf(stderr, "The IP address is %s\n", inet_ntoa(servaddr.sin_addr.s_addr));
	connect( sock, (struct sockaddr *)&servaddr, sizeof(servaddr) );

	InitColorChord(); // bb changed from Init() which does not seem to exist;

	// set up these to default of 1 as not set up from saved settings as in embedded8266
	for( i = 0; i < FIXBINS; i++ )
	{
		max_bins[i] = 1;
	}
	maxallbins = 1;

	switch( COLORCHORD_OUTPUT_DRIVER )
	{
		case 254:
			samplesPerFrame = 32;
			samplesPerHandleInfo = 128;
		break;
		default:
			samplesPerFrame = 128; // <= but if < required new def of max to respond to peaks
			samplesPerHandleInfo = 128;
	};


	int isamp=0; // initial sample index
//	for (i=0; i<TIME_LIMIT*DFREQ; i++)
	while( ( ci = getchar() ) != EOF ) // streaming input rates limits speed of loop
	{

// get sample from input
//		int cs = ci - 0x80;
		int cs;
// generate sample from functional form
		float octpersec = OCT_PER_SECOND;
		if (isamp < TIME_SWEEP*DFREQ) {
			cs = 127.0 * sinf(2.0/octpersec/log(2)*3.14159*55.0*pow(2.0,((float)isamp*octpersec/DFREQ))); //Chirp start 55 Hz
		} else {
			cs = 127.0 * sinf(2.0*3.14159*55.0*(float)isamp/DFREQ*pow(2.0,(TIME_SWEEP*octpersec))); //stay at final freq of chirp
		}
//		int cs = 127.0 * sinf(2.0*3.14159*110.0*(float)i/DFREQ);
		isamp++;
		if (isamp>TIME_LIMIT*DFREQ) return 0; // stop after TIME_LIMIT secs
#ifdef USE_32DFT
		//PushSample32( ((int8_t)cs)*32 );
		PushSample32( ((int8_t)cs) );
#else
		Push8BitIntegerSkippy( (int8_t)cs );
#endif
		wh++;
		if( wh >= samplesPerHandleInfo )
		{
			HandleFrameInfo();
			wh = 0;
		}
		wf++;
		if( wf >= samplesPerFrame )
		{
			NewFrame();
			wf = 0;
		}
	}
	return 0;
}

