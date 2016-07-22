#include "c_types.h"
#include "user_interface.h"
#include "user_config.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"

static struct espconn dnsConn;
static esp_udp dnsUdp;

static const char * const domain_list[] = {
	INTERFACE_DOMAIN, /* this is our apparent server dns name */

	//captive portal domains
	"google.com",//android , yes I can be Google too    
	"gsp1.apple.com", //iphone
	"akamaitechnologies.com",
	"apple.com",
	"appleiphonecell.com",
	"itools.info",
	"ibook.info",
	"airport.us",
	"thinkdifferent.us",
	"apple.com.edgekey.net",
	"akamaiedge.net",
	"msftncsi.com", //for windows and windows phone,
	"microsoft.com",
};

static int ICACHE_FLASH_ATTR
dns_known(char *dns)
{
	int n;

	for (n = 0; n < sizeof(domain_list) / sizeof(domain_list[0]); n++)
		if (strstr(dns, domain_list[n]))
			return 1;

	return 0;
}

static void ICACHE_FLASH_ATTR
dns_process_query(void *arg, char *data, unsigned short length)
{
	char domain[72 + 12 + 16], *pos = domain;
	struct espconn *conn = arg;
	int ofs = 12, len = data[ofs];

	memset(domain, 0, sizeof(domain));

	while (len && pos < sizeof(domain) - 1 && ofs++ < length) {
		memcpy(pos, data + ofs, len);
		pos += len;
		ofs += len;
		len = data[ofs];

		if (len)
			*pos++ = '.';
	}

	NODE_DBG("%s: %s", __func__, domain);
	if (!dns_known(domain))
		return;

	/* can we actually assemble the reply with our buffer? */
	if (length >= sizeof(domain) - 12 - 16)
		return;
	
	pos = domain;
	
	*pos++ = data[0];
	*pos++ = data[1];
	*pos++ = 0x84 | (data[2] & 1);
	*pos++ = 0;
	*pos++ = data[4];
	*pos++ = data[5];
	*pos++ = data[4];
	*pos++ = data[5];
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;
	
	memcpy(pos, data + 12, length - 12);
	pos += length - 12;

	/* Point to the domain name in the question section */
	*pos++ = 0xC0;
	*pos++ = 12;

	/* Set the type to "Host Address" */
	*pos++ = 0;
	*pos++ = 1;

	/* Set the response class to IN */
	*pos++ = 0;
	*pos++ = 1;

	/* TTL in seconds, 0 means no caching */
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;

	/* RDATA length */
	*pos++ = 0;
	*pos++ = 4;

	/* RDATA payload is the captive ipv4 address */
	*pos++ = 192;
	*pos++ = 168;
	*pos++ = 4;
	*pos++ = 1;

	espconn_sendto(conn, (uint8_t *)domain, pos - domain);
}

void ICACHE_FLASH_ATTR
init_dns(void)
{
	uint8_t mode = 1;
	int res;

	wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &mode);
	wifi_set_broadcast_if(3);
	//espconn_disconnect(&dnsConn);
	espconn_delete(&dnsConn);

	dnsConn.type = ESPCONN_UDP;
	dnsConn.state = ESPCONN_NONE;
	dnsUdp.local_port = 53;
	dnsConn.proto.udp = &dnsUdp;

	espconn_regist_recvcb(&dnsConn, dns_process_query);

	res = espconn_create(&dnsConn);
	NODE_DBG("%s: conn=%p , status=%d", __func__, &dnsConn, res);
}
