//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "uart.h"
#include "osapi.h"
#include "espconn.h"
#include "esp82xxutil.h"
#include "ws2812_i2s.h"
#include "hpatimer.h"
#include "DFT32.h"
#include "ccconfig.h"
#include <embeddednf.h>
#include <embeddedout.h>
#include <commonservices.h> 
#include "ets_sys.h"
#include "gpio.h"
//#define PROFILE

#define SERVER_TIMEOUT 1500
#define MAX_CONNS 5
#define MAX_FRAME 2000

#define procTaskPrio        0
#define procTaskQueueLen    1

struct CCSettings CCS;
int gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0;
int gROTATIONSHIFT = 0; //Amount of spinning of pattern around a LED ring
static volatile os_timer_t some_timer;
static struct espconn *pUdpServer;

void EnterCritical();
void ExitCritical();

extern uint16_t median_filter(uint16_t datum);
extern volatile uint8_t sounddata[HPABUFFSIZE];
extern volatile uint16_t soundhead;
volatile uint16_t soundtail;

uint8_t sounddatacopy[HPABUFFSIZE];
static uint8_t hpa_running = 0;
static uint8_t hpa_started = 0;
static uint8_t hpa_is_paused_for_wifi = 0;
void ICACHE_FLASH_ATTR CustomStart( );

void ICACHE_FLASH_ATTR user_rf_pre_init()
{
}


//Call this once we've stacked together one full colorchord frame.
static void ICACHE_FLASH_ATTR NewFrame()
{
	if( !COLORCHORD_ACTIVE ) return;
	gFRAMECOUNT_MOD_SHIFT_INTERVAL++;
	if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL >= COLORCHORD_SHIFT_INTERVAL ) gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0;
	//printf("MOD FRAME %d ******\n", gFRAMECOUNT_MOD_SHIFT_INTERVAL);	//uint8_t led_outs[NUM_LIN_LEDS*3];
	int i;

	switch( COLORCHORD_OUTPUT_DRIVER )
	{
//	case 0:
//		UpdateLinearLEDs();
//		break;
//	case 1:
//		UpdateAllSameLEDs(); not needed, just have NERF_NOTE_PORP>50, Display brightness proportional to amp2
//		break;
//	case 2:
//		UpdateRotatingLEDs(); not needed, just have NERF_NOTE_PORP>50, Display brightness fixed and middle interval size prop to amp2, flip on 'bass'
//		break;
	case 254:
		PureRotatingLEDs();
		break;
	case 255:
		DFTInLights();
		break;
	default:
		UpdateLinearLEDs(); // have variety of display options and uses COLORCHORD_OUTPUT_DRIVER to select them
	};

	//SendSPI2812( ledOut, NUM_LIN_LEDS );
	//EnterCritical();
	//ets_delay_us( 1 );
	ws2812_push( ledOut, NUM_LIN_LEDS * 3 );
	//ets_delay_us( 1 );
	//ExitCritical();
}

os_event_t    procTaskQueue[procTaskQueueLen];
uint32_t samp_iir = 0;
int wf = 64; //this will stagger calls to NewFrame and HandleFrameInfo
int wh = 0;
int samplesPerFrame = 128;
int samplesPerHandleInfo = 1;
int32_t samp;
int32_t samp_centered;
int32_t samp_prior;
int32_t samp_adjustment;
int32_t samp_adjusted;
uint8_t glitch_count;
uint8_t glitch_count_max=GLITCH_COUNT_MAX;
uint8_t glitch_drop=GLITCH_DROP;

//Tasks that happen all the time.

static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{
	system_os_post(procTaskPrio, 0, 0 );

	if( COLORCHORD_ACTIVE && !hpa_running )
	{
//printf("c\n");
		ExitCritical(); //continue hpatimer
		hpa_running = 1;
	}

	if( !COLORCHORD_ACTIVE && hpa_running )
	{
		EnterCritical(); //pause hpatimer
//printf("p");
		hpa_running = 0;
	}
	
	//For profiling so we can see how much CPU is spent in this loop.
#ifdef PROFILE
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 1 );
#endif
	//Need to sample sound only when active, probable want to turn off leds when inactive
	if( COLORCHORD_ACTIVE && hpa_running )
	{
		switch( COLORCHORD_OUTPUT_DRIVER )
		{
		case 0:
		case 1:
		case 2:
		case 4:
			samplesPerFrame = 128; // <= but if < required new def of max to respond to peaks
			samplesPerHandleInfo = 128;
			break;
		case 3:
			samplesPerFrame = 32;
			samplesPerHandleInfo = 128;
			break;
		};

		while( soundtail != soundhead )
		{
// #if PROTECT_SOUNDDATA code
// cleans noise and shows vcc/2 when oscope open or gui not showing page
//    seems more sensitive to mic too.
//  if showing gui and oscope closed or off get noise!, if gpio open less noise
//  when oscope closed or off for NUM_LEDS 18 more noise than when 255
//  still very sharp spikes dropping to vcc/3. Previously noise was drops
//  to vcc/3 for longer periods so on oscope looked like square wave pulses
//  the delays below and changing menuinterface.js improve, interferance seems
//  to be occurring when system HZ in gui is too fast (400MHz) when oscope open it slows down.
//  BUT freq response is different when gui open: with C# is blue, without C# is red. Which is correct freq?
//      when gui is shut C# red both ways
//  Also less stable when running off lipo 3.7 v

#if PROTECT_SOUNDDATA
			EnterCritical();
			//ets_delay_us( 2);
#endif
			if (soundtail != soundhead) samp = sounddata[soundtail];
#if PROTECT_SOUNDDATA
			//ets_delay_us( 2 );
			ExitCritical();
#endif

			// from chlohr/colorchord master commit 1d86a1c52
			samp_iir = samp_iir - (samp_iir>>10) + samp; // tracks mean signal times 2^10
			samp_centered = samp - (samp_iir>>10); //samp centered about 0
/*
			// Attempt to ignore glitches which is a sudden drop at least GLITCH_DROP
			// Can get stuck if somehow get very large samp_prior, then samp_centered get quiet.
			// So have counter to prevent this
			if ((glitch_count > glitch_count_max) || ((samp_prior - samp_centered - glitch_drop) < 0))
			{
				samp_prior = samp_centered; // no glitch
				glitch_count = 0;
			} else {
				samp_centered = samp_prior; // still in glitch zone
				glitch_count++;
			}
*/
			// Alternative attempt to ignore glitches. Assume sudden drop  at least GLITCH_DROP
			//   means requires adding or subtracting the actual change. When jump of at least
			//   HALF this size the glitch ends.
			// Can get stuck if somehow get very large samp_prior, then samp_centered get quiet.
			// So have counter to prevent this

			if (samp_prior - samp_centered > glitch_drop) //drop happened
			{
				samp_adjustment += samp_prior - samp_centered;
				glitch_count = 0;
			} else if (samp_centered - samp_prior > glitch_drop/2) // jump happened
			{
				//samp_adjustment += samp_prior - samp_centered;
				samp_adjustment = 0;
			}
			samp_prior = samp_centered;
			glitch_count++;
			if (glitch_count > glitch_count_max) samp_adjustment = 0;

			samp_adjusted = samp_centered + samp_adjustment;



			samp_adjusted = (samp_adjusted * INITIAL_AMP /16); //amplified
			PushSample32( samp_adjusted * 16 );

//			sounddatacopy[soundtail] = median_filter(samp); //can't get to work
			//WARNING samp_centered + samp_iir>>10 compiles as (samp_centered + samp_iir)>>10
			int32_t samp_mean = (samp_iir>>10);
			int32_t samp_oscope = samp_adjusted + samp_mean;

			// clip for oscope
			if (samp_oscope < 0)
			{
				sounddatacopy[soundtail] = 0x0;
			}
			else if (samp_oscope >255)
			{
				sounddatacopy[soundtail] = 0xff;
			}
			else
			{
				sounddatacopy[soundtail] = samp_oscope;
			}

			soundtail = (soundtail+1)&(HPABUFFSIZE-1);

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
	}
#ifdef PROFILE
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 0 );
#endif

	if( events->sig == 0 && events->par == 0 )
	{
		CSTick( 0 );
	}

}

//Timer event.
static void ICACHE_FLASH_ATTR myTimer(void *arg)
{
	CSTick( 1 );

//	if( hpa_is_paused_for_wifi && (wifi_station_get_connect_status() == STATION_GOT_IP))
	if( hpa_is_paused_for_wifi && printed_ip ) // which implies it is connected to the station
	{
		StartHPATimer(); //Init the high speed  ADC timer.
		hpa_running = 1;
		hpa_is_paused_for_wifi = 0; // only need to do once prevents unstable ADC
		printf("start HPATimer for station\n");
	}
//	uart0_sendStr(".");
//	printf( "%d/%d\n",soundtail,soundhead );
//	printf( "%d/%d\n",soundtail,soundhead );
//	uint8_t ledout[] = { 0x00, 0xff, 0xaa, 0x00, 0xff, 0xaa, };
//	ws2812_push( ledout, 6 );
}


//Called when new packet comes in.
static void ICACHE_FLASH_ATTR udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;
//	uint8_t buffer[MAX_FRAME];
//	uint8_t ledout[] = { 0x00, 0xff, 0xaa, 0x00, 0xff, 0xaa, };
	uart0_sendStr("X");
	ws2812_push( pusrdata+3, len );
}

void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}


void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	struct rst_info *rtc_info = system_get_rst_info();
	printf("\nreset reason: %x\n", rtc_info->reason);
	if (rtc_info->reason==REASON_WDT_RST|| rtc_info->reason==REASON_EXCEPTION_RST||rtc_info->reason==REASON_SOFT_WDT_RST) {
		if (rtc_info->reason== REASON_EXCEPTION_RST) {
			printf("Fatal exception (%d):\n", rtc_info->exccause);
		}
	printf("epc1=0x%08x, epc2=0x%08x, epc3=0x%08x\n", rtc_info->epc1, rtc_info->epc2, rtc_info->epc3);
	printf("excvaddr=0x%08x, depc=0x%08x\n", rtc_info->excvaddr, rtc_info->depc);
	}

	wifi_get_opmode(); // if removed get resets

	uart0_sendStr("\r\nColorChord\r\n");

//Uncomment this to force a system restore.
//	system_restore();

	CustomStart(); //read saved info flash or set defaults

#ifdef PROFILE
	GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
#endif
//	TEST	starting HPATimer early assuming starting in AP mode
//	   then CSPreInit(); which calls Enter and Exit Critical will
//         pause a timer that is started.
//         seems if put other statements e.g. prints, if etc in EnterCritical
//         will make reset unless timer is already going
//	StartHPATimer(); //Init the high speed  ADC timer.
//	hpa_running = 1;

	CSPreInit(); //Commonservices pre Init

	pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer->proto.udp->local_port = COM_PORT;
	espconn_regist_recvcb(pUdpServer, udpserver_recv);

	if( espconn_create( pUdpServer ) )
	{
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}

	CSInit(); //Commonservices Init

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	//Timer example
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&some_timer, 100, 1); //100ms

	//Set GPIO16 for Input
	WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC and rtc_gpio0 connection

	WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable

	WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);	//out disable

	InitColorChord(); //Init colorchord

	//Tricky: If we are in station mode, wait for that to get resolved before enabling the high speed timer.
	if( wifi_get_opmode() == 1 )
	{
		hpa_is_paused_for_wifi = 1;
	}
	else
	{
		StartHPATimer(); //Init the high speed  ADC timer.
		hpa_running = 1;
		printf("start HPATimer for AP\n");
	}

	ws2812_init();
	printf("Colorchord Finish user_init\n");
	// Attempt to make ADC more stable
	// https://github.com/esp8266/Arduino/issues/2070
	// see peripherals https://espressif.com/en/support/explore/faq
	//wifi_set_sleep_type(NONE_SLEEP_T); // on its own stopped wifi working
	//wifi_fpm_set_sleep_type(NONE_SLEEP_T); // with this seemed no difference

	system_os_post(procTaskPrio, 0, 0 );
}

// Tried this wrapper and using in EnterCritical and ExitCritical
//    but doesn't work. Wanted to try having hpa_running static in
//    the helper funciton. 
//void CriticalHelper(bool pause)
//{	if (pause) {
//		PauseHPATimer();
//	} else {
//		ContinueHPATimer();
//	}
//}

// With TEST fiddle starting HPATimer early for AP mode could
//    put print and setting hpa_running = 0; but not if statments
//    couldn't add any more to ExitCritical though. ???
void EnterCritical()
{
//	CriticalHelper(1);
//	if (!hpa_running) return;
	PauseHPATimer();
//printf("e"); // if put this here loops
//	hpa_running = 0; // if put this here loops
	//ets_intr_lock();
}

void ExitCritical()
{
//	CriticalHelper(0);
	//ets_intr_unlock();
//	if (hpa_running) return;
//printf("x\n");
//	hpa_running = 1;
	ContinueHPATimer();
}
