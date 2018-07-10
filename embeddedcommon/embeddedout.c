//Copyright 2015 <>< Charles Lohr under the ColorChord License.

#include "embeddedout.h"
#if DEBUGPRINT
#include <stdio.h>
#endif

//uint8_t ledArray[NUM_LIN_LEDS]; //Points to which notes correspond to these LEDs
uint8_t ledOut[NUM_LIN_LEDS*3];
uint16_t ledAmpOut[NUM_LIN_LEDS];
int16_t ledFreqOut[NUM_LIN_LEDS];
int16_t ledFreqOutOld[NUM_LIN_LEDS];

uint32_t total_note_a_prev = 0;
int diff_a_prev = 0;
int rot_dir = 1; // initial rotation direction 1
int16_t ColorCycle =0;
#define DECREASING 2
#define INCREASING 1


void Sort(uint8_t orderType, uint16_t values[], uint16_t *map, uint8_t num)
{
	//    bubble sort on a specified orderType to reorder sorted_note_map
	uint8_t holdmap;
	uint8_t change;
	int not_correct_order;
	int i,j;
	for( i = 0; i < num; i++ )
	{
		change = 0;
		for( j = 0; j < num -1 - i; j++ )
		{
			switch(orderType) {
				case DECREASING :
					not_correct_order = values[map[j]] < values[map[j+1]];
				break;
				default : // increasing
					not_correct_order = values[map[j]] > values[map[j+1]];
			}
			if ( not_correct_order )
			{
				change = 1;
				holdmap = map[j];
				map[j] = map[j+1];
				map[j+1] = holdmap;
			}
		}
		if (!change) return;
	}
}

void AssignColorledOut(uint32_t color, int16_t jshift, uint8_t repeats )
{
	int16_t i,indled;
	int16_t rmult = NUM_LIN_LEDS/(repeats+1);
	for (i=0;i<=repeats;i++) {
		indled = jshift + i*rmult;
		if( indled >= NUM_LIN_LEDS ) indled -= NUM_LIN_LEDS;
		ledOut[indled*3+0] = ( color >> 0 ) & 0xff;
		ledOut[indled*3+1] = ( color >> 8 ) & 0xff;
		ledOut[indled*3+2] = ( color >>16 ) & 0xff;
	}
}


void UpdateLinearLEDs()
{
	//Source material:
	/*
		extern uint16_t fuzzed_bins[]; //[FIXBINS]  <- The Full DFT after IIR, Blur and Taper
		extern uint16_t max_bins[]; //[FIXBINS]  <- Max of bins after Full DFT after IIR, Blur and Taper
		extern uint16_t octave_bins[OCTAVES];
		extern uint32_t maxallbins;
		extern uint16_t folded_bins[]; //[FIXBPERO] <- The folded fourier output.
		extern int16_t  note_peak_freqs[];
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
	uint16_t sorted_note_map[MAXNOTES]; //mapping from which note into the array of notes from the rest of the system.
	uint16_t snmapmap[MAXNOTES];
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
		if( note_peak_freqs[i] < 0) continue;
		sorted_note_map[sorted_map_count] = i;
		sorted_map_count++;
		total_note_a += note_peak_amps[i];
	}
	Sort(DECREASING, note_peak_amps, sorted_note_map, sorted_map_count);
	note_nerf_a = total_note_a * NERF_NOTE_PORP /100;

	// eliminates  and reduces count of notes with amp too small relative to non-eliminated
	// adjusts total amplitude
	j=sorted_map_count -1;
	while (j>=0)
	{
		uint16_t ist = note_peak_amps[sorted_note_map[j]];
		if( ist < note_nerf_a )
		{
			total_note_a -= ist;
			note_nerf_a = total_note_a * NERF_NOTE_PORP /100;
			sorted_map_count--;
			j--;
			continue;
		}
		else break;
	}

	diff_a = total_note_a_prev - total_note_a; // used to check increasing or decreasing

#define ORDER_ORGINAL 0
#define ORDER_FREQ_INC 1
#define ORDER_AMP1_DEC 2
#define ORDER_AMP2_DEC 3

	switch (COLORCHORD_SORT_NOTES) {
		case ORDER_ORGINAL : // restore orginal note order
			for (i=0; i< sorted_map_count;i++) snmapmap[i]=i;
			Sort(INCREASING, sorted_note_map, snmapmap, sorted_map_count);
			break;
		case ORDER_FREQ_INC :
			Sort(INCREASING, note_peak_freqs, sorted_note_map, sorted_map_count);
			break;
		case ORDER_AMP1_DEC: // already sorted like this
			//Sort(DECREASING, note_peak_amps, sorted_note_map, sorted_map_count);
			break;
		case ORDER_AMP2_DEC :
			Sort(DECREASING, note_peak_amps2, sorted_note_map, sorted_map_count);
			break;
	}


	//Make a copy of all of the variables into these local ones so we don't have to keep triple or double-dereferencing.
	uint16_t local_peak_amps[MAXNOTES];
	uint16_t local_peak_amps2[MAXNOTES];
	int16_t  local_peak_freq[MAXNOTES];
	uint8_t  local_note_jumped_to[MAXNOTES];

	switch (COLORCHORD_SORT_NOTES) {
		case ORDER_ORGINAL :
			for( i = 0; i < sorted_map_count; i++ )
			{
				local_peak_amps[i] = note_peak_amps[sorted_note_map[snmapmap[i]]];
				local_peak_amps2[i] = note_peak_amps2[sorted_note_map[snmapmap[i]]];
				local_peak_freq[i] = note_peak_freqs[sorted_note_map[snmapmap[i]]];
				local_note_jumped_to[i] = note_jumped_to[sorted_note_map[snmapmap[i]]];
			}
			break;
		case ORDER_FREQ_INC :
		case ORDER_AMP1_DEC :
		case ORDER_AMP2_DEC :
		default :
			for( i = 0; i < sorted_map_count; i++ )
			{
				local_peak_amps[i] = note_peak_amps[sorted_note_map[i]];
				local_peak_amps2[i] = note_peak_amps2[sorted_note_map[i]];
				local_peak_freq[i] = note_peak_freqs[sorted_note_map[i]];
				local_note_jumped_to[i] = note_jumped_to[sorted_note_map[i]];
			}
	}


	for( i = 0; i < sorted_map_count; i++ )
	{
		uint16_t ist = local_peak_amps[i];
		porpamps[i] = 0;
//TODO   probably redundant as should be same as total_note_a
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
	do
	{
		for( i = 0; (i < sorted_map_count) && total_unaccounted_leds; i++ )
		{
			porpamps[i]++; total_unaccounted_leds--;
		}
	} while( total_unaccounted_leds );

#if DEBUGPRINT
	printf( "note_nerf_a = %d,  total_size_all_notes =  %d, porportional = %d, total_accounted_leds = %d \n", note_nerf_a, total_size_all_notes, porportional,  total_accounted_leds );
	printf("snm: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", sorted_note_map[i]);
	printf( "\n" );

	printf("npf: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", note_peak_freqs[sorted_note_map[i]]);
	printf( "\n" );

	printf("npa: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", note_peak_amps[sorted_note_map[i]]);
	printf( "\n" );

	printf("lpf: ");
        for( i = 0; i < sorted_map_count; i++ )  printf( "%d /", local_peak_freq[i]);
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
			ledAmpOut[j] = ((uint32_t)local_peak_amps2[i]*NOTE_FINAL_AMP)>>16;
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
//TODO  d might be better is used both freq and amp, now only using freq
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
	// Zero all led's - maybe for very large number of led's this takes too much time and need to zero only onces that won't get reassigned below
	memset( ledOut, 0, sizeof( ledOut ) );

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
		uint32_t color = ECCtoAdjustedHEX( ledFreqOut[minimizingShift], NOTE_FINAL_SATURATION, amp );

		AssignColorledOut(color, jshift, SYMMETRY_REPEAT );
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
	int16_t freq = 0;
	uint16_t amp = 0;


	for( i = 0; i < MAXNOTES; i++ )
	{
		uint16_t ist = note_peak_amps[i];
		int16_t ifrq = note_peak_freqs[i];
		if( ifrq >= 0 && ist > amp  )
		{
			freq = ifrq;
			amp = ist;
		}
	}
	amp = (((uint32_t)(amp))*NOTE_FINAL_AMP)>>MAX_AMP2_LOG2;

	if( amp > NOTE_FINAL_AMP ) amp = NOTE_FINAL_AMP;
	uint32_t color = ECCtoAdjustedHEX( freq, NOTE_FINAL_SATURATION, amp );
	for( i = 0; i < NUM_LIN_LEDS; i++ )
	{
	AssignColorledOut(color, i, 0x00 );
	}
}

void UpdateRotatingLEDs()
{
	int16_t i;
	int16_t jshift; // int8_t jshift; caused instability especially for large no of LEDs
	int8_t shift_dist;
	int16_t freq = 0;
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
		int16_t ifrq = note_peak_freqs[i];
		if( ifrq >= 0 )
		{
			if( ist > amp2 ) {
				freq = ifrq;
				amp2 = ist;
				amp = note_peak_amps[i];
			}
			//total_note_a += note_peak_amps[i]; // or see outside loop to use bass
			//total_note_a2 += ist;
		}
	}
	total_note_a = octave_bins[0]+octave_bins[1]; // bass;

	diff_a = total_note_a_prev - total_note_a;

	// can set color intensity using amp2 or fixed value
	amp2 = (((uint32_t)(amp2))*NOTE_FINAL_AMP)>>MAX_AMP2_LOG2; // for PC 14;
	if( amp2 > NOTE_FINAL_AMP ) amp2 = NOTE_FINAL_AMP;
	//uint32_t color = ECCtoAdjustedHEX( freq, NOTE_FINAL_SATURATION, amp2 );
	uint32_t color = ECCtoAdjustedHEX( freq, NOTE_FINAL_SATURATION, NOTE_FINAL_AMP );

	// can have led_arc_len a fixed size or proportional to amp
	//led_arc_len = 5;
//TODO look below
	led_arc_len = ((uint32_t)amp * (USE_NUM_LIN_LEDS + 1) ) >> 15; // empirical
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
// move on beat	when amp2 IIR about 9
//		if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL == 0 && diff_a_prev <= 0 && diff_a > 0 ) {
			gROTATIONSHIFT += rot_dir * COLORCHORD_SHIFT_DISTANCE;
		        //printf("tnap tna %d %d dap da %d %d rot_dir %d, j shift %d\n",total_note_a_prev, total_note_a, diff_a_prev,  diff_a, rot_dir, j);
		}
	} else {
		gROTATIONSHIFT = 0; // reset
	}

	jshift = ( gROTATIONSHIFT - led_arc_len/2 ) % NUM_LIN_LEDS; // neg % pos is neg so fix
	if ( jshift < 0 ) jshift += NUM_LIN_LEDS;
	memset( ledOut, 0, sizeof( ledOut ) );
	for( i = 0; i < led_arc_len; i++, jshift++ )
	{
		AssignColorledOut(color, jshift, SYMMETRY_REPEAT );
	}

	total_note_a_prev = total_note_a;
	diff_a_prev = diff_a;

}

void DFTInLights()
{
// used folded_bins having FIXBPERO and map to USE_NUM_LIN_LEDS
	int16_t i;
	int16_t fbind;
	int16_t freq;
	uint16_t amp;
	memset( ledOut, 0, sizeof( ledOut ) );
	for( i = 0; i < USE_NUM_LIN_LEDS; i++ )
	{
//		fbind = i*(FIXBPERO-1)/(USE_NUM_LIN_LEDS-1); // exact tranformation but then need check divide by zero
		fbind = i*FIXBPERO/USE_NUM_LIN_LEDS; // this is good enough and still will not exceed FIXBPERO-1
		freq = fbind*(1<<SEMIBITSPERBIN);

// 		assign colors (0, 1, ... FIXBPERO-1 ) * 2^SEMIBITSPERBIN
// 		brightness is value of bins.
		amp = folded_bins[fbind];
		amp = (((uint32_t)(amp))*NOTE_FINAL_AMP)>>MAX_AMP2_LOG2; // for PC 14;
		if( amp > NOTE_FINAL_AMP ) amp = NOTE_FINAL_AMP;
		freq = fbind*(1<<SEMIBITSPERBIN);

/*
//		each leds color depends on value in bin. If want green lowest to yellow hightest use ROOT_NOTE_OFFSET = 110
		freq = folded_bins[fbind];
		freq = (((int32_t)(freq))*NOTE_FINAL_AMP)>>MAX_AMP2_LOG2; // for PC 14;
		if( freq > NOTERANGE ) freq = NOTERANGE;
		freq = NOTERANGE - freq;
		amp = NOTE_FINAL_AMP;
*/
		uint32_t color = ECCtoAdjustedHEX( freq, NOTE_FINAL_SATURATION, amp );
		AssignColorledOut(color, i, SYMMETRY_REPEAT );
	}
}

// pure pattern only brightness related to sound bass amplitude
void PureRotatingLEDs()
{
	int16_t i;
	int16_t jshift; // int8_t jshift; caused instability especially for large no of LEDs
	int32_t led_arc_len;
	int16_t freq;
	uint32_t amp;
	freq = ColorCycle;
//	uint32_t color = ECCtoAdjustedHEX( freq, NOTE_FINAL_SATURATION, NOTE_FINAL_AMP );

	// can have led_arc_len a fixed size or proportional to amp2
	amp = octave_bins[0]+octave_bins[1]; // bass;
	amp = (((uint32_t)(amp))*NOTE_FINAL_AMP)>>MAX_AMP2_LOG2; // for PC 14;
	if( amp > NOTE_FINAL_AMP ) amp = NOTE_FINAL_AMP;
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
	memset( ledOut, 0, sizeof( ledOut ) );
	for( i = 0; i < led_arc_len; i++, jshift++ )
	{
//		uint32_t color = ECCtoAdjustedHEX( (freq + i*NOTERANGE*NERF_NOTE_PORP/led_arc_len/100)%NOTERANGE, NOTE_FINAL_SATURATION, NOTE_FINAL_AMP );
		uint32_t color = ECCtoAdjustedHEX( (freq + i*NOTERANGE*NERF_NOTE_PORP/led_arc_len/100)%NOTERANGE, NOTE_FINAL_SATURATION, amp );
		AssignColorledOut(color, jshift, SYMMETRY_REPEAT );
	}
}


uint32_t ECCtoAdjustedHEX( int16_t note, uint8_t sat, uint8_t val )
{
	uint8_t hue = 0;
	note=(note+ROOT_NOTE_OFFSET*NOTERANGE/120)%NOTERANGE;
	uint32_t renote = ((uint32_t)note * 65535) / (NOTERANGE - 1);
	#define rn0  0
	#define hn0  298
	#define rn1  21845
	#define hn1  255
	#define rn2  43690
	#define hn2  170
	#define rn3  65535
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

