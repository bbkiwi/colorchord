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
int gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0; //bb
int gROTATIONSHIFT = 0; //bb Amount of spinning of pattern around a LED ring
static os_timer_t some_timer; //bb was static volatile
static struct espconn *pUdpServer;
void EnterCritical();
void ExitCritical();

extern uint16_t median_filter(uint16_t datum); //bb has bugs
extern volatile uint8_t sounddata[HPABUFFSIZE];
extern volatile uint16_t soundhead;
uint16_t soundtail; //bb was volatile

uint8_t sounddatacopy[HPABUFFSIZE]; //bb
static uint8_t hpa_running = 0;
//static uint8_t hpa_started = 0; //bb
//static uint8_t hpa_is_paused_for_wifi = 0; //bb
static uint8_t hpa_is_paused_for_wifi;
void ICACHE_FLASH_ATTR CustomStart( );

//void ICACHE_FLASH_ATTR user_rf_pre_init() //bb
//{
//}


//Call this once we've stacked together one full colorchord frame.
//static void ICACHE_FLASH_ATTR NewFrame() //bb 
static void NewFrame()
{
	if( !COLORCHORD_ACTIVE ) return;
	gFRAMECOUNT_MOD_SHIFT_INTERVAL++; //bb
	if ( gFRAMECOUNT_MOD_SHIFT_INTERVAL >= COLORCHORD_SHIFT_INTERVAL ) gFRAMECOUNT_MOD_SHIFT_INTERVAL = 0; //bb
	//printf("MOD FRAME %d ******\n", gFRAMECOUNT_MOD_SHIFT_INTERVAL); //bb

	int i;
	HandleFrameInfo();

	switch( COLORCHORD_OUTPUT_DRIVER ) //bb all cases
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

	ws2812_push( ledOut, NUM_LIN_LEDS * 3, LED_DRIVER_MODE );
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


void fpm_wakup_cb_func1(void) { wifi_fpm_close(); }


//Tasks that happen all the time.

//static void ICACHE_FLASH_ATTR procTask(os_event_t *events) //bb
static void procTask(os_event_t *events)
{
	system_os_post(procTaskPrio, 0, 0 );

	if( COLORCHORD_ACTIVE && !hpa_running )
	{
		ExitCritical(); //bb continue hpatimer
		hpa_running = 1;
	}

	if( !COLORCHORD_ACTIVE && hpa_running )
	{
		EnterCritical(); //bb pause hpatimer
		hpa_running = 0;
	}
	
	//For profiling so we can see how much CPU is spent in this loop.
#ifdef PROFILE
	WRITE_PERI_REG( PERIPHS_GPIO_BASEADDR + GPIO_ID_PIN(0), 1 );
#endif
/* All from charles	
	while( soundtail != soundhead )
	{
		int32_t samp = sounddata[soundtail];
		samp_iir = samp_iir - (samp_iir>>10) + samp;
		samp = (samp - (samp_iir>>10))*16;
		samp = (samp * CCS.gINITIAL_AMP) >> 4;
		PushSample32( samp );
		soundtail = (soundtail+1)&(HPABUFFSIZE-1);

		wf++;
		if( wf == 128 )
		{
			NewFrame();
			wf = 0; 
		}
	}
*/
//This from bb
//Need to sample sound only when active, probable want to turn off leds when inactive
	if( COLORCHORD_ACTIVE && hpa_running )
	{
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

			if (samp_prior - samp_centered >= glitch_drop) //drop happened
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
			if (glitch_count >= glitch_count_max) samp_adjustment = 0;

			samp_adjusted = samp_centered + samp_adjustment;

			//amplifing won't loose significant bits but best signal should have come from
			//full 8 bit sounddata. If sound was only in range 0-64 it only has 6 significant
			// bit accuracy, INITIAL_AMP of 32 will make full range, but still steppy data
			samp_adjusted = (samp_adjusted * INITIAL_AMP)>>3;
			// amplifying by 2^A is equivalent to dropping RMUXSHIFTSTART in cconfig.h by A


//			sounddatacopy[soundtail] = median_filter(samp); //can't get to work
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

			if ( COLORCHORD_OUTPUT_DRIVER == 253) return;

			PushSample32(samp_adjusted);
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

	if( GPIO_INPUT_GET( 0 ) == 0 )
	{
		if( system_get_rst_info()->reason != 5 ) //See if we're booting from deep sleep.  (5 = deep sleep, 4 = reboot, 0 = fresh boot)
		{
			system_deep_sleep_set_option(4);	//Option 4 = When rebooting from deep sleep, totally disable wifi.
			system_deep_sleep(2000);
		}
		else
		{
			//system_deep_sleep_set_option(1);	//Option 1 = Reboot with radio recalibration and wifi on.
			//system_deep_sleep(2000);
			system_restart();
		}
	}

	if( hpa_is_paused_for_wifi && printed_ip ) //bb // which implies it is connected to the station
	{
		StartHPATimer(); //Init the high speed  ADC timer.
		hpa_running = 1;
		hpa_is_paused_for_wifi = 0; // only need to do once prevents unstable ADC
		printf("start HPATimer for station\n"); //bb
	}
//	uart0_sendStr(".");
//	printf( "%d/%d\n",soundtail,soundhead );
//	printf( "%d/%d\n",soundtail,soundhead );
//	uint8_t ledout[] = { 0x00, 0xff, 0xaa, 0x00, 0xff, 0xaa, };
//	ws2812_push( ledout, 6, LED_DRIVER_MODE );
}


//Called when new packet comes in.
static void ICACHE_FLASH_ATTR udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;
//	uint8_t buffer[MAX_FRAME];
//	uint8_t ledout[] = { 0x00, 0xff, 0xaa, 0x00, 0xff, 0xaa, };
	uart0_sendStr("X");
	ws2812_push( pusrdata+3, len, LED_DRIVER_MODE );
}

void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
//bb from here to
	struct rst_info *rtc_info = system_get_rst_info();
	printf("\nreset reason: %x\n", rtc_info->reason);
	if (rtc_info->reason==REASON_WDT_RST|| rtc_info->reason==REASON_EXCEPTION_RST||rtc_info->reason==REASON_SOFT_WDT_RST) {
		if (rtc_info->reason== REASON_EXCEPTION_RST) {
			printf("Fatal exception (%d):\n", rtc_info->exccause);
		}
	printf("epc1=0x%08x, epc2=0x%08x, epc3=0x%08x\n", rtc_info->epc1, rtc_info->epc2, rtc_info->epc3);
	printf("excvaddr=0x%08x, depc=0x%08x\n", rtc_info->excvaddr, rtc_info->depc);
	}
//bb here

	int wifiMode = wifi_get_opmode(); //bb if removed get resets

	uart0_sendStr("\r\nColorChord\r\n");

//Uncomment this to force a system restore.
//	system_restore();

	CustomStart(); //bb read saved info flash or set defaults


#ifdef PROFILE
	GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
#endif

	//Tricky: New recommendation is to connect GPIO14 to vcc for audio circuitry, so we turn this on by default.
	GPIO_OUTPUT_SET( GPIO_ID_PIN(14), 1);
	PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTMS_U, 3 );

//bb	TEST	starting HPATimer early assuming starting in AP mode
//bb	   then CSPreInit(); which calls Enter and Exit Critical will
//bb         pause a timer that is started.
// bb        seems if put other statements e.g. prints, if etc in EnterCritical
//bb         will make reset unless timer is already going
//bb	StartHPATimer(); //Init the high speed  ADC timer.
//bb	hpa_running = 1;


	CSPreInit();

	pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer->proto.udp->local_port = COM_PORT; //bb orig was 7777;
	espconn_regist_recvcb(pUdpServer, udpserver_recv);

	if( espconn_create( pUdpServer ) )
	{
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}

	CSInit( 1 );
	//CSInit(); //bb Commonservices Init

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	//Timer example
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&some_timer, 100, 1); //100ms

	//Set GPIO16 for Input
#if 0
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF,
                   (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC and rtc_gpio0 connection

    WRITE_PERI_REG(RTC_GPIO_CONF,
                   (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable

    WRITE_PERI_REG(RTC_GPIO_ENABLE,
                   READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);	//out disable
#endif

#if CPU_FREQ == 160
	system_update_cpu_freq(SYS_CPU_160MHZ);
#endif
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
		printf("start HPATimer for AP\n"); //bb
	}

	ws2812_init();
	printf("Colorchord Finish user_init\n"); //bb

	printf( "RST REASON: %d\n", system_get_rst_info()->reason );

	// Attempt to make ADC more stable
	// https://github.com/esp8266/Arduino/issues/2070
	// see peripherals https://espressif.com/en/support/explore/faq
	//bb wifi_set_sleep_type(NONE_SLEEP_T); // on its own stopped wifi working	
	wifi_set_sleep_type(MODEM_SLEEP_T); //bb in charles
	//wifi_fpm_set_sleep_type(NONE_SLEEP_T); // with this seemed no difference

	system_os_post(procTaskPrio, 0, 0 );
}

void ICACHE_FLASH_ATTR EnterCritical()
{
	PauseHPATimer();
	//ets_intr_lock();
}

void ICACHE_FLASH_ATTR ExitCritical()
{
	//ets_intr_unlock();
	ContinueHPATimer();
}





/*==============================================================================
 * Partition Map Data
 *============================================================================*/

#define SYSTEM_PARTITION_OTA_SIZE_OPT2                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR_OPT2               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT2              0xfb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT2            0xfc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT2    0xfd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT2 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT2                        2

#define SYSTEM_PARTITION_OTA_SIZE_OPT3                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR_OPT3               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT3              0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT3            0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT3    0x1fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT3 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT3                        3

#define SYSTEM_PARTITION_OTA_SIZE_OPT4                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR_OPT4               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT4              0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT4            0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT4    0x3fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT4 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT4                        4

#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM SYSTEM_PARTITION_CUSTOMER_BEGIN
#define EAGLE_FLASH_BIN_ADDR                 SYSTEM_PARTITION_CUSTOMER_BEGIN + 1
#define EAGLE_IROM0TEXT_BIN_ADDR             SYSTEM_PARTITION_CUSTOMER_BEGIN + 2

static const partition_item_t partition_table_opt2[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0x60000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT2,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT2,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT2, 0x3000},
};

static const partition_item_t partition_table_opt3[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0x60000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT3,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT3,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT3, 0x3000},
};

static const partition_item_t partition_table_opt4[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0x60000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT4,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT4,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT4, 0x3000},
};



/**
 * This is called on boot for versions ESP8266_NONOS_SDK_v1.5.2 to
 * ESP8266_NONOS_SDK_v2.2.1. system_phy_set_rfoption() may be called here
 */
void user_rf_pre_init(void)
{
	; // nothing
}

/**
 * Required function as of ESP8266_NONOS_SDK_v3.0.0. Must call
 * system_partition_table_regist(). This tries to register a few different
 * partition maps. The ESP should be happy with one of them.
 */
void ICACHE_FLASH_ATTR user_pre_init(void)
{
    if(system_partition_table_regist(
                partition_table_opt2,
                sizeof(partition_table_opt2) / sizeof(partition_table_opt2[0]),
                SPI_FLASH_SIZE_MAP_OPT2))
    {
        os_printf("system_partition_table_regist 2 success!!\r\n");
    }
    else if(system_partition_table_regist(
                partition_table_opt4,
                sizeof(partition_table_opt4) / sizeof(partition_table_opt4[0]),
                SPI_FLASH_SIZE_MAP_OPT4))
    {
        os_printf("system_partition_table_regist 4 success!!\r\n");
    }
    else if(system_partition_table_regist(
                partition_table_opt3,
                sizeof(partition_table_opt3) / sizeof(partition_table_opt3[0]),
                SPI_FLASH_SIZE_MAP_OPT3))
    {
        os_printf("system_partition_table_regist 3 success!!\r\n");
    }
    else
    {
        os_printf("system_partition_table_regist all fail\r\n");
        while(1);
    }
}

