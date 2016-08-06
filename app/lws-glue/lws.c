
#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "os_type.h"

#include "../lws/libwebsockets/lib/libwebsockets.h"

#include "user_interface.h"
#include "user_config.h"
#include "serial_number.h"

void *pvPortMalloc(size_t s, const char *f, int line);
static inline void *malloc(size_t s) { return pvPortMalloc(s, "", 0); }
static inline void *zalloc(size_t s) { void *v = pvPortMalloc(s, "", 0); if (v) memset(v, 0, s); return v; }
void *pvPortRealloc(void *p, size_t s, const char *f, int line);
static inline void *realloc(void *p, size_t s) { return pvPortRealloc(p, s, "", 0); }
void vPortFree(void *p, const char *f, int line);
static inline void free(void *p) { vPortFree(p, "", 0); }

int ets_snprintf(char *str, size_t size, const char *format, ...);
int ets_sprintf(char *str, const char *format, ...);
int os_printf_plus(const char *format, ...);

#define snprintf ets_snprintf
#define sprintf ets_sprintf

#define LWS_PLUGIN_STATIC

/* ap-mode protocols */

#include "plugins/protocol_esplws_scan.c"

/* station mode protocols */

#include "../lws/libwebsockets/plugins/protocol_dumb_increment.c"
#include "../lws/libwebsockets/plugins/protocol_lws_mirror.c"
#include "../lws/libwebsockets/plugins/protocol_post_demo.c"
#include "../lws/libwebsockets/plugins/protocol_lws_status.c"

#include <romfs.h>

static const __attribute__((section(".irom.text"))) unsigned char romfs[] = {
#include "../../romfs.img.h"
};

static struct lws_context *context;


extern int
lwsesp_is_booting_in_ap_mode(void);

static const struct lws_protocols protocols_station[] = {
	{
		"http-only",
		lws_callback_http_dummy,
		0,
		900,
	},
	LWS_PLUGIN_PROTOCOL_DUMB_INCREMENT,
	LWS_PLUGIN_PROTOCOL_MIRROR,
	LWS_PLUGIN_PROTOCOL_POST_DEMO,
	LWS_PLUGIN_PROTOCOL_LWS_STATUS,
	{ NULL, NULL, 0, 0 } /* terminator */
}, protocols_ap[] = {
	{
		"http-only",
		lws_callback_http_dummy,
		0,	/* per_session_data_size */
		900,
	},
	LWS_PLUGIN_PROTOCOL_ESPLWS_SCAN,
	{ NULL, NULL, 0, 0 } /* terminator */
};

struct esp8266_file {
	const struct inode *i;
	size_t ofs;
	size_t size;
};

static lws_filefd_type
esp8266_lws_fops_open(struct lws *wsi, const char *filename,
		      unsigned long *filelen, int flags)
{
	struct esp8266_file *f = malloc(sizeof(*f));
	
	if (!f)
		return NULL;
	
	f->i = romfs_lookup(romfs, romfs, filename);
	if (!f->i) {
		free(f);
		return NULL;
	}
	
	*filelen = romfs_inode_size(f->i);
	f->ofs = 0;
	f->size = *filelen;
	
	return f;
}
static int
esp8266_lws_fops_close(struct lws *wsi, lws_filefd_type fd)
{
	lwsl_notice("%s: %p: fclose\n", __func__, wsi);
	free((void *)fd);
}
static unsigned long
esp8266_lws_fops_seek_cur(struct lws *wsi, lws_filefd_type fd,
		     long offset_from_cur_pos)
{
	struct esp8266_file *f = fd;
	
	f->ofs += offset_from_cur_pos;
	
	if (f->ofs > f->size)
		f->ofs = f->size;
}
static int
esp8266_lws_fops_read(struct lws *wsi, lws_filefd_type fd,
		      unsigned long *amount, unsigned char *buf,
		      unsigned long len)
{
	struct esp8266_file *f = fd;
	size_t s;
	
	if ((long)buf & 3) {
		lwsl_err("misaligned buf\n");
		
		return -1;
	}

	s = romfs_read(f->i, f->ofs, (uint32_t *)buf, len);
	*amount = s;
	f->ofs += s;
	
	return 0;
}

/* mounts related to the normal station website */

static const struct lws_http_mount mount_station_post = {
	.mountpoint		= "/formtest",
	.origin			= "protocol-post-demo",
	.origin_protocol	= LWSMPRO_CALLBACK,
	.mountpoint_len		= 9,
};

static const struct lws_http_mount mount_station = {
        .mount_next		= &mount_station_post,
        .mountpoint		= "/",
        .origin			= "/station",
        .def			= "test.html",
        .origin_protocol	= LWSMPRO_FILE,
        .mountpoint_len		= 1,
};

/* mount related to the AP configuration website */

static const struct lws_http_mount mount_ap = {
        .mountpoint		= "/",
        .origin			= "/ap",
        .def			= "index.html",
        .origin_protocol	= LWSMPRO_FILE,
        .mountpoint_len		= 1,
};

void
init_lws(void)
{
	struct lws_context_creation_info info;
	
	lws_set_log_level(7, lwsl_emit_syslog);	

	os_memset(&info, 0, sizeof(info));

	info.port = 80;
	info.fd_limit_per_thread = 12;
	info.max_http_header_pool = 1;
	info.max_http_header_data = 512;
	info.pt_serv_buf_size = 1500;
	info.keepalive_timeout = 5;	
	//info.ws_ping_pong_interval = 30;
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;

	context = lws_create_context(&info);
	if (context == NULL)
		return;
	
	lws_get_fops(context)->open = esp8266_lws_fops_open;
	lws_get_fops(context)->close = esp8266_lws_fops_close;
	lws_get_fops(context)->read = esp8266_lws_fops_read;
	lws_get_fops(context)->seek_cur = esp8266_lws_fops_seek_cur;

	if (!lwsesp_is_booting_in_ap_mode()) {
		info.vhost_name = "station";
		info.protocols = protocols_station;
		info.mounts = &mount_station;
		if (!lws_create_vhost(context, &info))
			lwsl_err("Failed to create vhost station\n");
	} else {
		info.vhost_name = "ap";
		info.protocols = protocols_ap;
		info.mounts = &mount_ap;
		if (!lws_create_vhost(context, &info))
			lwsl_err("Failed to create vhost ap\n");
	}

	lws_protocol_init(context);
}
