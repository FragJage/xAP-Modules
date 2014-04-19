//A FAIRE 1 : Câbler la méthode POST
//A FAIRE 2 : Câbler la liste des capteur sur la page index
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#define  sockclose			closesocket
typedef int			*ARG_ACCEPT;
#endif

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
typedef int			SOCKET;
typedef struct sockaddr	SOCKADDR;
typedef struct sockaddr_in	SOCKADDR_IN;
typedef int	*ARG_ACCEPT;
#define  INVALID_SOCKET	-1
#define  sockclose		close
#define SOCKET_ERROR (-1)
#include <pthread.h>
#endif

#include "Service.h"
#include "Fichier.h"
#include "xpp.h"
#include "Http.h"

int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_receiver_sockfd;
int g_receiver_sockphp;
int g_Port_TCP;
int g_IntervalInfo;
int g_IntervalHbeat;

CAPTEUR *g_LstCapteur = NULL;
SERVICE *g_LstService = NULL;
CLIENT  *g_LstClient  = NULL;
PARAM   *g_LstParam   = NULL;
char FichierCnf[256];


/****************************************************************************************************************/
/* Initialisation du module Php																					*/
int Php_Init()
{
	char	tmp[20];
	char	uniqueID[20];
	char	instance[20];
	char	interfacename[20];
	int		interfaceport;
	int		debuglevel;

	*instance = '\0';
	*uniqueID = '\0';
	*interfacename = '\0';
	interfaceport = 0;
	debuglevel = 0;
	g_IntervalHbeat = 60;
	g_IntervalInfo = 60;

	if(Fcnf_Valeur(g_LstParam, "XAP_Port", tmp)==1) interfaceport = atoi(tmp);			//3639
	if(Fcnf_Valeur(g_LstParam, "XAP_Debug", tmp)==1) debuglevel = atoi(tmp);			//0
	Fcnf_Valeur(g_LstParam, "XAP_UID", uniqueID);										//GUID
	Fcnf_Valeur(g_LstParam, "XAP_Instance", instance);									//DEFAULT_INSTANCE
	Fcnf_Valeur(g_LstParam, "XAP_Interface", interfacename);							//eth0
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalHbeat", tmp)==1) g_IntervalHbeat = atoi(tmp);
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalInfo", tmp)==1) g_IntervalInfo = atoi(tmp);

	if(Fcnf_Valeur(g_LstParam, "Http_Debug", tmp)==1) g_debuglevel = atoi(tmp);
	if(Fcnf_Valeur(g_LstParam, "Http_Port", tmp)==1) g_Port_TCP = atoi(tmp);

	if(g_Port_TCP==0) g_Port_TCP = 4640;

	return xpp_init(uniqueID, interfacename, interfaceport, instance, debuglevel);
}

/****************************************************************************************************************/
/* Création de la socket d'écoute TCP																			*/
int Php_Socket()
{
    int sockTCP;
	int	arg;
    struct sockaddr_in servAddr;


    if ((sockTCP = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de créer la socket d'écoute PHP.");
		return 0;
	}

    arg = 1;
    setsockopt(sockTCP, SOL_SOCKET, SO_REUSEADDR, (char *)&arg, sizeof(int));	//Indispensable ?

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;         
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(g_Port_TCP);            

    if (bind(sockTCP, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de 'binder' la socket d'écoute PHP.");
		return 0;
	}

	if (listen(sockTCP, 5) < 0)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de 'listener' la socket d'écoute PHP.");
		return 0;
	}

    return sockTCP;
}

/****************************************************************************************************************/
/* Recherche dans la liste des capteur																		*/
CAPTEUR *Capteur_Recherche(char *id)
{
	CAPTEUR	*capteur;

	capteur = g_LstCapteur;
	while(capteur != NULL)
	{
		if(STRICMP(capteur->Id, id)==0)	return capteur;
		capteur = capteur->Suivant;
	}
	return NULL;
}

/****************************************************************************************************************/
/* Traitement des messages 'service'																			*/
int xpp_handler_service()
{
	char tmp[64];;


	if (xpp_GetCmd("request:state", tmp)!=0)
	{
		if (STRICMP(tmp, "stop")==0) g_bServiceStop = TRUE;
		if (STRICMP(tmp, "start")==0) g_bServicePause = FALSE;
		if (STRICMP(tmp, "pause")==0) g_bServicePause = TRUE;
		return 0;
	}
	if (xpp_GetCmd("request:init", tmp)!=0)
	{
		if (STRICMP(tmp, "config")==0)
		{
			Fcnf_Lire(FichierCnf, &g_LstParam);
		}
		return 0;
	}

	return 1;
}

/****************************************************************************************************************/
/* Traitement des messages 'xAP'																			*/
int xpp_handler_XAP(int typMsg)
{
	CAPTEUR	*capteur;
	char source[128];
	char valeur[64];


	//Lire la valeur
	if (xpp_GetTargetText(valeur)==-1) return 0;
	if(atof(valeur)==0) xpp_GetTargetState(valeur);

	//Rechercher le capteur dans le cache
	if (xpp_GetSourceName(source)==-1) return 0;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche du capteur %s dans le cache.", source);
	capteur = Capteur_Recherche(source);
	if(capteur != NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Capteur trouvé, %s mis en cache.", valeur);
		strcpy(capteur->Valeur, valeur);
		capteur->TimeRecu = time((time_t*)0);
		return 0;
	}

	//Ajouter au cache
	capteur = (CAPTEUR *) malloc(sizeof(CAPTEUR));
	if(capteur==NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas assez de mémoire pour mettre le capteur en cache.");
		return 0;
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Ajout au cache %s = %s", source, valeur);
	strcpy(capteur->Id, source);
	strcpy(capteur->Valeur, valeur);
	capteur->TimeRecu = time((time_t*)0);
	capteur->Suivant = g_LstCapteur;
	g_LstCapteur = capteur;

	return 1;
}

/****************************************************************************************************************/
/* Recherche dans la liste des services																			*/
SERVICE *Service_Recherche(char *id)
{
	SERVICE	*service;

	service = g_LstService;
	while(service != NULL)
	{
		if(STRICMP(service->Id, id)==0)	return service;
		service = service->Suivant;
	}
	return NULL;
}

/****************************************************************************************************************/
/* Traitement des messages 'xAP-Hbeat'																			*/
int xpp_handler_hbeat()
{
	char source[128];
	int interval;
	SERVICE	*service;

	//Lire les infos
	if(xpp_GetHbeat(source, &interval)==-1) return 0;

	//Rechercher le service dans le cache
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche le service %s dans le cache.", source);
	service = Service_Recherche(source);
	if(service != NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Service trouvé, %d mis en cache.", interval);
		service->Interval = interval;
		service->TimeRecu = time((time_t*)0);
		return 0;
	}

	//Ajouter au cache
	service = (SERVICE *) malloc(sizeof(SERVICE));
	if(service==NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas assez de mémoire pour mettre le service en cache.");
		return 0;
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Ajout au cache %s = %d", source, interval);
	strcpy(service->Id, source);
	service->Interval = interval;
	service->TimeRecu = time((time_t*)0);
	service->Suivant = g_LstService;
	g_LstService = service;

	return 1;
}

int MySend(SOCKET socket, char *msg)
{
	int ret;
	int reste;
	char *ptr;


	ret = 0;
	ptr = msg;
	reste = strlen(msg);
	while((reste>0)&&(ret!=-1))
	{
		ret = send(socket, ptr, reste, 0);
		reste -= ret;
		ptr += ret;
	}
	return ret;
}

void php_Send(char *valeur, SOCKET socket, BOOL bHttp)
{
	int ret;
	int taille;
	char *msg;


	msg = valeur;

	if(bHttp)
	{
		taille = strlen(valeur);
		msg = (char *) malloc(taille+128);
		if(msg==NULL)
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas assez de mémoire pour emettre la réponse en http.");
			return;
		}
		sprintf(msg, "HTTP/1.0 200 OK\nServer: xAP-Http/0.0.1\nContent-Type: text/html\nContent-Length: %d\n\n%s", taille, valeur);
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Valeur envoyée : %s.", msg);
	ret = MySend(socket, msg);
	if((ret == SOCKET_ERROR) && (g_debuglevel>=DEBUG_INFO)) Flog_Ecrire("Echec du send sur la socket TCP.");

	if(msg != valeur) free(msg);
}

char php_DispatchCmd(char *buffer)
{
	char *ptr;
	char cmde[16];

	ptr = strchr(buffer, ' ');
	if(ptr!=NULL) *ptr='\0';
	strcpy(cmde, buffer);
	if(ptr!=NULL) *ptr=' ';

	if(STRICMP(cmde, "GET")==0) return 'G';
	if(STRICMP(cmde, "DETAIL")==0) return 'D';
	if(STRICMP(cmde, "SET")==0) return 'S';
	if(STRICMP(cmde, "LIST")==0) return 'L';
	if(STRICMP(cmde, "HBEAT")==0) return 'H';
	if(STRICMP(cmde, "HTTP_AIDE")==0) return 'A';
	if(STRICMP(cmde, "HTTP_404")==0) return '4';
	return ' ';
}

void http_DecodeUrl(char *buffer)
{
	char *Ptr;
	int i;


	//Exemple de réception GET /GET?FRAGXAP.xAP-Fictif.RoDot/Temperature_BoucleDecharge HTTP/1.1...

	//Afficher l'aide
	if((STRNICMP(buffer, "GET / ", 6)==0)||(STRNICMP(buffer, "GET /index.", 11)==0))
	{
		strcpy(buffer, "HTTP_AIDE");
		return;
	}

	//Nettoyer la chaine
	Ptr = strchr(buffer, '\r');
	if(Ptr!=NULL) *Ptr = '\0';

	//Convertir HTTP -> STD
	for(Ptr=buffer+5, i=0; *Ptr!='\0'; Ptr++)
	{
		switch(*Ptr)
		{
			case '?' : 
				buffer[i++] = ' ';
				break;
			case '&' : 
				buffer[i++] = ' ';
				break;
			case '/' : 
				buffer[i++] = ':';
				break;
			case ' ' : 
				buffer[i++] = '\0';
				break;
			default :
				buffer[i++] = *Ptr;

		}
	}

	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Conversion de la requête HTTP : %s", buffer);
	if(php_DispatchCmd(buffer) != ' ') return;

	strcpy(buffer, "HTTP_404");
}

BOOL http_LireKeepAlive(char *buffer)
{
  char	*ptr = buffer;
  BOOL	keepalive = TRUE;


	while (ptr == NULL)
	{
		/* Détecte l'en-tête 'Connection: close' */
		if ((STRNICMP(ptr, "Connection:", 11)==0) && strstr(ptr, "close"))
		{
			keepalive = 0;
			break;
		}

		/* Ligne suivante */
		ptr = strchr(ptr, '\n');
		if(ptr==NULL) ptr = strchr(ptr, '\r');
		if(ptr==NULL) break;
	    
		/* Fin des en-têtes */
		if (*ptr=='\n' || *ptr=='\r') break;	    
	}

    return keepalive;
}

void php_Protocole_GetorDetail(char *cmde, BOOL bDetail, SOCKET socket, BOOL bHttp)
{
	char	*capteur;
	char	msg[256];
	CAPTEUR	*cache;
	time_t	timeDebut;
    struct tm	*tp;

	
	//Exemple de réception GET FRAGXAP.xAP-Fictif.RoDot:Temperature_BoucleDecharge
	//Exemple de réception DETAIL FRAGXAP.xAP-Fictif.RoDot:Temperature_BoucleDecharge

	//*******************************************************************************************************
	//*** Recherche du capteur dans la chaine
	capteur = strchr(cmde, ' ');
	if(capteur==NULL)
	{
		sprintf(msg, "ERROR : Impossible de trouver le capteur dans le message %s", cmde);
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire(msg);
		php_Send(msg, socket, bHttp);
		return;
	}
	*capteur = '\0';
	capteur++;

	//*******************************************************************************************************
	//*** Recherche du capteur dans le cache
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche du capteur %s dans le cache.", capteur);
	cache = Capteur_Recherche(capteur);

	//*******************************************************************************************************
	//*** Pas trouvé, je demande, et j'attends
	if(cache==NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas trouvé dans le cache -> Demander la valeur sur réseau xAP.");
		xpp_query(capteur);

		timeDebut = time((time_t*)0);
		while((cache==NULL)&&(time((time_t*)0)-timeDebut<5))
		{
			Sleep(100);
			cache = Capteur_Recherche(capteur);
		}
	}

	//*******************************************************************************************************
	//*** Trouvé, j'envoie la réponse
	if(bDetail)
	{
		tp = localtime(&cache->TimeRecu);
		sprintf(msg, "%s\t%s\t%02d/%02d/%04d %02d:%02d:%02d", cache->Id, cache->Valeur, tp->tm_mday, tp->tm_mon + 1, tp->tm_year+1900, tp->tm_hour, tp->tm_min, tp->tm_sec);
		php_Send(msg, socket, bHttp);
	}
	else
		php_Send(cache->Valeur, socket, bHttp);
}


void php_Protocole_Get(char *cmde, SOCKET socket, BOOL bHttp)
{
	//Exemple de réception GET FRAGXAP.xAP-Fictif.RoDot:Temperature_BoucleDecharge

	php_Protocole_GetorDetail(cmde, FALSE, socket, bHttp);
}

void php_Protocole_Detail(char *cmde, SOCKET socket, BOOL bHttp)
{
	//Exemple de réception DETAIL FRAGXAP.xAP-Fictif.RoDot:Temperature_BoucleDecharge

	php_Protocole_GetorDetail(cmde, TRUE, socket, bHttp);
}

void php_Protocole_Set(char *cmde)
{
	char *capteur;
	char *valeur;


	//Exemple de réception SET FRAGXAP.xAP-Fictif.RoDot:Temperature_BoucleDecharge 32

	capteur = strchr(cmde, ' ');
	if(capteur==NULL)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de trouver le capteur dans le message %s", cmde);
		return;
	}
	*capteur = '\0';
	capteur++;

	valeur = strchr(capteur, ' ');
	if(valeur==NULL)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de trouver la valeur dans le message %s", cmde);
		return;
	}
	*valeur = '\0';
	valeur++;

	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Mettre le capteur %s sur '%s'", capteur, valeur);

	if((STRICMP(valeur, "OFF") == 0) || (STRICMP(valeur, "ON") == 0))
		xpp_cmd(capteur, valeur, NULL, NULL);
	else
		xpp_cmd(capteur, NULL, valeur, valeur);
}

void php_Protocole_List(char *cmde, SOCKET socket, BOOL bHttp)
{
	CAPTEUR	*capteur;
    struct tm	*tp;
	char *msg;
	char *ptr;
	int taille;


	taille = 0;
	capteur = g_LstCapteur;
	while(capteur != NULL)
	{
		taille += strlen(capteur->Id)+strlen(capteur->Valeur)+23;
		capteur = capteur->Suivant;
	}

	capteur = g_LstCapteur;
	msg = (CHAR *) malloc(taille+1);
	if(msg==NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas assez de mémoire pour envoyer la liste.");
		php_Send("ERROR : Pas assez de mémoire pour envoyer la liste.", socket, bHttp);
		return;
	}

	ptr = msg;
	while(capteur != NULL)
	{
		tp = localtime(&capteur->TimeRecu);
		ptr += sprintf(ptr, "%s\t%s\t%02d/%02d/%04d %02d:%02d:%02d\r", capteur->Id, capteur->Valeur, tp->tm_mday, tp->tm_mon + 1, tp->tm_year+1900, tp->tm_hour, tp->tm_min, tp->tm_sec);
		capteur = capteur->Suivant;
	}
	php_Send(msg, socket, bHttp);
	free(msg);
}

int php_Protocole_ListHTML(char *liste, int tailleMax)
{
	CAPTEUR	*capteur;
    struct tm	*tp;
	char *ptr;
	char htmlDeb[] = "<table><tr><th>Capteur</th><th>Valeur</th><th>Date lecture</th></tr>";
	char htmlFin[] = "</table>";
	int taille;

	taille = strlen(htmlDeb)+strlen(htmlFin);
	capteur = g_LstCapteur;
	while(capteur != NULL)
	{
		taille += strlen(capteur->Id)+strlen(capteur->Valeur)+56;
		capteur = capteur->Suivant;
	}

	if(taille > tailleMax) return taille;

	capteur = g_LstCapteur;
	ptr = liste;
	ptr += sprintf(ptr, "%s", htmlDeb);
	while(capteur != NULL)
	{
		tp = localtime(&capteur->TimeRecu);
		ptr += sprintf(ptr, "<tr><td>%s</td><td>%s</td><td>%02d/%02d/%04d %02d:%02d:%02d</td></tr>", capteur->Id, capteur->Valeur, tp->tm_mday, tp->tm_mon + 1, tp->tm_year+1900, tp->tm_hour, tp->tm_min, tp->tm_sec);
		capteur = capteur->Suivant;
	}
	ptr += sprintf(ptr, "%s", htmlFin);

	return taille;
}

void php_Protocole_Hbeat(char *cmde, SOCKET socket, BOOL bHttp)
{
	SERVICE	*service;
    struct tm	*tp;
	char *msg;
	char *ptr;
	int taille;


	taille = 0;
	service = g_LstService;
	while(service != NULL)
	{
		taille += strlen(service->Id)+30;
		service = service->Suivant;
	}

	service = g_LstService;
	msg = (CHAR *) malloc(taille+8);
	if(msg==NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas assez de mémoire pour envoyer les Hbeat.");
		php_Send("ERROR : Pas assez de mémoire pour envoyer les Hbeat.", socket, bHttp);
		return;
	}
	ptr = msg;
	while(service != NULL)
	{
		tp = localtime(&service->TimeRecu);
		ptr += sprintf(ptr, "%s\t%d\t%02d/%02d/%04d %02d:%02d:%02d\r", service->Id, service->Interval, tp->tm_mday, tp->tm_mon + 1, tp->tm_year+1900, tp->tm_hour, tp->tm_min, tp->tm_sec);
		service = service->Suivant;
	}
	php_Send(msg, socket, bHttp);
	free(msg);
}

void http_Erreur404(SOCKET socket)
{
	int ret;


	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Demande sur protocole HTTP inconnue.");
	ret = MySend(socket, "HTTP/1.0 404 OK\nServer: xAP-Http/0.0.1\n\n");
	if((ret == SOCKET_ERROR) && (g_debuglevel>=DEBUG_INFO)) Flog_Ecrire("Echec du send sur la socket PHP.");
}

void http_Aide(SOCKET socket)
{
	char *msg;
	int taille;

	msg = (char *) malloc(10240);
	if(msg==NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas assez de mémoire pour envoyer l'aide.");
		php_Send("ERROR : Pas assez de mémoire pour envoyer l'aide.", socket, TRUE);
		return;
	}

	strcpy(msg, "\
<html>\
<head>\
</head>\
<body>\
<h2>xAP-PHP</h2>\
<h3>Description</h3>\
Service d'acc&egrave;s &agrave; votre r&eacute;seau xAP via des requ&ecirc;tes HTTP.\
<h3>Utilisation</h3>\
<ul>\
<li>GET?<i>CapteurxAP</i><br>Renvoie la valeur du capteur <i>CapteurxAP</i></li>\
<li>DETAIL?<i>CapteurxAP</i><br>Renvoie Nom&lt;TAB&gt;Valeur&lt;TAB&gt;Derni&egrave;re mise &agrave; jour du capteur <i>CapteurxAP</i></li>\
<li>SET?<i>CapteurxAP</i>&<i>ValeurxAP</i><br>Fixe le capteur <i>CapteurxAP</i> &agrave; la valeur <i>ValeurxAP</i></li>\
<li>LIST<br>Envoie le d&eacute;tail de tous les capteurs vu depuis le d&eacute;marrage du service.</li>\
<li>HBEAT<br>Envoie la liste des services xAP au format Nom&lt;TAB&gt;HeartBeat&lt;TAB&gt;Derni&egrave;re mise &agrave; jour.</li>\
</ul>\
<b>ATTENTION</b> : Le caract&egrave;re : dans le nom du capteur doit être remplacé par un /.<br>\
<h3>Exemples</h3>\
<ul>\
<li>HTTP://127.0.0.1:4640/GET?FRAGXAP.xAP-Fictif.RoDot/Temperature_Salon<br>Renvoie la temp&eacute;rature du salon.</li>\
<li>HTTP://127.0.0.1:4640/DETAIL?FRAGXAP.xAP-Fictif.RoDot/Temperature_Cuisine<br>Renvoie le détail des informations sur la temp&eacute;rature de la cuisine.</li>\
<li>HTTP://127.0.0.1:4640/SET?FRAGXAP.xAP-Fictif.RoDot/Lumiere_Cave&ON<br>Allume la lumi&egrave;re de la cave.</li>\
<li>HTTP://127.0.0.1:4640/SET?FRAGXAP.xAP-Fictif.RoDot/Consigne_Temperature&19<br>Fixe la temp&eacute;rature &agrave; 19.</li>\
</ul>\
<h3>Capteurs vu</h3>");

	taille = 10240 - strlen(msg) - 15;
	php_Protocole_ListHTML(msg+strlen(msg), taille);
	strcat(msg, "</body></html>");
	php_Send(msg, socket, TRUE);
	free(msg);
}

static DWORD WINAPI php_GereConnexion(LPVOID socket)
{
	char	buffer[4096];
	int		ret;
	SOCKET	sockClient = (SOCKET) socket;
	BOOL	keepAlive = TRUE;
	BOOL	modeHttp;


	while(keepAlive)
	{
		memset(buffer, 0, sizeof(buffer));
		ret = recv(sockClient, buffer, sizeof(buffer), 0);

		if(ret == SOCKET_ERROR)
		{
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Echec du recv sur la socket d'écoute TCP %d.", sockClient);
			sockclose(sockClient);
			return 0;
		}

		if(g_bServicePause)
		{
			sockclose(sockClient);
			return 0;
		}

		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("La socket d'écoute TCP %d a reçu : '%s'", sockClient, buffer);

		if(STRNICMP(buffer, "GET /", 5)==0)
		{
			modeHttp  = TRUE;
			keepAlive = http_LireKeepAlive(buffer);
			http_DecodeUrl(buffer);
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Mode keep alive (socket %d) : %d", sockClient, keepAlive);
		}
		else
		{
			modeHttp  = FALSE;
			keepAlive = FALSE;
		}

		switch(php_DispatchCmd(buffer))
		{
			case 'G' :
				php_Protocole_Get(buffer, sockClient, modeHttp);
				break;
			case 'D' :
				php_Protocole_Detail(buffer, sockClient, modeHttp);
				break;
			case 'S' :
				php_Protocole_Set(buffer);
				break;
			case 'L' :
				php_Protocole_List(buffer, sockClient, modeHttp);
				break;
			case 'H' :
				php_Protocole_Hbeat(buffer, sockClient, modeHttp);
				break;
			case 'A' :
				http_Aide(sockClient);
				break;
			case '4' :
				http_Erreur404(sockClient);
				break;
		}
	}
	sockclose(sockClient);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin du thread de traitement de la demande client %d.", sockClient);
	return 0;
}

/****************************************************************************************************************/
/* Fonctions services : start																					*/
int ServiceStart(char *ConfigFile, char *LogFile)
{
	fd_set	i_rdfs;
	char	fichier[256];
	struct timeval i_tv;
	char	i_xpp_buff[1500+1];
	int		arg;
    DWORD	ThreadId;			// Inutilisé
	//pthread_t id;
	SOCKET	SockClient;
	SOCKADDR_IN	SockAddr;


	//******************************************************************************************************************
	//*** Initialisation générale
	FichierInit(g_ServiceChemin, g_ServiceNom);
	if(LogFile==NULL)
	{
		FichierStd(fichier, TYPFIC_LOG);
		Flog_Init(fichier);
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

	Flog_Ecrire("Fichier de conf : %s", FichierCnf);
	g_receiver_sockfd = Php_Init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);

	//******************************************************************************************************************
	//*** Socket d'écoute PHP
	g_receiver_sockphp = Php_Socket();
	if(g_receiver_sockphp==0)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Arrêt du service car pas de socket d'écoute PHP.");
		return -1;
	}

	//******************************************************************************************************************
	//*** Boucle principale

	while(!g_bServiceStop) 
	{ 
		// Send heartbeat periodically
		xpp_heartbeat_tick(g_IntervalHbeat);
		
		FD_ZERO(&i_rdfs);
		FD_SET(g_receiver_sockfd, &i_rdfs);
		FD_SET(g_receiver_sockphp, &i_rdfs);

		i_tv.tv_sec=5;
		i_tv.tv_usec=0;
	
		select(g_receiver_sockphp+1, &i_rdfs, NULL, NULL, &i_tv);
		// Select either timed out, or there was data - go look for it.	
		if (FD_ISSET(g_receiver_sockfd, &i_rdfs))
		{
			// there was an incoming message, not that we care
			if (xpp_PollIncoming(g_receiver_sockfd, i_xpp_buff, sizeof(i_xpp_buff))>0)
			{
				switch(xpp_DispatchReception(i_xpp_buff))
				{
					case XPP_RECEP_CAPTEUR_INFO :
						if(!g_bServicePause) xpp_handler_XAP(XPP_RECEP_CAPTEUR_INFO);
						break;
					case XPP_RECEP_CAPTEUR_EVENT :
						if(!g_bServicePause) xpp_handler_XAP(XPP_RECEP_CAPTEUR_EVENT);
						break;
					case XPP_RECEP_SERVICE_CMD :
						xpp_handler_service();
						break;
				}
				//if(g_LstClient!=NULL) php_handler_File();
			}
			if (xpp_MessageType() == XPP_MSG_HBEAT) xpp_handler_hbeat();
		}

		if (FD_ISSET(g_receiver_sockphp, &i_rdfs))
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Réception d'un message TCP.");
			
			/* Accepter la connexion */
			arg = sizeof SockAddr;
			SockClient = accept(g_receiver_sockphp, (SOCKADDR *)&SockAddr, (ARG_ACCEPT)&arg);
			if(SockClient<=0)
			{
				if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Echec d'un accept sur la socket d'écoute TCP.");
				continue;
			}
			
			/* Créer un thread pour gérer la connexion */
			if (CreateThread(NULL, 0, php_GereConnexion, (LPVOID)SockClient, 0, &ThreadId) == NULL)
			{
				if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Echec de la creation du thread de traitement de la communication TCP.");
				continue;
			}

			//errno = pthread_create(&id, NULL, php_GereConnexion, (void*)SockClient);
			//if (errno) fatal_error("échec de pthread_create");

			/* Pas besoin de synchro entre les threads */
			//errno = pthread_detach(id);
			//if (errno) fatal_error("échec de pthread_detach");


		}
	} // while

	Flog_Ecrire("Arrêt de %s", XAP_SOURCE);
	return 0;
}  // main  
  
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
	Flog_Ecrire("Arrêt en cours...");
	g_bServiceStop = TRUE;
	shutdown(g_receiver_sockfd, 1);
	sockclose(g_receiver_sockfd);
	shutdown(g_receiver_sockphp, 1);
	sockclose(g_receiver_sockphp);
	return;
}
