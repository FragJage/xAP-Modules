#include "xapdef.h"
#include "Hub.h"
#include "Service.h"
#include "Fichier.h"

#define XAP_MAX_HUB_ENTRIES	50
#define MAX_BACKLOG_QUEUE 20  // number of connections that can queue (ignored by OS I think?)

int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_IntervalHbeat;

PARAM *g_LstParam = NULL;
char FichierCnf[256];

struct tg_xap_hubentry 
{
	int port; // ip-port to forward to
	int interval;
	int timer;
	int is_alive;
};

struct tg_xap_hubentry g_xap_hubentry[XAP_MAX_HUB_ENTRIES];

void xaphub_build_heartbeat(char* a_buff, int a_port, const char* a_instance)
{
	sprintf(a_buff, "xap-hbeat\n{\nv=12\nhop=1\nuid=%s\nclass=xap-hbeat.alive\nsource=%s.%s.%s\ninterval=%d\nport=%d\n}\n", g_uid, XAP_ME, XAP_SOURCE,  a_instance, g_IntervalHbeat, a_port);
	if (g_debuglevel>=DEBUG_VERBOSE) printf("Heartbeat source=%s, instance=%s, interval=%d, port=%d\n",XAP_SOURCE, a_instance, g_IntervalHbeat, a_port );
}

int xaphub_broadcast_heartbeat(const char* a_buff, int a_sock, const struct sockaddr_in* a_addr)
{
	int i;

	i= sendto(a_sock, a_buff, (int)strlen(a_buff), 0, (struct sockaddr *) &a_addr, sizeof(struct sockaddr_in));
	if (g_debuglevel>=DEBUG_VERBOSE) printf("Broadcasting heartbeat\n");

	return i;
}


int xaphub_init()
{
	int i;
	char i_tmp[20];
	char i_uniqueID[20];
	char i_instance[20];
	char i_interfacename[20];
	int i_interfaceport;
	int i_debuglevel;

	*i_instance = '\0';
	*i_uniqueID = '\0';
	*i_interfacename = '\0';
	i_interfaceport = 0;
	i_debuglevel = 0;

	for (i=0; i<XAP_MAX_HUB_ENTRIES; i++) g_xap_hubentry[i].is_alive=0;
	
	if(Fcnf_Valeur(g_LstParam, "XAP_Port", i_tmp)==1) i_interfaceport = atoi(i_tmp);//3639
	if(Fcnf_Valeur(g_LstParam, "XAP_Debug", i_tmp)==1) i_debuglevel = atoi(i_tmp);	//0
	Fcnf_Valeur(g_LstParam, "XAP_UID", i_uniqueID);									//XAP_GUID
	Fcnf_Valeur(g_LstParam, "XAP_Instance", i_instance);							//XAP_DEFAULT_INSTANCE
	Fcnf_Valeur(g_LstParam, "XAP_Interface", i_interfacename);						//eth0
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalHbeat", i_tmp)==1) g_IntervalHbeat = atoi(i_tmp);
	if(g_IntervalHbeat==0) g_IntervalHbeat = 60;
	if(Fcnf_Valeur(g_LstParam,	"HUB_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);		//0

	xap_init_defaut(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);

	return 1;
}

int xaphub_addentry(int a_port, int a_interval)
{
	// add new or update existing hub entry, resetting countdown timer
	int i;
	int matched;

	matched=XAP_MAX_HUB_ENTRIES;


	for (i=0; i<XAP_MAX_HUB_ENTRIES; i++) 
	{
		if (g_xap_hubentry[i].port==a_port) 
		{
				// entry exists, update it
				g_xap_hubentry[i].interval=a_interval;
				g_xap_hubentry[i].timer=a_interval*2;
				g_xap_hubentry[i].is_alive=1;
				matched=i;
				if (g_debuglevel>=DEBUG_VERBOSE) printf("Heartbeat for port %d\n",g_xap_hubentry[i].port);
				break;
		}
	}
	if (matched==XAP_MAX_HUB_ENTRIES) 
	{
		// no entry exists, create a new entry in first free slot
			for (i=0; i<XAP_MAX_HUB_ENTRIES; i++)
			{
				if (g_xap_hubentry[i].is_alive==0)
				{
						// free entry exists, use it it
						g_xap_hubentry[i].port=a_port;
						g_xap_hubentry[i].interval=a_interval;
						g_xap_hubentry[i].timer=a_interval*2;
						g_xap_hubentry[i].is_alive=1;
						matched=i;
						if (g_debuglevel>=DEBUG_INFO) printf("Connecting port %d\n",g_xap_hubentry[i].port);
						break;
				}
			}
	}
	return matched; // value of XAP_MAX_HUB_ENTRIES indicates list full
}

void xaphub_tick(time_t a_interval) 
{
	// Called regularly. a_interval specifies number of whole seconds
	// elapsed since last call.
	int i;

	for (i=0; i<XAP_MAX_HUB_ENTRIES; i++)
	{
		if ((g_xap_hubentry[i].is_alive)&&(g_xap_hubentry[i].timer!=0))
		{
				// update timer entries. If timer is 0 then
				// ignore hearbeat ticks
				g_xap_hubentry[i].timer-=(int)a_interval;

				if (g_xap_hubentry[i].timer<=0)
				{
					if (g_debuglevel>=DEBUG_INFO) printf("Disconnecting port %d due to loss of heartbeat\n",g_xap_hubentry[i].port);
					g_xap_hubentry[i].is_alive=0; // mark as idle
				}
				break;
		}
	}
}

int xaphub_relay(int a_txsock, const char* a_buf)
{
	struct sockaddr_in tx_addr;
	int i,j=0;

	tx_addr.sin_family = AF_INET;		
	tx_addr.sin_addr.s_addr=inet_addr("127.0.0.1");


	for (i=0; i<XAP_MAX_HUB_ENTRIES; i++) 
	{
		if (g_xap_hubentry[i].is_alive==1) 
		{
				// entry exists, use it
			if (g_debuglevel>=DEBUG_VERBOSE) printf("Relayed to %d\n",g_xap_hubentry[i].port);
			j++;
			tx_addr.sin_port=htons(g_xap_hubentry[i].port);
            sendto(a_txsock, a_buf, (int)strlen(a_buf), 0, (struct sockaddr*)&tx_addr, sizeof(struct sockaddr_in));	
		}
	}
	return j; // number of connected hosts we relayed to
}


/* receive a xap (broadcast) packet and process */
int xap_handler(const char* a_buf, int a_port)
{
  char i_interval[16];
  char i_port[16];


  xapmsg_parse(a_buf);

	if (xapmsg_getvalue("xap-hbeat:interval", i_interval)==0) 
	{
	  if (g_debuglevel>=DEBUG_DEBUG) printf("Could not find <%s> in message\n","xap-hbeat.interval"); 
	}
	else
	{
		if (xapmsg_getvalue("xap-hbeat:port", i_port)==0)
		{
			if (g_debuglevel>=DEBUG_DEBUG) printf("Could not find <%s> in message\n","xap-hbeat:port"); 
		}
		else
		{
			xaphub_addentry(atoi(i_port), atoi(i_interval));
		}
	} // if interval

	return 1;
}

int ServiceStart(char *ConfigFile, char *LogFile) 
{
	char Fichier[256];
	
	int server_sockfd;
	int client_sockfd;
	int heartb_sockfd;
	struct sockaddr_in server_address;
	struct sockaddr_in heartbeat_addr;
	struct sockaddr_in client_address;

	struct sockaddr_in i_mybroadcast;
	struct sockaddr_in i_myinterface;
	struct sockaddr_in i_mynetmask;

	time_t i_timenow;
	time_t i_xaptick;
	time_t i_heartbeattick;

	int i_optval;
	int i_optlen;
	int i; 
	char buff[1500];
	int client_len;
	fd_set i_rdfs;
	struct timeval i_tv;
	int i_retval;

	#ifdef WIN32
	u_long iMode = 1;
	WSADATA	WsaData;
	#endif
  

	//******************************************************************************************************************
	//*** Initialisation générale
  	FichierInit(g_ServiceChemin, g_ServiceNom);
	if(LogFile==NULL)
	{
		FichierStd(Fichier, TYPFIC_LOG);
		Flog_Init(Fichier);
	}
	else
		Flog_Init(LogFile);

	//******************************************************************************************************************
	//*** Bavardage
	Flog_Ecrire("Démarrage de %s", XAP_SOURCE);

	//******************************************************************************************************************
	//*** Lecture du fichier INI
	if(ConfigFile==NULL)
	{
		FichierStd(FichierCnf, TYPFIC_CNF);
		if(!Fcnf_Lire(FichierCnf, &g_LstParam))
		{
			FichierStd(FichierCnf, TYPFIC_LOC+TYPFIC_CNF);
			Fcnf_Lire(FichierCnf, &g_LstParam);
		}
	}
	else
	{
		strcpy(FichierCnf, ConfigFile);
		Fcnf_Lire(ConfigFile, &g_LstParam);
	}

	if (g_debuglevel>=DEBUG_DEBUG) printf("Fichier de conf : %s\n", FichierCnf);

	//******************************************************************************************************************
	//*** Initialisation du hub
	xaphub_init();

	#ifdef WIN32
    if (WSAStartup(MAKEWORD(2,0), &WsaData) != 0)
	{
		Flog_Ecrire("WSAStartup failed");
		return 0;
	}
	#endif

	//ptrp = getprotobyname("udp");  

	//******************************************************************************************************************
	//*** Creation de la socket serveur
	server_sockfd=(int)socket(AF_INET, SOCK_DGRAM, 0);
	if (server_sockfd==-1)
	{
		Flog_Ecrire("Unable to establish server broadcast socket");
		return 0;
	}

	i_optval=1;
	i_optlen=sizeof(int);
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_BROADCAST, (char*)&i_optval, i_optlen)!=0)
	{
		Flog_Ecrire("Cannot set options on broadcast server socket");
		return 0;
	}
	
	i_optval=1;
	i_optlen=sizeof(int);
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&i_optval, i_optlen)!=0)
	{
		Flog_Ecrire("Cannot set reuseaddr options on broadcast server socket");
		return 0;
	}

	#ifdef WIN32
	ioctlsocket(server_sockfd, FIONBIO, &iMode);
	if (iMode == 0) 
	{
		Flog_Ecrire("Unable to create non-blocking server socket");
		return 0;
	}
	#endif
	#ifdef __linux__
	fcntl(server_sockfd, F_SETFL, O_NONBLOCK);
	#endif


	//******************************************************************************************************************
	//*** Ouverture du port d'écoute (en cas d'échec, on déduit qu'il y a déjà un hub actif)
	server_address.sin_family = AF_INET; 	
	server_address.sin_addr.s_addr=htonl(INADDR_ANY);
	server_address.sin_port=htons(g_interfaceport);
		
	if (bind(server_sockfd, (struct sockaddr*)&server_address, sizeof(server_address))!=0) 
	{
		Flog_Ecrire("Broadcast socket port %d in use",g_interfaceport);
		exit(-1);
	}
	else
	{
		Flog_Ecrire("Acquired broadcast socket, port %d",g_interfaceport);
		Flog_Ecrire("Assuming no local hub is active");		
	}

	listen(server_sockfd, MAX_BACKLOG_QUEUE);

	//******************************************************************************************************************
	//*** Creation des sockets client heartbeat (equivalent keep alive)
	client_sockfd =(int)socket(AF_INET, SOCK_DGRAM, 0);
	if (client_sockfd==-1)
	{
		Flog_Ecrire("Unable to establish client broadcast socket");
		return 0;
	}

	heartb_sockfd =(int)socket(AF_INET, SOCK_DGRAM, 0);
	if (client_sockfd==-1)
	{
		Flog_Ecrire("Unable to establish heartbeat broadcast socket");
		return 0;
	}

    i_optval=1;
    i_optlen=sizeof(int);
    if (setsockopt(heartb_sockfd, SOL_SOCKET, SO_BROADCAST, (char*)&i_optval, i_optlen))
	{
		Flog_Ecrire("Unable to set heartbeat broadcast socket options");
		return 0;
	}

	//*** Recherche de l'adresse de broadcast pour la socket heartbeat
	if(xap_net_info(g_interfacename, client_sockfd, &i_myinterface, &i_mynetmask)!=0)
	{
		i_mybroadcast.sin_addr.s_addr=(~i_mynetmask.sin_addr.s_addr)|i_myinterface.sin_addr.s_addr;
	}
	else
	{
		i_mybroadcast.sin_addr.s_addr=inet_addr("127.255.255.255");
	}

	heartbeat_addr.sin_family=AF_INET;
	heartbeat_addr.sin_port=htons(g_interfaceport);
	heartbeat_addr.sin_addr.s_addr=i_mybroadcast.sin_addr.s_addr;
  
	i_xaptick=time((time_t*)0);
	i_heartbeattick=time((time_t*)0)-g_IntervalHbeat; // force heartbeat on startup

	//******************************************************************************************************************
	//*** Boucle principale
	// Parse heartbeat messages received on broadcast interface
	// If they originated from this host, add the port number to the list of known ports
	// Otherwise ignore.
	// If ordinary header then pass to all known listeners
	Flog_Ecrire("%s à l'écoute", XAP_SOURCE);

	while(!g_bServiceStop) 
	{ 
		i_timenow=time((time_t*)0);
	
		if (i_timenow-i_xaptick>=1)
		{
			if (g_debuglevel>=DEBUG_DEBUG) printf("XAP Connection list tick %d\n",(int)i_timenow-(int)i_xaptick);
			xaphub_tick(i_timenow-i_xaptick);
			i_xaptick=i_timenow;
		}

		if (i_timenow-i_heartbeattick>=g_IntervalHbeat)
		{
			if (g_debuglevel>=DEBUG_DEBUG) printf("Outgoing heartbeat tick %d\n",(int)i_timenow-(int)i_heartbeattick);
 			i_heartbeattick=i_timenow;
			xaphub_build_heartbeat(buff, g_interfaceport, g_instance);

			// Send heartbeat to all external listeners
			xaphub_broadcast_heartbeat(buff, heartb_sockfd, &heartbeat_addr); 
			// Send heartbeat to all locally connected apps
			xaphub_relay(client_sockfd, buff);
		} // heartbeat tick

		FD_ZERO(&i_rdfs);
		FD_SET(server_sockfd, &i_rdfs);
		
		i_tv.tv_sec=5;
		i_tv.tv_usec=0;
		
		i_retval=select(server_sockfd+1, &i_rdfs, NULL, NULL, &i_tv);
		// Select either timed out, or there was data - go look for it.
		client_len=sizeof(struct sockaddr);
		i=recvfrom(server_sockfd, buff, sizeof(buff), 0, (struct sockaddr*) &client_address, &client_len);

		if (i!=-1) 
		{
			buff[i]='\0';
			
			if (g_debuglevel>=DEBUG_VERBOSE) printf("Message from client %s:%d\n",inet_ntoa(client_address.sin_addr),ntohs(client_address.sin_port));

			if (client_address.sin_addr.s_addr == i_myinterface.sin_addr.s_addr) 
			{
				if (g_debuglevel>=DEBUG_VERBOSE) printf("Message originated locally\n");
				xap_handler(buff,ntohs(client_address.sin_port));

				// Either relay the heartbeat to all local apps, because
				// this was a heartbeat message - in which case the originator
				// will see his own heartbeat (but that doesn't really matter

				// or this wasn't a heartbeat message anyway, so we should
				// relay it. Again the originator will see his own message

				xaphub_relay(client_sockfd, buff);

			}
			else 
			{
				if (g_debuglevel>=DEBUG_VERBOSE) printf("Message originated remotely, relay\n");
				xaphub_relay(client_sockfd, buff);
			}
		}
	}	// while
	Flog_Ecrire("Arrêt de %s", XAP_SOURCE);
	return 0;
}        // main

/****************************************************************************************************************/
/* Fonctions services : pause																					*/
void ServicePause(BOOL bPause)
{
	g_bServicePause = bPause;
	return;
}

/****************************************************************************************************************/
/* Fonctions services : stop																					*/
void ServiceStop()
{
	g_bServiceStop = TRUE;
	shutdown(g_xap_receiver_sockfd, 1);
	close(g_xap_receiver_sockfd);
	return;
}
