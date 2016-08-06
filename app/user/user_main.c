/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/1/1, v1.0 create this file.
*******************************************************************************/
#include "user_interface.h"
#include "c_string.h"
#include "c_stdlib.h"
#include "c_stdio.h"

#include "osapi.h"
#include "../platform/platform.h"
#include "ets_sys.h"
#include "driver/uart.h"
#include "mem.h"

#include "gpio.h"

#include "dns.h"
#include "serial_number.h"

#include "user_config.h"

void ICACHE_FLASH_ATTR
init_lws(void);

#ifdef DEVELOP_VERSION
os_timer_t heapTimer;

static void
heapTimerCb(void *arg)
{
	NODE_DBG("FREE HEAP: %d\n", system_get_free_heap_size());
}
#endif

static os_timer_t timer_i2c;
char model_serial[20], esplws_settings_valid, esplws_force_ap_mode;
struct esplws_settings esplws_settings;

int
lwsesp_is_booting_in_ap_mode(void)
{
	return esplws_force_ap_mode;
}

void memcpy_aligned(uint32_t *dst32, const uint32_t *src32, int len)
{
	uint32_t l = len >> 2;
	uint8_t *b;

	while (l--)
		*dst32++ = *src32++;
	
	if (!len & 3)
		return;
	
	l = *src32;
	b = (uint8_t *)dst32;
	while (len-- & 3) {
		*b++ = l;
		l >>= 8;
	}
}

unsigned char
settings_csum(void)
{
	unsigned char m = 0, *p = (unsigned char *)&esplws_settings;
	int n;

	for (n = 0; n < sizeof(esplws_settings); n++)
		m += *p++;

	return m;
}

static const char hex[] = "0123456789ABCDEF";

static void b(char *p, unsigned char c)
{
	*p++ = hex[c >> 4];
	*p = hex[c & 0xf];
}

static void
config_wifi(void)
{
	struct station_config null_station_config;
	struct softap_config config;
	unsigned int n, m = STATION_MODE;
	char hostname[30];
	
	platform_key_led(0);

	n = system_get_chip_id();
	if (!esplws_settings.serial[0])
		n = ets_sprintf(model_serial, MODEL_NAME"-%02X%02X%02X",
			(n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff);
	else
		n = ets_sprintf(model_serial, MODEL_NAME"-%s",
				esplws_settings.serial);

	NODE_DBG("Model/Serial: %s\n", model_serial);

	if (lwsesp_is_booting_in_ap_mode())
		m = STATIONAP_MODE; /* AP-only mode cannot succeed to scan */
	else {
		wifi_station_set_config(&esplws_settings.station_config);
		wifi_station_set_hostname(model_serial);
		wifi_station_set_auto_connect(1);
	}

	wifi_set_opmode(m);

	if (m == STATION_MODE)
		return;

	/* configure the AP */

	wifi_softap_get_config(&config);

	config.ssid_len = ets_snprintf(config.ssid, sizeof(config.ssid), "%s",
				       model_serial);
	ets_memset(config.password, 0, sizeof(config.password));
	config.channel = 11;
	config.authmode = AUTH_OPEN;
	config.max_connection = 5;
	config.ssid_hidden = 0;
	
	/* make the station side unable to do anything while in AP mode */
	ets_memset(null_station_config, 0, sizeof(null_station_config));
	wifi_station_set_config(&null_station_config);
	wifi_station_set_auto_connect(0);

	wifi_softap_set_config(&config);
}

#include <xtensa/corebits.h>

struct XTensa_exception_frame_s {
	uint32_t pc;
	uint32_t ps;
	uint32_t sar;
	uint32_t vpri;
	uint32_t a0;
	uint32_t a[14]; //a2..a15
//These are added manually by the exception code; the HAL doesn't set these on an exception.
	uint32_t litbase;
	uint32_t sr176;
	uint32_t sr208;
	uint32_t a1;
	 //'reason' is abused for both the debug and the exception vector: if bit 7 is set,
	//this contains an exception reason, otherwise it contains a debug vector bitmap.
	uint32_t reason;
};

static void xputc(char c)
{
	while (((READ_PERI_REG(UART_STATUS(0))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT)>=126) ;
		WRITE_PERI_REG(UART_FIFO(0), c);
}

static void
gdb_exception_handler(struct XTensa_exception_frame_s *frame)
{
	volatile int n = 0;
	static char *hex = "0123456789ABCDEF";
	
	ets_wdt_disable();
	
	xputc('\n');
	xputc('P');
	xputc('C');
	xputc('=');

	for (n = 0; n < 8; n++) {
		xputc(hex[frame->pc >> 28]);
		frame->pc <<= 4;
	}
	
	xputc('\n');
	xputc('*');
	
	while (n++ < 1000000)
		;
	
	ets_wdt_enable();
	
	while (1)
		;
}

ICACHE_FLASH_ATTR void exception_init() {
  char causes[] = {EXCCAUSE_ILLEGAL, EXCCAUSE_INSTR_ERROR,
    EXCCAUSE_LOAD_STORE_ERROR, EXCCAUSE_DIVIDE_BY_ZERO,
    EXCCAUSE_UNALIGNED, EXCCAUSE_INSTR_PROHIBITED,
    EXCCAUSE_LOAD_PROHIBITED, EXCCAUSE_STORE_PROHIBITED};
  int i;
  for (i = 0; i < (int) sizeof(causes); i++) {
    _xtos_set_exception_handler(causes[i], gdb_exception_handler);
  }
}

void
user_init(void)
{
	int n, m = 0, forc;

	system_update_cpu_freq(160);
	
	exception_init();
	
	gpio_init();

	gpio16_output_set(1);
	gpio16_output_conf();
	gpio16_input_conf();

//	gpio_pin_intr_state_set(GPIO_ID_PIN0 + 3, GPIO_PIN_INTR_DISABLE);

//	PIN_PULLDWN_DIS(PERIPHS_IO_MUX_U0RXD_U);
//	PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);
//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
	esplws_force_ap_mode = !gpio16_input_get();
//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);

	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	memcpy_aligned((uint32_t *)&esplws_settings,
		       (uint32_t *)(0x40200000 | ESPLWS_SETTINGS_PAGE),
		       sizeof(esplws_settings));

	esplws_settings_valid =
		(unsigned char)(settings_csum() - esplws_settings.csum) ==
		esplws_settings.csum;

	if (esplws_force_ap_mode) {
		NODE_DBG("AP Mode Forced by user\n");
	}

	if (esplws_settings_valid) {
		NODE_DBG("NV: AP '%s'\n", esplws_settings.station_config.ssid);
	} else {
		NODE_DBG("NV: invalid, forcing AP mode\n");
		memset(&esplws_settings, 0, sizeof(esplws_settings));
		esplws_force_ap_mode = 1;
	}

	config_wifi();
	init_dns();
	platform_init();
	init_lws();

#ifdef DEVELOP_VERSION
	os_timer_setfn(&heapTimer, (os_timer_func_t *)heapTimerCb, NULL);
	os_timer_arm(&heapTimer, 5000, 1);
#endif
}

