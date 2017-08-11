#ifndef _CCCONFIG_H
#define _CCCONFIG_H


#define CCEMBEDDED
#define DEBUGPRINT 0
#define RING
#define LEDS_PER_ROW 16
#define NUM_LIN_LEDS 16
#define USE_NUM_LIN_LEDS 8
#define DFREQ 16000
#define NOTE_FINAL_AMP  15 

#define COLORCHORD_SHIFT_INTERVAL 5    // shift after this many frames, 0 no shifts
#define COLORCHORD_FLIP_ON_PEAK 1      // non-zero will flip on peak total amp2
#define COLORCHORD_SHIFT_DISTANCE 1    // distance of shift + anticlockwise, - clockwise, 0 no shift
#define COLORCHORD_SORT_NOTES 2        // 0 no sort, 1 inc freq, 2 dec amps, 3 dec amps2
#define COLORCHORD_LIN_WRAPAROUND 0    // 0 no adjusting, else current led display has minimum deviation to prev


#endif

