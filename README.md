esplws
------

This is a WIP No-OS Espressif SDK build including libwebsockets (https://libwebsockets.org)


Prerequisites
-------------

You need an ESP07 / 12 type module with >=1MB, and a usb serial dongle of some kind to flash it.


Features
--------

 - If not configured, or you GND GPIO16, will start up in AP mode with a captive portal allowing configuration of the station side

 - If already configured, starts up joining the configured ap and serving the generic lws test apps in http + ws (SSL not supported yet)

 - offers a liberal subset the usual libwebsockets features consistent with the platform restrictions

 - If your AP DHCP server provides it, can be accessed using hostname ( http://lwstest-12345 )

 - Stores AP setup data persistently in Flash

 - Serving using vhosts and mounts

 - ROMFS in flash holds mount data with directory structure


Building the image
------------------

1) Toolchain

Clone this

 `$ git clone https://github.com/pfalcon/esp-open-sdk`

and follow the build instructions. For Fedora, as mentioned in https://github.com/pfalcon/esp-open-sdk/pull/56/commits/464e275e6a18ef31a8381839d87abdd69f4878b4 the following packages will be needed.

sudo dnf install make unrar autoconf automake libtool gcc gcc-c++ gperf \
   flex bison texinfo gawk ncurses-devel expat-devel python sed \
   help2man python-devel pyserial genromfs

(Notice you will need genromfs package later)

I chose the "include the SDK" build option and after some minutes it completed the build OK on Fedora24.

Set PATH to point to wherever you cloned the Toolchain

export PATH=/projects/esp-open-sdk/xtensa-lx106-elf/bin/:$PATH

That's it you are ready to build Tensilica stuff.

2) ESPLWS

Clone esplws (this project)

 `$ git clone https://github.com/warmcat/esplws`

If you don't already have a copy of lws in ./apps/lws/libwebsockets , then

 `$ cd esplws/app/lws ; git clone https://github.com/warmcat/libwebsockets`

Come back up to the esplws directory and do

`$ make html all ; ./write.sh`

After building he should flash it, this is it getting flashed into my 1MB ESP-07.

(To make the chip enter the flashing ROM bootloader, you must reset or power up with GPIO0 forced to 0V.)

```
1392 ram area free
esptool.py v1.2-dev
Connecting...
Running Cesanta flasher stub...
Flash params set to 0x0020
Writing 36864 @ 0x0... 36864 (100 %)
Wrote 36864 bytes at 0x0 in 3.2 seconds (92.2 kbit/s)...
Writing 548864 @ 0x10000... 548864 (100 %)
Wrote 548864 bytes at 0x10000 in 47.5 seconds (92.4 kbit/s)...
Writing 4096 @ 0xfe000... 4096 (100 %)
Wrote 4096 bytes at 0xfe000 in 0.4 seconds (90.3 kbit/s)...
Writing 4096 @ 0xfc000... 4096 (100 %)
Wrote 4096 bytes at 0xfc000 in 0.4 seconds (90.5 kbit/s)...
Leaving...
Verifying just-written flash...
Verifying 0x8a90 (35472) bytes @ 0x00000000 in flash against bin/0x00000.bin...
-- verify OK (digest matched)
Verifying 0x85939 (547129) bytes @ 0x00010000 in flash against bin/0x10000.bin...
-- verify OK (digest matched)
Verifying 0x1000 (4096) bytes @ 0x000fe000 in flash against bin/blank.bin...
-- verify OK (digest matched)
Verifying 0x80 (128) bytes @ 0x000fc000 in flash against bin/esp_init_data_default.bin...
-- verify OK (digest matched)
```

How to customize
----------------

### remove files from the romfs

Remove test-app related assets from `./apps/lws/romfs-files/station/*` ... the large file transfer test leaf.jpg alone is 375KB so this makes a lot of room.

### remove test protocols from configuration

Edit ./apps/lws-glue/lws.c and remove the inclusion of the test protocol code

```
 /* station mode protocols */
 
-#include "../lws/libwebsockets/plugins/protocol_dumb_increment.c"
-#include "../lws/libwebsockets/plugins/protocol_lws_mirror.c"
-#include "../lws/libwebsockets/plugins/protocol_post_demo.c"
-#include "../lws/libwebsockets/plugins/protocol_lws_status.c"
```

Replace them with your own protocol(s) later.

and remove the test protocols themselves from being added to the station vhost protocols array

```
-       LWS_PLUGIN_PROTOCOL_DUMB_INCREMENT,
-       LWS_PLUGIN_PROTOCOL_MIRROR,
-       LWS_PLUGIN_PROTOCOL_POST_DEMO,
-       LWS_PLUGIN_PROTOCOL_LWS_STATUS,
```

Replace those with your own protocol strings created the same way later.

remove the extra test app mount

```
-static const struct lws_http_mount mount_station_post = {
-       .mountpoint             = "/formtest",
-       .origin                 = "protocol-post-demo",
-       .origin_protocol        = LWSMPRO_CALLBACK,
-       .mountpoint_len         = 9,
-};
-
 static const struct lws_http_mount mount_station = {
-        .mount_next            = &mount_station_post,
+        .mount_next            = NULL,
```

change the default file to "index.html"

```
-        .def                   = "test.html",
+        .def                   = "index.html",
```

### Create your own assets

Create your own `./apps/lws/romfs-files/station/index.html` and add your own images etc in the same directory.


Derivation
----------

The non-lws ESP8266 bits came from esp-ginx, you can get the original pieces here, but at the time of writing they don't seem to be being maintained.  Lws replaces most of their work (http / ws related) and that is deleted in this tree.  Actually what's left is mainly the Espressif SDK.

https://github.com/israellot/esp-ginx

That project in turn seems to have gotten pieces from the below projects they mentioned -->

This project was inspired esp-httpd by Sprite_tm, NodeMcu ( https://github.com/nodemcu/nodemcu-firmware ) and NGINX

The whole thing seems MIT or "Beer License", lws bits are LGPLv2 with Static Linking Exception
