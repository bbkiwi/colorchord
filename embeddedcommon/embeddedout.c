//Copyright 2015 <>< Charles Lohr under the ColorChord License.


#include "embeddedout.h"
#if DEBUGPRINT
#include <stdio.h>
#endif

//uint8_t ledArray[NUM_LIN_LEDS]; //Points to which notes correspond to these LEDs
uint8_t ledOut[NUM_LIN_LEDS*3];
uint16_t ledAmpOut[NUM_LIN_LEDS];
uint8_t ledFreqOut[NUM_LIN_LEDS];
uint8_t ledFreqOutOld[NUM_LIN_LEDS];

uint32_t total_note_a_prev = 0;
int diff_a_prev = 0;
int rot_dir = 1; // initial rotation direction 1
uint8_t ColorCycle =0; 

void UpdateLinearLEDs()
{
	//Source material:
	/*
		extern uint8_t  note_peak_freqs[];
		extern uint16_t note_peak_amps[];  //[MAXNOTES] 
		extern uint16_t note_peak_amps2[];  //[MAXNOTES]  (Responds quicker)
		extern uint8_t  note_jumped_to[]; //[MAXNOTES] When a note combines into another one,
		extern int gFRAMECOUNT_MOD_SHIFT_INTERVAL; // modular count of calls to NewFrame() in user_main.c
		(from ccconfig.h or defaults defined in embeddedout.h
		COLORCHORD_SHIFT_INTERVAL; // controls speed of shifting if 0 no shift
		COLORCHORD_FLIP_ON_PEAK; //if non-zero gives flipping at peaks of shift direction, 0 no flip
		COLORCHORD_SHIFT_DISTANCE; //distance of shift
		COLORCHORD_SORT_NOTES; // 0 no sort, 1 inc freq, 2 dec amps, 3 dec amps2
		COLORCHORD_LIN_WRAPAROUND; // 0 no adjusting, else current led display has minimum deviation to prev
	*/

	//Notes are found above a minimum amplitude
	//Goal: On a linear array of size USE_NUM_LIN_LEDS Make splotches of light that are proportional to the amps strength of theses notes.
	//Color them according to note_peak_freq with brightness related to amps2
	//Put this linear array on a ring with NUM_LIN_LEDS and optionally rotate it with optionally direction changes on peak amps2

	int16_t i; // uint8_t i; caused instability especially for large no of LEDS
	int16_t j, l;
	int16_t minimizingShift;
	uint32_t total_size_all_notes = 0;
	int32_t porpamps[MAXNOTES]; //number of LEDs for each corresponding note.
	uint8_t sorted_note_map[MAXNOTES]; //mapping from which note into the array of notes from the rest of the system.
	uint8_t sorted_map_count = 0;
	uint32_t note_nerf_a = 0;
	uint32_t total_note_a = 0;
	int diff_a = 0;
	int8_t shift_dist = 0;
	int16_t jshift; // int8_t jshift; caused instability especially for large no of LEDS

#if DEBUGPRINT
	printf( "Note Peak Freq: " );
        for( i = 0; i < MAXNOTES; i++ ) printf( " %5d /", note_peak_freqs[i]  );
	printf( "\n" );
        printf( "Note Peak Amps: " );
        for( i = 0; i < MAXNOTES; i++ ) printf( " %5d /", note_peak_amps[i]  );
	printf( "\n" );
        printf( "Note Peak Amp2: " );
        for( i = 0; i < MAXNOTES; i++ ) printf( " %5d /", note_peak_amps2[i]  );
	printf( "\n" );
        printf( "Note jumped to: " );
        for( i = 0; i < MAXNOTES; i++ ) printf( " %5d /", note_jumped_to[i]  );
	printf( "\n" );
#endif

	for( i = 0; i < MAXNOTES; i++ )
	{
		if( note_peak_freqs[i] == 255 ) continue;
		total_note_a += note_peak_amps[i];
	}

	diff_a = total_note_a_prev - total_note_a;

	note_nerf_a = ((total_note_a * NERF_NOTE_PORP)>>8);

	// ignore notes with amp too small or freq 255
	for( i = 0; i < MAXNOTES; i++ )
	{
		uint16_t ist = note_peak_amps[i];
		uint8_t nff = note_peak_freqs[i];
		if( nff == 255 )
		{
			continue;
		}
		if( ist < note_nerf_a )
		{
			continue;
		}
		sorted_note_map[sorted_map_count] = i;
		sorted_map_count++;
	}

	if ( COLORCHORD_SORT_NOTES ) {
		// note local_note_jumped_to still give original indices of notes (which may not even been inclued
		//    due to being eliminated as too small amplitude
		//    bubble sort on a specified key to reorder sorted_note_map
		uint8_t hold8;
		uint8_t change;
		int not_correct_order;
		for( i = 0; i < sorted_map_count; i++ )
		{
			change = 0;
			for( j = 0; j < sorted_map_count -1 - i; j++ )
			{
				switch(COLORCHORD_SORT_NOTES) {
					case 2 : // amps decreasing
						not_correct_order = note_peak_amps[sorted_note_map[j]] < note_peak_amps[sorted_note_map[j+1]];
					break;
					case 3 : // amps2 decreasing
						not_correct_order = note_peak_amps2[sorted_note_map[j]] < note_peak_amps2[sorted_note_map[j+1]];
					break;
					default : // freq increasing
						not_correct_order = note_peak_freqs[sorted_note_map[j]] > note_peak_freqs[sorted_note_map[j+1]];
				}
				if ( not_correct_order )
				{
					change = 1;
					hold8 = sorted_note_map[j];
					sorted_note_map[j] = sorted_note_map[j+1];
					sorted_note_map[j+1] = hold8;
				}
			}
			if (!change) break;
		}
	}
	//Make a copy of all of the variables into local ones so we don't have to keep double-dereferencing.
	uint16_t local_peak_amps[MAXNOTES];
	uint16_t local_peak_amps2[MAXNOTES];
	uint8_t  local_peak_freq[MAXNOTES];
	uint8_t  local_note_jumped_to[MAXNOTES];

	for( i = 0; i < sorted_map_count; i++ )
	{
		local_peak_amps[i] = note_peak_amps[sorted_note_map[i]] - note_nerf_a;
		local_peak_amps2[i] = note_peak_amps2[sorted_note_map[i]];
		local_peak_freq[i] = note_peak_freqs[sorted_note_map[i]];
		local_note_jumped_to[i] = note_jumped_to[sorted_note_map[i]];
	}

	for( i = 0; i < sorted_map_count; i++ )
	{
		uint16_t ist = local_peak_amps[i];
		porpamps[i] = 0;
		total_size_all_notes += local_peak_amps[i];
	}

	if( total_size_all_notes == 0 )
	{
		for( j = 0; j < NUM_LIN_LEDS * 3; j++ )
		{
			ledOut[j] = 0;
		}
		return;
	}

	uint32_t porportional = (uint32_t)(USE_NUM_LIN_LEDS<<16)/((uint32_t)total_size_all_notes);

	uint16_t total_accounted_leds = 0;

	for( i = 0; i < sorted_map_count; i++ )
	{
		porpamps[i] = (local_peak_amps[i] * porportional) >> 16;
		total_accounted_leds += porpamps[i];
	}

	int16_t total_unaccounted_leds = USE_NUM_LIN_LEDS - total_accounted_leds;

#if DEBUGPRINT
	printf( "note_nerf_a = %d,  total_size_all_notes =  %d, porportional = %d, total_accounted_leds = %d \n", note_nerf_a, total_size_all_notes, porportional,  total_accounted_leds );
	printf("snm: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", sorted_note_map[i]);
	printf( "\n" );

	printf("npf: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", note_peak_freqs[sorted_note_map[i]]);
	printf( "\n" );

	printf("lpf: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", local_peak_freq[i]);
	printf( "\n" );

	printf("npa: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", note_peak_amps[sorted_note_map[i]]);
	printf( "\n" );

	printf("lpa: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", local_peak_amps[i]);
	printf( "\n" );

	printf("lpa2: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", local_peak_amps2[i]);
	printf( "\n" );

	printf("porp: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", porpamps[i]);
	printf( "\n" );

	printf("lnjt: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", local_note_jumped_to[i]);
	printf( "\n" );

#endif


	int addedlast = 1;
	do
	{
		for( i = 0; i < sorted_map_count && total_unaccounted_leds; i++ )
		{
			porpamps[i]++; total_unaccounted_leds--;
			addedlast = 1;
		}
	} while( addedlast && total_unaccounted_leds );

	//Assign the linear LEDs info for 0, 1, ..., USE_NUM_LIN_LEDS
	//Each note (above a minimum amplitude) produces an interval
	//Its color relates to the notes frequency, the intensity to amps2,
	//  and the length proportional to amps
	j = 0;
	for( i = 0; i < sorted_map_count; i++ )
	{
		while( porpamps[i] > 0 )
		{
			ledFreqOut[j] = local_peak_freq[i];
			ledAmpOut[j] = (local_peak_amps2[i]*NOTE_FINAL_AMP)>>8;
			j++;
			porpamps[i]--;
		}
	}

	//This part possibly run on an embedded system with small number of LEDs.
	if (COLORCHORD_LIN_WRAPAROUND ) {
		//printf("NOTERANGE: %d ", NOTERANGE); //192
		// finds an index minimizingShift so that shifting the used leds will have the minimum deviation
		//    from the previous linear pattern
		uint16_t midx = 0;
		uint32_t mqty = 100000000;
		for( j = 0; j < USE_NUM_LIN_LEDS; j++ )
		{
			uint32_t dqty;
			uint16_t localj;
			dqty = 0;
			localj = j;
			for( l = 0; l < USE_NUM_LIN_LEDS; l++ )
			{
				int32_t d = (int32_t)ledFreqOut[localj] - (int32_t)ledFreqOutOld[l];
				if( d < 0 ) d *= -1;
				if( d > (NOTERANGE>>1) ) { d = NOTERANGE - d + 1; }
				dqty += ( d * d );
				localj++;
				if( localj >= USE_NUM_LIN_LEDS ) localj = 0;
			}
			if( dqty < mqty )
			{
				mqty = dqty;
				midx = j;
			}
		}
		minimizingShift = midx;
		//printf("spin: %d, min deviation: %d\n", minimizingShift, mqty);
	} else {
		minimizingShift = 0;
	}
	// if option change direction on max peaks of total amplitude
	if (COLORCHORD_FLIP_ON_PEAK ) {
		if (diff_a_prev <= 0 && diff_a > 0) {
			rot_dir *= -1;
		}
	} else rot_dir = 1;

        // want possible extra spin to relate to changes peak intensity
	// now every COLORCHORD_SHIFT_INTERVAL th frame
	if (COLORCHORD_SHIFT_INTERVAL != 0 ) {
		if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL == 0 ) {
			gROTATIONSHIFT += rot_dir * COLORCHORD_SHIFT_DISTANCE;
		        //printf("tnap tna %d %d dap da %d %d rot_dir %d, j shift %d\n",total_note_a_prev, total_note_a, diff_a_prev,  diff_a, rot_dir, j);
		}
	} else {
		gROTATIONSHIFT = 0; // reset
	}

	// compute shift to start rotating pattern around all the LEDs
	jshift = ( gROTATIONSHIFT ) % NUM_LIN_LEDS; // neg % pos is neg so fix
	if ( jshift < 0 ) jshift += NUM_LIN_LEDS;

#if DEBUGPRINT
	printf("rot_dir %d, gROTATIONSHIFT %d, jshift %d, gFRAMECOUNT_MOD_SHIFT_INTERVAL %d\n", rot_dir, gROTATIONSHIFT, jshift, gFRAMECOUNT_MOD_SHIFT_INTERVAL);
	printf("NOTE_FINAL_AMP = %d\n", NOTE_FINAL_AMP);
	printf("leds: ");
#endif
	// put linear pattern of USE_NUM_LIN_LEDS on ring NUM_LIN_LEDs
	for( l = 0; l < USE_NUM_LIN_LEDS; l++, jshift++, minimizingShift++ )
	{
		if( jshift >= NUM_LIN_LEDS ) jshift = 0;
		//lefFreqOutOld and adjusting minimizingShift needed only if wraparound
		if ( COLORCHORD_LIN_WRAPAROUND ) {
			if( minimizingShift >= USE_NUM_LIN_LEDS ) minimizingShift = 0;
			ledFreqOutOld[l] = ledFreqOut[minimizingShift];
		}
		uint16_t amp = ledAmpOut[minimizingShift];
#if DEBUGPRINT
	        printf("%d:%d/", ledFreqOut[minimizingShift], amp);
#endif
		if( amp > NOTE_FINAL_AMP ) amp = NOTE_FINAL_AMP;
		uint32_t color = ECCtoHEX( (ledFreqOut[minimizingShift]+ROOT_NOTE_OFFSET)%NOTERANGE, NOTE_FINAL_SATURATION, amp );
		ledOut[jshift*3+0] = ( color >> 0 ) & 0xff;
		ledOut[jshift*3+1] = ( color >> 8 ) & 0xff;
		ledOut[jshift*3+2] = ( color >>16 ) & 0xff;
	}
	// blackout remaining LEDs on ring
//TODO this could be sped up in case NUM_LIN_LEDS is much greater than USE_NUM_LIN_LEDS
//      by blacking out only previous COLORCHORD_SHIFT_DISTANCE LEDs that were not overwritten 
//      but if direction changing might be tricky
	for( l = USE_NUM_LIN_LEDS; l < NUM_LIN_LEDS; l++, jshift++ )
	{
		if( jshift >= NUM_LIN_LEDS ) jshift = 0;
		ledOut[jshift*3+0] = 0x0;
		ledOut[jshift*3+1] = 0x0;
		ledOut[jshift*3+2] = 0x0;
	}



#if DEBUGPRINT
        printf( "\n" );
	printf("bytes: ");
        for( i = 0; i < USE_NUM_LIN_LEDS; i++ )  printf( "%02x%02x%02x-", ledOut[i*3+0], ledOut[i*3+1],ledOut[i*3+2]);
	printf( "\n\n" );
#endif
	total_note_a_prev = total_note_a;
	diff_a_prev = diff_a;
}

void UpdateAllSameLEDs()
{
	int16_t i;
	int8_t j;
	uint8_t freq = 0;
	uint16_t amp = 0;


	for( i = 0; i < MAXNOTES; i++ )
	{
		uint16_t ist = note_peak_amps2[i];
		uint8_t ifrq = note_peak_freqs[i];
		if( ifrq != 255 && ist > amp  )
		{
			freq = ifrq;
			amp = ist;
		}
	}
	amp = (((uint32_t)(amp))*NOTE_FINAL_AMP)>>MAX_AMP2_LOG2;

	if( amp > NOTE_FINAL_AMP ) amp = NOTE_FINAL_AMP;
	uint32_t color = ECCtoHEX( (freq+ROOT_NOTE_OFFSET)%NOTERANGE, NOTE_FINAL_SATURATION, amp );
	for( i = 0; i < NUM_LIN_LEDS; i++ )
	{
		ledOut[i*3+0] = ( color >> 0 ) & 0xff;
		ledOut[i*3+1] = ( color >> 8 ) & 0xff;
		ledOut[i*3+2] = ( color >>16 ) & 0xff;
	}
}

void UpdateRotatingLEDs()
{
	int16_t i;
	int16_t jshift; // int8_t jshift; caused instability especially for large no of LEDs
	int8_t shift_dist;
	uint8_t freq = 0;
	uint16_t amp = 0;
	uint16_t amp2 = 0;
	uint32_t note_nerf_a = 0;
	uint32_t total_note_a = 0;
	//uint32_t total_note_a2 = 0;
	int diff_a = 0;
	int32_t led_arc_len;
	char stret[256];
	char *stt = &stret[0];

	//printf("%5d %5d %5d \n", bass, mid, treb);

	for( i = 0; i < MAXNOTES; i++ )
	{
		uint16_t ist = note_peak_amps2[i];
		uint8_t ifrq = note_peak_freqs[i];
		if( ifrq != 255 )
		{
			if( ist > amp2 ) {
				freq = ifrq;
				amp2 = ist;
			}
			//total_note_a += note_peak_amps[i]; // or see outside loop to use bass
			//total_note_a2 += ist;
		}
	}
	total_note_a = bass;

	diff_a = total_note_a_prev - total_note_a;

	// can set color intensity using amp2
	amp = (((uint32_t)(amp2))*NOTE_FINAL_AMP)>>MAX_AMP2_LOG2; // for PC 14;
	if( amp > NOTE_FINAL_AMP ) amp = NOTE_FINAL_AMP;
	//uint32_t color = ECCtoHEX( (freq+ROOT_NOTE_OFFSET)%NOTERANGE, NOTE_FINAL_SATURATION, amp );
	uint32_t color = ECCtoHEX( (freq+ROOT_NOTE_OFFSET)%NOTERANGE, NOTE_FINAL_SATURATION, NOTE_FINAL_AMP );

	// can have led_arc_len a fixed size or proportional to amp2
	//led_arc_len = 5;
	led_arc_len = (amp * (USE_NUM_LIN_LEDS + 1) ) >> 8;
	//printf("amp2 = %d, amp = %d, led_arc_len = %d, NOTE_FINAL_AMP = %d\n", amp2,  amp, led_arc_len, NOTE_FINAL_AMP );
	//stt += ets_sprintf( stt, "amp2 = %d, amp = %d, led_arc_len = %d, NOTE_FINAL_AMP = %d\n", amp2,  amp, led_arc_len, NOTE_FINAL_AMP );
	//uart0_sendStr(stret);

        // want possible extra spin to relate to changes peak intensity
	if (COLORCHORD_FLIP_ON_PEAK ) {
		if (diff_a_prev <= 0 && diff_a > 0) {
			rot_dir *= -1;
		}
	} else rot_dir = 1;

	// now every COLORCHORD_SHIFT_INTERVAL th frame
	if (COLORCHORD_SHIFT_INTERVAL != 0 ) {
		if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL == 0 ) {
			gROTATIONSHIFT += rot_dir * COLORCHORD_SHIFT_DISTANCE;
		        //printf("tnap tna %d %d dap da %d %d rot_dir %d, j shift %d\n",total_note_a_prev, total_note_a, diff_a_prev,  diff_a, rot_dir, j);
		}
	} else {
		gROTATIONSHIFT = 0; // reset
	}

	jshift = ( gROTATIONSHIFT - led_arc_len/2 ) % NUM_LIN_LEDS; // neg % pos is neg so fix
	if ( jshift < 0 ) jshift += NUM_LIN_LEDS;

	for( i = 0; i < led_arc_len; i++, jshift++ )
	{
		// even if led_arc_len exceeds NUM_LIN_LEDS using jshift will prevent over running ledOut
		if( jshift >= NUM_LIN_LEDS ) jshift = 0;
		ledOut[jshift*3+0] = ( color >> 0 ) & 0xff;
		ledOut[jshift*3+1] = ( color >> 8 ) & 0xff;
		ledOut[jshift*3+2] = ( color >>16 ) & 0xff;
	}

	for( i = led_arc_len; i < NUM_LIN_LEDS; i++, jshift++ )
	{
		if( jshift >= NUM_LIN_LEDS ) jshift = 0;
		ledOut[jshift*3+0] = 0x0;
		ledOut[jshift*3+1] = 0x0;
		ledOut[jshift*3+2] = 0x0;
	}
	total_note_a_prev = total_note_a;
	diff_a_prev = diff_a;

}

// pure pattern not reacting to sound
void PureRotatingLEDs()
{
	int16_t i;
	int16_t jshift; // int8_t jshift; caused instability especially for large no of LEDs
	int32_t led_arc_len;
	uint8_t freq;
	freq = ColorCycle;
//	uint32_t color = ECCtoHEX( (freq+ROOT_NOTE_OFFSET)%NOTERANGE, NOTE_FINAL_SATURATION, NOTE_FINAL_AMP );

	// can have led_arc_len a fixed size or proportional to amp2
	led_arc_len = USE_NUM_LIN_LEDS;

	// now every COLORCHORD_SHIFT_INTERVAL th frame
	if (COLORCHORD_SHIFT_INTERVAL != 0 ) {
		if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL == 0 ) {
			gROTATIONSHIFT += rot_dir * COLORCHORD_SHIFT_DISTANCE;
		        //printf("tnap tna %d %d dap da %d %d rot_dir %d, j shift %d\n",total_note_a_prev, total_note_a, diff_a_prev,  diff_a, rot_dir, j);
			ColorCycle++;
			if (ColorCycle >= NOTERANGE) ColorCycle = 0;
			if (ColorCycle == 0 && COLORCHORD_FLIP_ON_PEAK) rot_dir *= -1;
		}
	} else {
		ColorCycle = 0;
		gROTATIONSHIFT = 0; // reset
	}

	jshift = ( gROTATIONSHIFT - led_arc_len/2 ) % NUM_LIN_LEDS; // neg % pos is neg so fix
	if ( jshift < 0 ) jshift += NUM_LIN_LEDS;
	for( i = 0; i < led_arc_len; i++, jshift++ )
	{
		uint32_t color = ECCtoHEX( (freq + i*NERF_NOTE_PORP/led_arc_len + ROOT_NOTE_OFFSET)%NOTERANGE, NOTE_FINAL_SATURATION, NOTE_FINAL_AMP );
		// even if led_arc_len exceeds NUM_LIN_LEDS using jshift will prevent over running ledOut
		if( jshift >= NUM_LIN_LEDS ) jshift = 0;
		ledOut[jshift*3+0] = ( color >> 0 ) & 0xff;
		ledOut[jshift*3+1] = ( color >> 8 ) & 0xff;
		ledOut[jshift*3+2] = ( color >>16 ) & 0xff;
	}

	for( i = led_arc_len; i < NUM_LIN_LEDS; i++, jshift++ )
	{
		if( jshift >= NUM_LIN_LEDS ) jshift = 0;
		ledOut[jshift*3+0] = 0x0;
		ledOut[jshift*3+1] = 0x0;
		ledOut[jshift*3+2] = 0x0;
	}
}




/* 
uint32_t ECCtoHEX( uint8_t note, uint8_t sat, uint8_t val )
{
	uint16_t hue = 0;
	uint32_t renote = ((uint32_t)note * 65536) / NOTERANGE;

	hue = renote;
	hue >>= 8;
	hue = 256 - hue; //reverse rainbow order
	hue += 43; //start yellow
//	printf("%d %d \n", renote, hue);

	return EHSVtoHEX( hue, sat, val );
}
*/
/*
uint32_t ECCtoHEX( uint8_t note, uint8_t sat, uint8_t val )
{
	uint16_t hue = 0;
	uint32_t renote = ((uint32_t)note * 65536) / NOTERANGE;
	uint32_t rn0 = 0;
	uint16_t hn0 = 7*255/6;
	uint32_t rn1 = 65536/3;
	uint16_t hn1 = 255;
	uint32_t rn2 = 2 * rn1;
	uint16_t hn2 = 2*255/3;
	uint32_t rn3 = 65536;
	uint16_t hn3 = 255/6;
	if( renote < rn1 )
	{	hue = hn0 + (renote - rn0) * (hn1 - hn0) / (rn1 - rn0);
	}
	else if( renote < rn2 )
	{	hue = hn1 + (renote - rn1) * (hn2 - hn1) / (rn2 - rn1);
	}
	else
	{	hue = hn2 + (renote - rn2) * (hn3 - hn2) / (rn3 - rn2);
	}
	return EHSVtoHEX( hue, sat, val );
}
*/
uint32_t ECCtoHEX( uint8_t note, uint8_t sat, uint8_t val )
{
	uint16_t hue = 0;
	uint32_t renote = ((uint32_t)note * 65536) / NOTERANGE;
	#define rn0  0
	#define hn0  298
	#define rn1  21845
	#define hn1  255
	#define rn2  43690
	#define hn2  170
	#define rn3  65536
	#define hn3  43
	if( renote < rn1 )
	{	hue = hn0 - (renote - rn0) * (43) / (21845);
	}
	else if( renote < rn2 )
	{	hue = hn1 - (renote - rn1) * (85) / (21845);
	}
	else
	{	hue = hn2 - (renote - rn2) * (127) / (21846);
	}
	return EHSVtoHEX( hue, sat, val );
}


uint32_t EHSVtoHEX( uint8_t hue, uint8_t sat, uint8_t val )
{
	#define SIXTH1 43
	#define SIXTH2 85
	#define SIXTH3 128
	#define SIXTH4 171
	#define SIXTH5 213
	static const uint8_t gamma_correction_table[256] = {
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,
		1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,
		3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   5,   6,   6,   6,   6,   7,
		7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  10,  11,  11,  11,  12,  12,
		13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,
		20,  21,  21,  22,  22,  23,  24,  24,  25,  25,  26,  27,  27,  28,  29,  29,
		30,  31,  31,  32,  33,  34,  34,  35,  36,  37,  38,  38,  39,  40,  41,  42,
		42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,
		58,  59,  60,  61,  62,  63,  64,  65,  66,  68,  69,  70,  71,  72,  73,  75,
		76,  77,  78,  80,  81,  82,  84,  85,  86,  88,  89,  90,  92,  93,  94,  96,
		97,  99, 100, 102, 103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119, 120,
		122, 124, 125, 127, 129, 130, 132, 134, 136, 137, 139, 141, 143, 145, 146, 148,
		150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180,
		182, 184, 186, 188, 191, 193, 195, 197, 199, 202, 204, 206, 209, 211, 213, 215,
		218, 220, 223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255 };
	uint16_t or = 0, og = 0, ob = 0;

	// move in rainbow order RYGCBM as hue from 0 to 255

	if( hue < SIXTH1 ) //Ok: Red->Yellow
	{
		or = 255;
		og = (hue * 255) / (SIXTH1);
	}
	else if( hue < SIXTH2 ) //Ok: Yellow->Green
	{
		og = 255;
		or = 255 - (hue - SIXTH1) *255 / SIXTH1;
	}
	else if( hue < SIXTH3 )  //Ok: Green->Cyan
	{
		og = 255;
		ob = (hue - SIXTH2) * 255 / (SIXTH1);
	}
	else if( hue < SIXTH4 ) //Ok: Cyan->Blue
	{
		ob = 255;
		og = 255 - (hue - SIXTH3) * 255 / SIXTH1;
	}
	else if( hue < SIXTH5 ) //Ok: Blue->Magenta
	{
		ob = 255;
		or = (hue - SIXTH4) * 255 / SIXTH1;
	}
	else //Magenta->Red
	{
		or = 255;
		ob = 255 - (hue - SIXTH5) * 255 / SIXTH1;
	}

	uint16_t rv = val;
	if( rv > 128 ) rv++;
	uint16_t rs = sat;
	if( rs > 128 ) rs++;

	//or, og, ob range from 0...255 now.
	//Apply saturation giving OR..OB == 0..65025
	or = or * rs + 255 * (256-rs);
	og = og * rs + 255 * (256-rs);
	ob = ob * rs + 255 * (256-rs);
	or >>= 8;
	og >>= 8;
	ob >>= 8;
	//back to or, og, ob range 0...255 now.
	//Need to apply saturation and value.
	or = (or * val)>>8;
	og = (og * val)>>8;
	ob = (ob * val)>>8;
//	printf( "  hue = %d r=%d g=%d b=%d rs=%d rv=%d\n", hue, or, og, ob, rs, rv );

	or = gamma_correction_table[or];
	og = gamma_correction_table[og];
	ob = gamma_correction_table[ob];
//	return or | (og<<8) | ((uint32_t)ob<<16);
	return og | (or<<8) | ((uint32_t)ob<<16); //new
}

