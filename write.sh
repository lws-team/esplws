#!/bin/sh

RAM_LIMIT=36864

S=`stat bin/0x00000.bin -c %s`
if [ $S -gt $RAM_LIMIT ] ; then
  echo RAM area blew out by $(( $S - $RAM_LIMIT ))
  exit 1
else
	echo $(( $RAM_LIMIT - $S )) ram area free
fi

esptool.py --port /dev/ttyUSB0 --baud 460800 write_flash --verify -fs 8m \
 0x00000 bin/0x00000.bin \
 0x10000 bin/0x10000.bin \
 0xfe000 bin/blank.bin \
 0xfc000 bin/esp_init_data_default.bin 
