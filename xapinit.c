//#include <unistd.h>
#include "Fichier.h"
#include "xapdef.h"

int g_debuglevel_xap=0;

int xap_discover_broadcast_network(int* a_sender_sockfd, struct sockaddr_in* a_sender_address) 
{
	long int i_inverted_netmask;
	struct sockaddr_in i_mybroadcast;
	struct sockaddr_in i_myinterface;
	struct sockaddr_in i_mynetmask;
	int i_optval, i_optlen;

	// Discover the broadcast network settings
	*a_sender_sockfd=(int)socket(AF_INET, SOCK_DGRAM, 0);

	i_optval=1;
	i_optlen=sizeof(int);
	if (setsockopt(*a_sender_sockfd, SOL_SOCKET, SO_BROADCAST, (char*)&i_optval, i_optlen)!=0) 
	{
		Flog_Ecrire("Cannot set options on broadcast socket");
		return 0;
	}


	// Query the low-level capabilities of the network interface
	// we are to use. If none passed on command line, default to
	// eth0.
	if(xap_net_info(g_interfacename, *a_sender_sockfd, &i_myinterface, &i_mynetmask)!=0)
	{
		i_inverted_netmask=~i_mynetmask.sin_addr.s_addr;
		i_mybroadcast.sin_addr.s_addr=i_inverted_netmask|i_myinterface.sin_addr.s_addr;
	}
	else
	{
		i_mybroadcast.sin_addr.s_addr=inet_addr("127.255.255.255");
	}
	a_sender_address->sin_addr.s_addr=i_mybroadcast.sin_addr.s_addr;

	if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("Autoconfig: xAP broadcasts on %s:%d",inet_ntoa(a_sender_address->sin_addr),g_interfaceport);
	if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("xAP uid=%s, source=%s.%s.%s",g_uid, XAP_ME, XAP_SOURCE, g_instance);
	if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("xAP Debug level %d",g_debuglevel_xap);

	a_sender_address->sin_family = AF_INET;
	a_sender_address->sin_port=htons(g_interfaceport);
	
	return 1;
}


int xap_discover_hub_address(int* a_receiver_sockfd, struct sockaddr_in* a_receiver_address, int a_port_range_low, int a_port_range_high) {

	// In a hub configuration, the Xap application attempts
	// to open consecutive ports on the loop back interface
	// in a known range.
	// Once found, a heartbeat message is sent regularly to the hub
	// indicating which port is in use, and all incoming xAP messages 
	// are relayed to this application on this port.

	// returns 1 on success, 0 on failure.

	int i;
	int i_hubflag=0;
	#ifdef WIN32
	u_long iMode = 1;
	#endif

	if (a_port_range_high<a_port_range_low)
	{
		Flog_Ecrire("Illegal socket range (highest port is lower than lowest port!)");
		exit(-1);
	}
	if (a_port_range_low==0)
	{
		Flog_Ecrire("Illegal socket range (cannot use a socket of 0)");
		exit(-1);
	}

  	*a_receiver_sockfd = (int)socket(AF_INET, SOCK_DGRAM, 0); // Non-blocking listener
	
	#ifdef WIN32
	ioctlsocket(*a_receiver_sockfd, FIONBIO, &iMode);
	if (iMode == 0) {
		Flog_Ecrire("Unable to create non-blocking socket");
		exit(-1);
	}
	#endif
	#ifdef __linux__
	fcntl(*a_receiver_sockfd, F_SETFL, O_NONBLOCK);
	#endif

	// First atttempt to open the a broadcast port
	// If this fails then we can assume that a hub is active on this host

	a_receiver_address->sin_family = AF_INET; 
	a_receiver_address->sin_addr.s_addr=g_xap_mybroadcast_address.sin_addr.s_addr;
	a_receiver_address->sin_port=htons(a_port_range_low);
	
	if (bind(*a_receiver_sockfd, (struct sockaddr*)a_receiver_address, sizeof(*a_receiver_address))!=0)
	{
		if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Broadcast socket port %d in use",a_port_range_low);
		if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Assuming a hub is active");
		i_hubflag=1;
	}
	else 
	{
		if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("Acquired broadcast socket, port %d",a_port_range_low);
		if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("Assuming no local hub is active");
		i_hubflag=0;
	}

	if (i_hubflag==1)
	{
		for (i=a_port_range_low; i<a_port_range_high; i++) 
		{
			a_receiver_address->sin_family = AF_INET;
			a_receiver_address->sin_addr.s_addr=inet_addr("127.0.0.1");
			a_receiver_address->sin_port=htons(i);
	
			if (bind(*a_receiver_sockfd, (struct sockaddr*)a_receiver_address, sizeof(struct sockaddr))!=0)
			{
				if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Socket port %d in use",i);
			}
			else
			{
				if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("Discovered port %d",i);
				break;
			}
		}
	}

	listen(*a_receiver_sockfd, MAX_QUEUE_BACKLOG);
	return 1; 	
}

int xap_discover_hub_address2(int* a_receiver_sockfd, struct sockaddr_in* a_receiver_address, struct sockaddr_in* a_broadcast_address, int a_port_range_low, int a_port_range_high) {

	// In a hub configuration, the Xap application attempts
	// to open consecutive ports on the loop back interface
	// in a known range.
	// Once found, a heartbeat message is sent regularly to the hub
	// indicating which port is in use, and all incoming xAP messages 
	// are relayed to this application on this port.

	// returns 1 on success, 0 on failure.

	int i;
	int i_hubflag=0;
	#ifdef WIN32
	u_long iMode = 1;
	#endif

	if (a_port_range_high<a_port_range_low) {
		Flog_Ecrire("Illegal socket range (highest port is lower than lowest port!)");
		exit(-1); // Illegal params
	}
	if (a_port_range_low==0) {
		Flog_Ecrire("Illegal socket range (cannot use a socket of 0)");
		exit(-1);				  // Illegal params
	}
	

  	*a_receiver_sockfd = (int)socket(AF_INET, SOCK_DGRAM, 0); // Non-blocking listener

	#ifdef WIN32
	ioctlsocket(*a_receiver_sockfd, FIONBIO, &iMode);
	if (iMode == 0) {
		Flog_Ecrire("Unable to create non-blocking socket");
		exit(-1);				  // Illegal params
	}
	#endif
	#ifdef __linux__
	fcntl(*a_receiver_sockfd, F_SETFL, O_NONBLOCK);
	#endif


	// First atttempt to open the a broadcast port
	// If this fails then we can assume that a hub is active on this host
	
	memset(a_receiver_address, 0, sizeof(struct sockaddr_in));
//	memcpy(a_receiver_address, a_broadcast_address, sizeof(struct sockaddr_in));

        a_receiver_address->sin_family = AF_INET; 
        Flog_Ecrire("Address %s",inet_ntoa(a_broadcast_address->sin_addr));
//	a_receiver_address->sin_addr.s_addr=a_receiver_address->sin_addr.s_addr;
	a_receiver_address->sin_port=htons(a_port_range_low);
//	a_receiver_address->sin_addr.s_addr=a_broadcast_address->sin_addr.s_addr;
	a_receiver_address->sin_addr.s_addr=inet_addr("0.0.0.0");

//	a_receiver_address->sin_port=htons(a_port_range_low);
	
	if (bind(*a_receiver_sockfd, (struct sockaddr*)a_receiver_address, sizeof(*a_receiver_address))!=0) {
		if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Broadcast socket port %d in use",a_port_range_low);
		if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Assuming a hub is active");
		i_hubflag=1;
	}


	else {
		if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("Acquired broadcast socket, port %d",a_port_range_low);
		if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("Assuming no local hub is active");
		i_hubflag=0;
	}

	if (i_hubflag==1) {

	for (i=a_port_range_low; i<a_port_range_high; i++) {

	a_receiver_address->sin_family = AF_INET;
	a_receiver_address->sin_addr.s_addr=inet_addr("127.0.0.1");
	a_receiver_address->sin_port=htons(i);

	
	
	if (bind(*a_receiver_sockfd, (struct sockaddr*)a_receiver_address, sizeof(struct sockaddr))!=0) 
	{
		if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Socket port %d in use",i);
	}
	else 
	{
		if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("Discovered port %d",i);
		break;
	}
	}
	}


	listen(*a_receiver_sockfd, MAX_QUEUE_BACKLOG);
	return 1; 	
}


int xap_init_defaut(char *uid, char *interfacename, int interfaceport, char *instance, int debuglevel)
{
	*g_instance = '\0';
	*g_uid = '\0';
	*g_interfacename = '\0';
	g_interfaceport = 0;
	g_debuglevel_xap = 0;


	if((uid!=NULL)&&(uid[0]!='\0')) strcpy(g_uid, uid);
	if(g_uid[0]=='\0') strcpy(g_uid, XAP_GUID);

	if((instance!=NULL)&&(instance[0]!='\0')) strcpy(g_instance, instance);
	if(g_instance[0]=='\0') gethostname(g_instance, 20);
	if(g_instance[0]=='\0') strcpy(g_instance, XAP_DEFAULT_INSTANCE);

	if((interfacename!=NULL)&&(interfacename[0]!='\0')) strcpy(g_interfacename, interfacename);
	if(g_interfacename[0]=='\0') strcpy(g_interfacename, "eth0");
	
	g_interfaceport = interfaceport;
	if(g_interfaceport==0) g_interfaceport=3639;

	g_debuglevel_xap=debuglevel;

	return 1;
}

int xap_init(char *uid, char *interfacename, int interfaceport, char *instance, int debuglevel)
{
	int i_lowest_port=XAP_LOWEST_PORT;
	int i_highest_port=XAP_HIGHEST_PORT;


	#ifdef WIN32
    WSADATA	WsaData;
    if (WSAStartup(MAKEWORD(2,0), &WsaData) != 0)
	{
		Flog_Ecrire("WSAStartup failed");
		return 0;
	}
	#endif

	xap_init_defaut(uid, interfacename, interfaceport, instance, debuglevel);

	if ((g_interfaceport<XAP_LOWEST_PORT)||(g_interfaceport>XAP_HIGHEST_PORT))
	{
		i_lowest_port=g_interfaceport;
		i_highest_port=g_interfaceport+1;
	}
			
	xap_discover_broadcast_network(&g_xap_sender_sockfd, &g_xap_sender_address);
	memcpy(&g_xap_mybroadcast_address, &g_xap_sender_address, sizeof(g_xap_sender_address));
	xap_discover_hub_address(&g_xap_receiver_sockfd, &g_xap_receiver_address, i_lowest_port, i_highest_port);
	return 1;
}

#ifdef WIN32
int xap_net_info(char *InterfaceName, int Socket, struct sockaddr_in* Address, struct sockaddr_in* NetMask)
{
	DWORD bytesReturned;
	u_long SetFlags;
	INTERFACE_INFO localAddr[10];  // Assume there will be no more than 10 IP interfaces 
	int numLocalAddr; 
	int i;


	if ((WSAIoctl(Socket, SIO_GET_INTERFACE_LIST, NULL, 0, &localAddr, sizeof(localAddr), &bytesReturned, NULL, NULL)) == SOCKET_ERROR)
	{
		Flog_Ecrire("WSAIoctl fails with error %d", GetLastError());
		return 0;
	}

	numLocalAddr = (bytesReturned/sizeof(INTERFACE_INFO));
	for (i=0; i<numLocalAddr; i++) 
	{
		SetFlags = localAddr[i].iiFlags;
		if(!(SetFlags & IFF_UP)) continue;
		if(!(SetFlags & IFF_BROADCAST)) continue;
		if(!(SetFlags & IFF_MULTICAST)) continue;
		if (SetFlags & IFF_LOOPBACK) continue;
		if (SetFlags & IFF_POINTTOPOINT) continue;

		//memcpy(Address, &localAddr[i].iiAddress, sizeof(SOCKADDR_IN));
		Address->sin_addr.s_addr=((struct sockaddr_in*)&localAddr[i].iiAddress)->sin_addr.s_addr;
		//memcpy(Netmask, &localAddr[i].iiNetmask, sizeof(SOCKADDR_IN));
		NetMask->sin_addr.s_addr=((struct sockaddr_in*)&localAddr[i].iiNetmask)->sin_addr.s_addr;
		break;
	}

	return 1;
}
#endif

#ifdef __linux__
int xap_net_info(char *InterfaceName, int Socket, struct sockaddr_in* Address, struct sockaddr_in* Netmask)
{
	struct ifreq i_interface;


	memset((char*)&i_interface, sizeof(i_interface),0);

	i_interface.ifr_addr.sa_family = AF_INET; 
	i_interface.ifr_broadaddr.sa_family = AF_INET; 
	strcpy(i_interface.ifr_name,InterfaceName);

	if ((ioctl(Socket, SIOCGIFADDR, &i_interface))!=0) 
	{
		Flog_Ecrire("Could not determine interface address for interface %s",g_interfacename);
		return 0;
	}
	Address->sin_addr.s_addr=((struct sockaddr_in*)&i_interface.ifr_broadaddr)->sin_addr.s_addr;
	//Flog_Ecrire("%s: address %s",i_interface.ifr_name, inet_ntoa( ((struct sockaddr_in*)&i_interface.ifr_addr)->sin_addr));

	if ((ioctl(Socket, SIOCGIFNETMASK, &i_interface))!=0) 
	{
		Flog_Ecrire("Unable to determine netmask for interface %s",g_interfacename);
		return 0;
	}
	Netmask->sin_addr.s_addr=((struct sockaddr_in*)&i_interface.ifr_broadaddr)->sin_addr.s_addr;

	return 1;
}
#endif
