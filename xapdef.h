#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef __linux__
#include <termios.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/time.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#endif

#ifdef WIN32
void cfmakeraw(struct termios *termios_p);
#define strcasecmp _stricmp
#define close closesocket
#endif

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
	typedef int    BOOL;
#endif

extern const char* XAP_ME;
extern const char* XAP_SOURCE;
extern const char* XAP_GUID;
extern const char* XAP_DEFAULT_INSTANCE;

/********************************* xAP Handler & Parser ********************************/

#define XAP_MSG_HBEAT  1		// special control message, type heartbeat
#define XAP_MSG_ORDINARY  2		// ordinary xap message type
#define XAP_MSG_CONFIG_REQUEST 3		// xap config message type
#define XAP_MSG_CACHE_REQUEST 4			// xap cache message type
#define XAP_MSG_CONFIG_REPLY 5		// xap config message type
#define XAP_MSG_CACHE_REPLY 6			// xap cache message type

#define XAP_MSG_UNKNOWN  0              // unknown xap message type
#define XAP_MSG_NONE 0                  // (or no message received)

#define XAP_MAX_MSG_ELEMENTS 1000
#define XAP_MAX_KEYNAME_LEN 128
#define XAP_MAX_KEYVALUE_LEN 1500
#define XAP_MAX_SECTION_LEN 128

#define MAX_QUEUE_BACKLOG 50  // number of connections that can queue (ignored by OS I think?)
#define XAP_LOWEST_PORT 3639
#define XAP_HIGHEST_PORT 4639

#ifndef XAP_GLOBAL_DATA
#define XAP_GLOBAL_DATA

extern int g_debuglevel_xap; // 0=off, 1=info, 2=verbose, 3=debug

int g_xap_index;
struct tg_xap_msg {
char section[XAP_MAX_SECTION_LEN+1];
char name[XAP_MAX_KEYNAME_LEN+1];
char value[XAP_MAX_KEYVALUE_LEN+1];
};

struct tg_xap_msg g_xap_msg[XAP_MAX_MSG_ELEMENTS];

// The network interface, port that xAP is to use.

char g_interfacename[20];
int g_interfaceport;

char g_instance[20];
char g_serialport[20];
char g_uid[9];

// xAP global socket structures

int g_xap_sender_sockfd;
int g_xap_receiver_sockfd;

struct sockaddr_in g_xap_receiver_address;
struct sockaddr_in g_xap_mybroadcast_address;
struct sockaddr_in g_xap_sender_address;

#endif

// From xaptx
int xap_send_tick(int a_interval);
int xap_broadcast_heartbeat(int a_sock,  struct sockaddr_in* a_addr, int a_interval);
int xap_send_message(const char* a_buff);
int xap_heartbeat_tick(int a_interval);

// From xaprx
int xap_compare(const char* a_item1, const char* a_item2);
int xap_poll_incoming(int a_server_sockfd, char* a_buffer, int a_buffer_size);
int xapmsg_parse(const unsigned char* a_buffer);
int xapmsg_getvalue(const char* a_keyname, char* a_keyvalue);
int xapmsg_updatevalue(const char* a_keyname, const char* a_keyvalue);
int xapmsg_toraw(char* a_raw_msg);
int xapmsg_gettype();

// From xapinit
int xap_discover_broadcast_network(int* a_sender_sockfd, struct sockaddr_in* a_sender_address);
int xap_discover_hub_address(int* a_receiver_sockfd, struct sockaddr_in* a_receiver_address, int a_port_range_low, int a_port_range_high);
int xap_init(char *uid, char *interfacename, int interfaceport, char *instance, int debuglevel);
int xap_init_defaut(char *uid, char *interfacename, int interfaceport, char *instance, int debuglevel);
int xap_net_info(char *InterfaceName, int Socket, struct sockaddr_in* Address, struct sockaddr_in* NetMask);
