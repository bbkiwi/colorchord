#ifndef _CCCONFIG_H
#define _CCCONFIG_H


#define CCEMBEDDED
#define DEBUGPRINT 0
#define RING
#define LEDS_PER_ROW 16
#define NUM_LIN_LEDS 16
#define USE_NUM_LIN_LEDS 1
#define DFREQ 16000
#define NOTE_FINAL_AMP  255		//Final brightness Number from 0...255

//Controls, basically, the minimum size of the splotches.
#define NERF_NOTE_PORP 15 //value from 0 to 255

#define COLORCHORD_OUTPUT_DRIVER 3    // 0 UpdateLinearLEDs, 1 UpdateAllSameLEDs, 2 UpdateRotatingLEDs; 3 PureRotatingLEDs (not depend on sound);
#define COLORCHORD_SHIFT_INTERVAL 1000  // shift after this many frames, 0 no shifts
#define COLORCHORD_FLIP_ON_PEAK 1      // non-zero will flip on peak total amp2
#define COLORCHORD_SHIFT_DISTANCE 1    // distance of shift + anticlockwise, - clockwise, 0 no shift (if divisor of NUM_LIN_LEDS strobe effects)
#define COLORCHORD_SORT_NOTES 0        // 0 no sort, 1 inc freq, 2 dec amps, 3 dec amps2
#define COLORCHORD_LIN_WRAPAROUND 0    // 0 no adjusting, else current led display has minimum deviation to prev

#define OCTAVES  5
#define FIXBPERO 24
//You may increase this past 5 but if you do, the amplitude of your incoming
//signal must decrease.  Increasing this value makes responses slower.  Lower
//values are more responsive.
#define DFTIIR 3
//The higher the number the slackier your FFT will be come.
#define FUZZ_IIR_BITS  1
//Notes are the individually identifiable notes we receive from the sound.
//We track up to this many at one time.  Just because a note may appear to
//vaporize in one frame doesn't mean it is annihilated immediately.
#define MAXNOTES  12
#define FILTER_BLUR_PASSES 2

//Determines bit shifts for where notes lie.  We represent notes with an
//uint8_t.  We have to define all of the possible locations on the note line
//in this. note_frequency = 0..((1<<SEMIBITSPERBIN)*FIXBPERO-1)
#define SEMIBITSPERBIN 3  // so NOTERANGE ((1<<SEMIBITSPERBIN)*FIXBPERO)
//If there is detected note this far away from an established note, we will
//then consider this new note the same one as last time, and move the
//established note.  This is also used when combining notes.  It is this
//distance times two.
#define MAX_JUMP_DISTANCE 4
#define MAX_COMBINE_DISTANCE 7
//These control how quickly the IIR for the note strengths respond.  AMP 1 is
//the response for the slow-response, or what we use to determine size of
//splotches, AMP 2 is the quick response, or what we use to see the visual
//strength of the notes.
#define AMP_1_IIR_BITS 3 // 3 fastest below 3 blows up
#define AMP_2_IIR_BITS 0 // 0 fastest reactions
//This is the amplitude, coming from folded_bins.  If the value is below this
//it is considered a non-note.
#define MIN_AMP_FOR_NOTE 80
//If the strength of a note falls below this, the note will disappear, and be
//recycled back into the unused list of notes.
#define MINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR 64
#define MAX_AMP2_LOG2 13 // used to scale AMP2

#endif

