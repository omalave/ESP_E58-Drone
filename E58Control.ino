
/*
  Very basic code to control the Eachine E58 drone
*/

#if LWIP_FEATURES && !LWIP_IPV6

#ifndef STASSID
#define STASSID "WiFi-720P-2CDC1C"
#define STAPSK  ""
#define APSSID "E58drone"
#endif

#define ControlPort 50000

#include <ESP8266WiFi.h>
#include <lwip/napt.h>
#include <lwip/dns.h>
#include <lwip/netif.h>
#include <netif/etharp.h>
#include <lwip/udp.h>
#include <dhcpserver.h>

#define NAPT 1000
#define NAPT_PORT 10

PACK_STRUCT_BEGIN
struct tcp_hdr {
  PACK_STRUCT_FIELD(u16_t src);
  PACK_STRUCT_FIELD(u16_t dest);
  PACK_STRUCT_FIELD(u32_t seqno);
  PACK_STRUCT_FIELD(u32_t ackno);
  PACK_STRUCT_FIELD(u16_t _hdrlen_rsvd_flags);
  PACK_STRUCT_FIELD(u16_t wnd);
  PACK_STRUCT_FIELD(u16_t chksum);
  PACK_STRUCT_FIELD(u16_t urgp);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

static netif_input_fn orig_input_drone;
static netif_linkoutput_fn orig_output_drone;

bool check_packet_in(struct pbuf *p) {
struct eth_hdr *mac_h;
struct ip_hdr *ip_h;
struct udp_hdr *udp_h;
struct tcp_hdr *tcp_h;
char *payload;
/*
  if (p->len < sizeof(struct eth_hdr))
    return false;

  mac_h = (struct eth_hdr *)p->payload;

  // Check only IPv4 traffic
  if (ntohs(mac_h->type) != ETHTYPE_IP)
    return true;

  if (p->len < sizeof(struct eth_hdr)+sizeof(struct ip_hdr))
    return false;

  ip_h = (struct ip_hdr *)(p->payload + sizeof(struct eth_hdr));


  if (IPH_PROTO(ip_h) == IP_PROTO_UDP) {
    if (p->len < sizeof(struct eth_hdr)+sizeof(struct ip_hdr)+sizeof(struct udp_hdr))
      return false;

    udp_h = (struct udp_hdr *)(p->payload + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));
    payload = (char*)(p->payload + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr));

    if (ntohs(udp_h->dest) == ControlPort) {
      Serial.printf("X %+3d Y %+3d Z %+3d R %+3d C %x\n", payload[1], payload[2], payload[3], payload[4], payload[5]); 
    }
  }

  if (IPH_PROTO(ip_h) == IP_PROTO_TCP) {
    if (p->len < sizeof(struct eth_hdr)+sizeof(struct ip_hdr)+sizeof(struct tcp_hdr))
      return false;

    tcp_h = (struct tcp_hdr *)(p->payload + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));
  }
*/
  return true;
}

err_t my_input_drone (struct pbuf *p, struct netif *inp) {

  if (check_packet_in(p)) {
    return orig_input_drone(p, inp);
  } else {
    pbuf_free(p);
    return ERR_OK;
  }
}

bool check_packet_out(struct pbuf *p) {
struct eth_hdr *mac_h;
struct ip_hdr *ip_h;
struct udp_hdr *udp_h;
struct tcp_hdr *tcp_h;
char *payload;

  if (p->len < sizeof(struct eth_hdr))
    return false;

  mac_h = (struct eth_hdr *)p->payload;

  // Check only IPv4 traffic
  if (ntohs(mac_h->type) != ETHTYPE_IP)
    return true;

  if (p->len < sizeof(struct eth_hdr)+sizeof(struct ip_hdr))
    return false;

  ip_h = (struct ip_hdr *)(p->payload + sizeof(struct eth_hdr));

  if (IPH_PROTO(ip_h) == IP_PROTO_UDP) {
    if (p->len < sizeof(struct eth_hdr)+sizeof(struct ip_hdr)+sizeof(struct udp_hdr))
      return false;

    udp_h = (struct udp_hdr *)(p->payload + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));
    payload = (char*)(p->payload + sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct udp_hdr));

    if (ntohs(udp_h->dest) == ControlPort) {
      Serial.printf("X %+04d Y %+04d Z %+04d R %+04d C %x\n", payload[1]-0x80, payload[2]-0x80, payload[3]-0x80, payload[4]-0x80, payload[5]); 
    }
  }

  if (IPH_PROTO(ip_h) == IP_PROTO_TCP) {
    if (p->len < sizeof(struct eth_hdr)+sizeof(struct ip_hdr)+sizeof(struct tcp_hdr))
      return false;

    tcp_h = (struct tcp_hdr *)(p->payload + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));
  }

  return true;
}

err_t my_output_drone (struct netif *outp, struct pbuf *p) {
  if (check_packet_out(p)) {
    return orig_output_drone(outp, p);
  } else {
    pbuf_free(p);
    return ERR_OK;
  }
}

// patches the netif to insert the filter functions
void patch_netif(ip_addr_t netif_ip, netif_input_fn ifn, netif_input_fn *orig_ifn, netif_linkoutput_fn ofn, netif_linkoutput_fn *orig_ofn)
{
struct netif *nif;

  for (nif = netif_list; nif != NULL && nif->ip_addr.addr != netif_ip.addr; nif = nif->next);
  if (nif == NULL) return;

  if (ifn != NULL && nif->input != ifn) {
    *orig_ifn = nif->input;
    nif->input = ifn;
  }
  if (ofn != NULL && nif->linkoutput != ofn) {
    *orig_ofn = nif->linkoutput;
    nif->linkoutput = ofn;
  }
}


void setup() {
  Serial.begin(115200);
  Serial.printf("\n\nEPS E58 Drone Controller\n");

  // first, connect to STA so we can get a proper local DNS server
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, "");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  
  Serial.printf("\nSTA: %s\n", WiFi.localIP().toString().c_str());

  WiFi.softAPConfig(  // enable AP, with android-compatible google domain
    IPAddress(172, 217, 28, 254),
    IPAddress(172, 217, 28, 254),
    IPAddress(255, 255, 255, 0));
  WiFi.softAP(APSSID, "");
  Serial.printf("AP: %s\n", WiFi.softAPIP().toString().c_str());

  err_t ret = ip_napt_init(NAPT, NAPT_PORT);

  if (ret == ERR_OK) {
    ret = ip_napt_enable_no(SOFTAP_IF, 1);
    if (ret == ERR_OK) {
      Serial.printf("Initialization successful, connect to '%s'\n",APSSID);
    }
  }

  if (ret != ERR_OK) {
    Serial.printf("Initialization failed\n");
  }
  
  // Insert the filter functions
  patch_netif(WiFi.localIP(), my_input_drone, &orig_input_drone, my_output_drone, &orig_output_drone);
}

#else

void setup() {
  Serial.begin(115200);
  Serial.printf("\n\nNAPT not supported in this configuration\n");
}

#endif

void loop() {
}