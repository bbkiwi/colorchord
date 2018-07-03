#define STOPPER 256                                   /* Larger than any datum */
#define    MEDIAN_FILTER_SIZE    0x0007
#include "c_types.h"
#include "esp82xxutil.h"

//This is from https://embeddedgurus.com/stack-overflow/2010/10/median-filtering/
// https://www.embedded.com/design/programming-languages-and-tools/4399504/Better-Than-Average
// modified by bb to sort from lowest to highest and all values 0..255
// cant get it to work for ESP8266 NOTE have to mode Makefile to have this a source
//    and declare extrn in user_main.c
// if only run first bit of code and bail out with return datum get wdt resets
// datpoint = buffer is statement causing problem
// wrote datpoint = &buffer[0] and then with printf at 44 and return at 46
//     runs printing on serial, but if take out printf get resets
// Do structures being pointed to need to be 32 bit aligned?
// Must pointers be uint32_t?
// datum will be uint8_t so between 0 and 255
uint16_t median_filter(uint16_t datum)
{
 struct pair
 {
   struct pair   *point;                              /* Pointers forming list linked in sorted order */
   uint16_t  value;                                   /* Values to sort */
 };
 static struct pair buffer[MEDIAN_FILTER_SIZE] = {0}; /* Buffer of nwidth pairs */
 static struct pair *datpoint = buffer;               /* Pointer into circular buffer of data */
 static struct pair large = {NULL, STOPPER};          /* Chain stopper */
 static struct pair small = {&large, 0};                /* Pointer to head (smallest) of linked list.*/

 struct pair *successor;                              /* Pointer to successor of replaced data item */
 struct pair *scan;                                   /* Pointer used to scan up the sorted list */
 struct pair *scanold;                                /* Previous value of scan */
 struct pair *median;                                 /* Pointer to median */
 uint16_t i;

if (datum >= STOPPER)
 {
   datum = STOPPER - 1;                             /* No stoppers allowed. */
 }

//printf("hjhhj %i %i\n", datpoint - buffer, MEDIAN_FILTER_SIZE);
return datum; // runs and does nothing
 if ( (++datpoint - buffer) >= MEDIAN_FILTER_SIZE)
  {
   datpoint = &buffer[0];                               /* Increment and wrap data in pointer.*/
   //printf("%i %i  %i\n", datpoint, buffer, datpoint - buffer);
}
return datum; // if get this far makes wdt resets
 datpoint->value = datum;                           /* Copy in new datum */

 successor = datpoint->point;                       /* Save pointer to old value's successor */
 median = &small;                                     /* Median initially to first in chain */
 scanold = NULL;                                    /* Scanold initially null. */
 scan = &small;                                       /* Points to pointer to first (smallest) datum in chain */
return datum; // if get this far makes wdt resets

 /* Handle chain-out of first item in chain as special case */
 if (scan->point == datpoint)
 {
   scan->point = successor;
 }
 scanold = scan;                                     /* Save this pointer and   */
 scan = scan->point ;                                /* step up chain */

 /* Loop through the chain, normal loop exit via break. */
 for (i = 0 ; i < MEDIAN_FILTER_SIZE; ++i)
 {
   /* Handle odd-numbered item in chain  */
   if (scan->point == datpoint)
   {
     scan->point = successor;                      /* Chain out the old datum.*/
   }

   if (scan->value > datum)                        /* If datum is smaller than scanned value,*/
   {
     datpoint->point = scanold->point;             /* Chain it in here.  */
     scanold->point = datpoint;                    /* Mark it chained in. */
     datum = STOPPER;
   };

   /* Step median pointer down chain after doing odd-numbered element */
   median = median->point;                       /* Step median pointer.  */
   if (scan == &large)
   {
     break;                                      /* Break at end of chain  */
   }
   scanold = scan;                               /* Save this pointer and   */
   scan = scan->point;                           /* step up chain */

   /* Handle even-numbered item in chain.  */
   if (scan->point == datpoint)
   {
     scan->point = successor;
   }

   if (scan->value > datum)
   {
     datpoint->point = scanold->point;
     scanold->point = datpoint;
     datum = STOPPER;
   }

   if (scan == &large)
   {
     break;
   }

   scanold = scan;
   scan = scan->point;
 }
 return median->value;
}
