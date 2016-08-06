#ifndef __SERIAL_NUMBER_H
#define __SERIAL_NUMBER_H

#define SERIAL_NUMBER "865732"

struct esplws_settings {
	struct station_config station_config;
	char serial[10];
	unsigned char csum;
};

#define ESPLWS_SETTINGS_PAGE 0xFB000

extern struct esplws_settings esplws_settings;
extern char model_serial[];

unsigned char
settings_csum(void);

#endif
