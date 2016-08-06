/*
 * libwebsockets-test-server - libwebsockets test implementation
 *
 * Copyright (C) 2010-2016 Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * The test apps are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 */

#include <string.h>

typedef enum {
	SCAN_STATE_NONE,
	SCAN_STATE_INITIAL,
	SCAN_STATE_LIST,
	SCAN_STATE_FINAL
} scan_state;

struct per_session_data__esplws_scan {
	struct per_session_data__esplws_scan *next;

	scan_state scan_state;
	struct bss_info *bss_info_next;
	unsigned char subsequent:1;
	unsigned char changed_partway:1;
};

struct per_vhost_data__esplws_scan {
	struct per_session_data__esplws_scan *live_pss_list;
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
	char count_live_pss;

	uv_timer_t timeout_watcher;
	struct bss_info *bss_info;
	unsigned char scan_ongoing:1;
	unsigned char completed_any_scan:1;
	unsigned char reboot:1;
};

/*
 * We have no choice... the ESP scan callback does not come with any user data
 * pointer passed back
 */
static struct per_vhost_data__esplws_scan *pvh_scan;

static void
scan_finished(struct per_vhost_data__esplws_scan *vhd)
{
	struct bss_info *bss_info = vhd->bss_info, *temp;

	while (bss_info) {
		temp = (struct bss_info *)bss_info->next.stqe_next;
		os_free(bss_info);
		bss_info = temp;
	}
	vhd->bss_info = NULL;
}

static void
scan_callback(void *arg, STATUS status);

static int
esplws_simple_arg(char *dest, int len, const char *in, const char *match)
{
	const char *p = strstr(in, match);
	int n = 0;

	if (!p) {
		lwsl_err("No match %s\n", match);
		return 1;
	}

	p += strlen(match);
	while (*p && *p != '\"' && n < len - 1)
		dest[n++] = *p++;
	dest[n] = '\0';

	return 0;
}

static void
uv_timeout_cb_scan(uv_timer_t *w)
{
	struct per_vhost_data__esplws_scan *vhd = lws_container_of(w,
			struct per_vhost_data__esplws_scan, timeout_watcher);

	if (vhd->reboot) {
		ets_wdt_enable();

		while (1)
			;
	}

	if (vhd->scan_ongoing)
		lwsl_err("%s: while ongoing\n", __func__);

	lwsl_err("starting ap scan\n");

	vhd->scan_ongoing = 1;
	wifi_station_scan(NULL, scan_callback);
}

static void
scan_callback(void *arg, STATUS status)
{
	struct bss_info *bss_info = arg, **next = &pvh_scan->bss_info;
	int n;

	pvh_scan->scan_ongoing = 0;

	lwsl_err("%s: %d\n", __func__, status);

	if (status == OK) {
		struct per_session_data__esplws_scan *pss = pvh_scan->live_pss_list;

		if (pvh_scan->bss_info)
			scan_finished(pvh_scan);

		pvh_scan->completed_any_scan = 1;

		while (bss_info) {
			if (strlen(bss_info->ssid) > 0) {
				*next = os_malloc(sizeof(*bss_info));
				if (!*next)
					return;
				memcpy(*next, bss_info, sizeof(*bss_info));
				(*next)->next.stqe_next = NULL;
				next = &((*next)->next.stqe_next);
			}
			bss_info = bss_info->next.stqe_next;
		}

		while (pss) {
			if (pss->scan_state == SCAN_STATE_NONE) {
				pss->subsequent = 0;
				pss->scan_state = SCAN_STATE_INITIAL;
			} else
				pss->changed_partway = 1;

			pss = pss->next;
		}

		lws_callback_on_writable_all_protocol(pvh_scan->context,
						      pvh_scan->protocol);
	} else
		uv_timer_start(&pvh_scan->timeout_watcher,
				uv_timeout_cb_scan, 100, 0);
}

/* lws-status protocol */

int
callback_esplws_scan(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len)
{
	struct per_session_data__esplws_scan *pss =
			(struct per_session_data__esplws_scan *)user,
			*pss1, *pss2;
	struct per_vhost_data__esplws_scan *vhd =
			(struct per_vhost_data__esplws_scan *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	char buf[LWS_PRE + 384], ip[24], *start = buf + LWS_PRE - 1, *p = start,
	     *end = buf + sizeof(buf) - 1;
	int n, m;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__esplws_scan));
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		pvh_scan = vhd;
		vhd->scan_ongoing = 0;
		uv_timer_init(lws_uv_getloop(vhd->context, 0),
			      &vhd->timeout_watcher);
		uv_timer_start(&pvh_scan->timeout_watcher,
			       uv_timeout_cb_scan, 1000, 0);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (!vhd)
			break;
		uv_timer_stop(&vhd->timeout_watcher);
		break;

	case LWS_CALLBACK_ESTABLISHED:
		vhd->count_live_pss++;
		pss->next = vhd->live_pss_list;
		vhd->live_pss_list = pss;
		/* if we have scan results, update them.  Otherwise wait */
		if (vhd->bss_info) {
			pss->scan_state = SCAN_STATE_INITIAL;
			lws_callback_on_writable(wsi);
		} else
			if (!vhd->completed_any_scan)
				uv_timer_start(&pvh_scan->timeout_watcher,
						uv_timeout_cb_scan, 10, 0);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		switch (pss->scan_state) {
		case SCAN_STATE_INITIAL:
			n = LWS_WRITE_TEXT | LWS_WRITE_NO_FIN;;
			p += snprintf(p, end - p,
				      "{ \"model\":\"%s\","
				      " \"serial\":\"%s\","
				      " \"host\":\"%s\","
				      " \"aps\":[",
				      MODEL_NAME,
				      esplws_settings.serial,
				      model_serial);
			pss->scan_state = SCAN_STATE_LIST;
			pss->bss_info_next = vhd->bss_info;
			break;
		case SCAN_STATE_LIST:
			n = LWS_WRITE_CONTINUATION | LWS_WRITE_NO_FIN;
			if (!pss->bss_info_next)
				goto scan_state_final;

			if (pss->subsequent)
				*p++ = ',';
			pss->subsequent = 1;
			p += snprintf(p, end - p,
				      "{\"ssid\":\"%s\","
				       "\"bssid\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
				       "\"rssi\":\"%d\","
				       "\"chan\":\"%d\","
				       "\"auth\":\"%d\"}",
					pss->bss_info_next->ssid,
					pss->bss_info_next->bssid[0],
					pss->bss_info_next->bssid[1],
					pss->bss_info_next->bssid[2],
					pss->bss_info_next->bssid[3],
					pss->bss_info_next->bssid[4],
					pss->bss_info_next->bssid[5],
					pss->bss_info_next->rssi,
					pss->bss_info_next->channel,
					pss->bss_info_next->authmode);
			pss->bss_info_next = pss->bss_info_next->next.stqe_next;
			if (!pss->bss_info_next)
				pss->scan_state = SCAN_STATE_FINAL;
			break;
		case SCAN_STATE_FINAL:
scan_state_final:
			n = LWS_WRITE_CONTINUATION;
			p += sprintf(p, "]}");
			if (pss->changed_partway) {
				pss->subsequent = 0;
				pss->bss_info_next = vhd->bss_info;
				pss->scan_state = SCAN_STATE_INITIAL;
			} else
				pss->scan_state = SCAN_STATE_NONE;
			break;
		default:
			return 0;
		}

		m = lws_write(wsi, (unsigned char *)start, p - start, n);
		if (m < 0) {
			lwsl_err("ERROR %d writing to di socket\n", m);
			return -1;
		}

		if (pss->scan_state != SCAN_STATE_NONE)
			lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_RECEIVE:
		if (strstr((const char *)in, "identify")) {
			lwsl_err("identify\n");
			break;
		}

		if (esplws_simple_arg(esplws_settings.station_config.ssid,
				      sizeof(esplws_settings.station_config.ssid),
				      in, "ssid\":\""))
			break;

		if (esplws_simple_arg(esplws_settings.station_config.password,
				      sizeof(esplws_settings.station_config.password),
				      in, ",\"pw\":\""))
			break;

		if (esplws_simple_arg(esplws_settings.serial,
				      sizeof(esplws_settings.serial),
				      in, ",\"serial\":\""))
			break;

		esplws_settings.station_config.bssid_set = 0;

		lwsl_err("Rx: ssid '%s', pw '%s', ser '%s'\n",
				esplws_settings.station_config.ssid,
				esplws_settings.station_config.password,
				esplws_settings.serial);

		esplws_settings.csum = 0;
		esplws_settings.csum = settings_csum();

		spi_flash_erase_sector(ESPLWS_SETTINGS_PAGE >> 12);
		spi_flash_write(ESPLWS_SETTINGS_PAGE, (uint32_t *)&esplws_settings, sizeof(esplws_settings));

		wifi_station_disconnect();
		wifi_set_opmode(STATION_MODE);
		wifi_station_set_config(&esplws_settings.station_config);
		wifi_station_connect();

		vhd->reboot = 1;
		uv_timer_start(&pvh_scan->timeout_watcher,
			       uv_timeout_cb_scan, 100, 0);

		break;

	case LWS_CALLBACK_CLOSED:
		vhd->count_live_pss--;
		break;
	default:
		break;
	}

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_ESPLWS_SCAN \
	{ \
		"esplws-scan", \
		callback_esplws_scan, \
		sizeof(struct per_session_data__esplws_scan), \
		512, \
	}

