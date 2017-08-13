#ifndef _CCCONFIG_H
#define _CCCONFIG_H


#define CCEMBEDDED
#define DEBUGPRINT 1
#define RING
#define LEDS_PER_ROW 16
#define NUM_LIN_LEDS 16
#define USE_NUM_LIN_LEDS 8
#define DFREQ 16000
#define NOTE_FINAL_AMP  150 

#define COLORCHORD_OUTPUT_DRIVER 0     // 0 UpdateLinearLEDs, 1 UpdateAllSameLEDs, 2 UpdateRotatingLEDs
#define COLORCHORD_SHIFT_INTERVAL 0    // shift after this many frames, 0 no shifts
#define COLORCHORD_FLIP_ON_PEAK 0      // non-zero will flip on peak total amp2
#define COLORCHORD_SHIFT_DISTANCE 1    // distance of shift + anticlockwise, - clockwise, 0 no shift
#define COLORCHORD_SORT_NOTES 2        // 0 no sort, 1 inc freq, 2 dec amps, 3 dec amps2
#define COLORCHORD_LIN_WRAPAROUND 0    // 0 no adjusting, else current led display has minimum deviation to prev

#define AMP_1_IIR_BITS 5

#endif

