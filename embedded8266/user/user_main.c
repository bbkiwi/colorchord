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

#define PORT 7777
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

extern volatile uint8_t sounddata[HPABUFFSIZE];
extern volatile uint16_t soundhead;
uint16_t soundtail;

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
	if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL >= gCOLORCHORD_SHIFT_INTERVAL ) gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0;
	//printf("MOD FRAME %d ******\n", gFRAMECOUNT_MOD_SHIFT_INTERVAL);	//uint8_t led_outs[NUM_LIN_LEDS*3];
	int i;
	HandleFrameInfo();

	switch( COLORCHORD_OUTPUT_DRIVER )
	{
	case 0:
		UpdateLinearLEDs();
		break;
	case 1:
		UpdateAllSameLEDs();
		break;
	case 2:
		UpdateRotatingLEDs();
		break;
	};

	//SendSPI2812( ledOut, NUM_LIN_LEDS );
#ifdef SUPRESS_ADC_WHEN_PUSH_LEDS
	//Adding Enter and Exit Critical with time delay
	//  makes ADC output center 1/2 way
	//  for vcc/2, starts stable when no websockets
	//  but becomes unstatble after a time
	//  unstable especially with large # LEDs
	EnterCritical();
#endif
	ws2812_push( ledOut, NUM_LIN_LEDS * 3 );
#ifdef SUPRESS_ADC_WHEN_PUSH_LEDS
	//ets_delay_us( 5000 ); // thought might supress noice but just 
	ExitCritical();
#endif
}

os_event_t    procTaskQueue[procTaskQueueLen];
uint32_t samp_iir = 0;
int wf = 0;

//Tasks that happen all the time.

static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{
	system_os_post(procTaskPrio, 0, 0 );

	if( COLORCHORD_ACTIVE && !hpa_running )
	{
printf("c\n"); 
		ExitCritical(); //continue hpatimer
		hpa_running = 1;
	}

	if( !COLORCHORD_ACTIVE && hpa_running )
	{
		EnterCritical(); //pause hpatimer
printf("p");
		hpa_running = 0;
	}
	
	//For profiling so we can see how much CPU is spent in this loop.
#ifdef PROFILE
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 1 );
#endif
	//Need to sample sound only when active, probable want to turn off leds when inactive
	if( COLORCHORD_ACTIVE && hpa_running )
	{
		while( soundtail != soundhead )
		{
			int16_t samp = sounddata[soundtail];
			samp_iir = samp_iir - (samp_iir>>10) + samp;
			PushSample32( (samp - (samp_iir>>10))*16 );
			soundtail = (soundtail+1)&(HPABUFFSIZE-1);

			wf++;
			if( wf == 128 )
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
	int wifiMode = wifi_get_opmode();

	uart0_sendStr("\r\nColorChord\r\n");

//Uncomment this to force a system restore.
//	system_restore();

	CustomStart();

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

	CSPreInit();

	pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
//TODO should this be COM_PORT instead of 7777?
	pUdpServer->proto.udp->local_port = 7777;
	espconn_regist_recvcb(pUdpServer, udpserver_recv);

	if( espconn_create( pUdpServer ) )
	{
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}

	CSInit();

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	//Timer example
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&some_timer, 100, 1);

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
void CriticalHelper(bool pause)
{	if (pause) {
		PauseHPATimer();
	} else {
		ContinueHPATimer();
	}
}

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