//A FAIRE : Gérer les répétitions
//A FAIRE :  - Emettre 5 fois en HomeEasy DI-O
//A FAIRE :  - Reception 1 fois HomeEasy DI-O

//Nom=Chacon DI-O
//VerrouDebut=275-2675
//Bit1=310-1340-310-310
//Bit0=310-310-310-1340
//VerrouFin=275-9900
//Flux=Source1-State-Source2-[Level]

//Protocole HomeEasy : http://blog.domadoo.fr/2010/03/21/principe-du-protocole-homeeasy/

#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include <time.h>
#include <wiringPi.h>
#include <sched.h>
#include <errno.h>
#include "Service.h"
#include "Fichier.h"
#include "xpp.h"
#include "Radio.h"

#ifdef WIN32
#define close closesocket
#endif


int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_receiver_sockfd;
int g_writing_pipe;
int g_IntervalInfo;
int g_PinEmission = -1;
int g_PinReception = -1;

volatile struct timeval g_InterruptTime;
int g_InterruptVerrouDeb[2] = {275, 2675};
int g_InterruptBit0[4] = {260, 260, 260, 1300};
int g_InterruptBit1[4] = {260, 1300, 260, 260};
int g_InterruptVerrouFin[2] = {275, 9900};

PARAM *g_LstParam = NULL;
char FichierCnf[256];


/****************************************************************************************************************/
/* Initialisation du module Rss																					*/
int Radio_init()
{
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

	if(Fcnf_Valeur(g_LstParam,	"XAP_Port", i_tmp)==1) i_interfaceport = atoi(i_tmp);	//3639
	if(Fcnf_Valeur(g_LstParam,	"XAP_Debug", i_tmp)==1) i_debuglevel = atoi(i_tmp);		//0
	Fcnf_Valeur(g_LstParam,		"XAP_UID", i_uniqueID);										//GUID
	Fcnf_Valeur(g_LstParam,		"XAP_Instance", i_instance);								//DEFAULT_INSTANCE
	Fcnf_Valeur(g_LstParam,		"XAP_Interface", i_interfacename);							//eth0
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalInfo", i_tmp)==1) g_IntervalInfo = atoi(i_tmp);
	if(Fcnf_Valeur(g_LstParam,	"RADIO_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);		//0
	if(Fcnf_Valeur(g_LstParam,	"RADIO_PinEmission", i_tmp)==1) g_PinEmission = atoi(i_tmp);
	if(Fcnf_Valeur(g_LstParam,	"RADIO_PinReception", i_tmp)==1) g_PinReception = atoi(i_tmp);

	if(g_IntervalInfo==0) g_IntervalInfo = 60;

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);
}

/****************************************************************************************************************/
/* Temps réel																									*/
int scheduler_realtime()
{
	struct sched_param p;

	p.__sched_priority = sched_get_priority_max(SCHED_RR);
	if(sched_setscheduler(0, SCHED_RR, &p ) == -1) 
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible d'activer le mode temps réel.");
		return -1;
	}
	return 0;
}

int scheduler_standard()
{
	struct sched_param p;

	p.__sched_priority = 0;
	if(sched_setscheduler(0, SCHED_OTHER, &p) == -1)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de désactiver le mode temps réel.");
		return -1;
	}
	return 0;
}

/****************************************************************************************************************/
/* Protocole HomeEasy																							*/
int RadioHE_Emettre(long IdEmetteur, int Groupe, int Etat, int Bouton)
{
	int i;
	unsigned long stream;
	unsigned long mask;
	char StreamDebug[64] = {0};


	//***Construction du flux
	stream = IdEmetteur;
	stream = stream<<1;
	stream|= (Groupe&1);
	stream = stream<<1;
	stream|= (Etat&1);
	stream = stream<<4;
	stream|= (Bouton&15);

	//***Début temps réel
	scheduler_realtime();

	//*** Verrou début
//	digitalWrite(g_PinEmission, HIGH);
//	delayMicroseconds(275);
//	digitalWrite(g_PinEmission, LOW);
//	delayMicroseconds(9900);
	digitalWrite(g_PinEmission, HIGH);
	delayMicroseconds(275);
	digitalWrite(g_PinEmission, LOW);
	delayMicroseconds(2675);
//	digitalWrite(g_PinEmission, HIGH);

	//*** Envoi du flux
	mask = 1 << 31;
	for(i=0;i<32;i++)
	{
		if((stream&mask)>0)
		{
			strcat(StreamDebug, "1");
			digitalWrite(g_PinEmission, HIGH);
			delayMicroseconds(310);
			digitalWrite(g_PinEmission, LOW);
			delayMicroseconds(1340);
			digitalWrite(g_PinEmission, HIGH);
			delayMicroseconds(310);
			digitalWrite(g_PinEmission, LOW);
			delayMicroseconds(310);
		}
		else
		{
			strcat(StreamDebug, "0");
			digitalWrite(g_PinEmission, HIGH);
			delayMicroseconds(310);
			digitalWrite(g_PinEmission, LOW);
			delayMicroseconds(310);
			digitalWrite(g_PinEmission, HIGH);
			delayMicroseconds(310);
			digitalWrite(g_PinEmission, LOW);
			delayMicroseconds(1340);
		}
		mask = mask >> 1;
	}

	//*** Verrou fin
	digitalWrite(g_PinEmission, HIGH);
	delayMicroseconds(275);
	digitalWrite(g_PinEmission, LOW);
	delayMicroseconds(9900);

	//***Fin temps réel
	scheduler_standard();
	//delay(10);

	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Envoyé : %s", StreamDebug);
	return 0;
}

void Radio_Interrupt() //BOOL bFrontMontant)
{
	struct timeval t;
	unsigned long micros;
	unsigned long valeur;
	int max;

	volatile static long long s_Stream=0;
	volatile static BOOL s_bDeb  = TRUE;
	volatile static BOOL s_bBit0 = FALSE;
	volatile static BOOL s_bBit1 = FALSE;
	volatile static BOOL s_bFin  = FALSE;
	volatile static int s_Pos    = 0;
	

	//*** Longueur du front précédent en µs
	gettimeofday(&t, NULL);
    if (t.tv_sec > g_InterruptTime.tv_sec) micros = 1000000L; else micros = 0;
    micros += (t.tv_usec - g_InterruptTime.tv_usec);
	if(micros < 50) return; //Parasites

	//*** Mémoriser le début du front suivant
	g_InterruptTime.tv_sec = t.tv_sec;
	g_InterruptTime.tv_usec = t.tv_usec;

	//*** Controle du verrou début
	if(s_bDeb)
	{
		valeur = g_InterruptVerrouDeb[s_Pos];
		if((micros<(valeur/1.2))||(micros>(valeur*1.2)))
		{
			s_Pos = 0;
			return;
		}
		s_Pos++;
		if(s_Pos==sizeof(g_InterruptVerrouDeb)/sizeof(int))
		{
			s_Pos   = 0;
			s_Stream=0;
			s_bDeb  = FALSE;
			s_bBit0 = TRUE;
			s_bBit1 = TRUE;
			s_bFin  = TRUE;
		}
		return;
	}

	//*** Détection Bit0
	if(s_bBit0)
	{
		valeur = g_InterruptBit0[s_Pos];
		if((micros>(valeur/1.2))&&(micros<(valeur*1.2)))
		{
			if((s_Pos+1)==(sizeof(g_InterruptBit0)/sizeof(int)))
			{
				s_Pos   = 0;
				s_bBit0 = TRUE;
				s_bBit1 = TRUE;
				s_bFin  = TRUE;
				s_Stream <<= 1;
				return;
			}
		}
		else
		{
			s_bBit0 = FALSE;
		}
	}
	//*** Détection Bit1
	if(s_bBit1)
	{
		valeur = g_InterruptBit1[s_Pos];
		if((micros>(valeur/1.2))&&(micros<(valeur*1.2)))
		{
			if((s_Pos+1)==(sizeof(g_InterruptBit1)/sizeof(int)))
			{
				s_Pos   = 0;
				s_bBit0 = TRUE;
				s_bBit1 = TRUE;
				s_bFin  = TRUE;
				s_Stream <<= 1;
				s_Stream |= 1;
				return;
			}
		}
		else
		{
			s_bBit1 = FALSE;
		}
	}
	//*** Détection Verrou Fin
	if(s_bFin)
	{
		valeur = g_InterruptVerrouFin[s_Pos];
		if((micros>(valeur/1.2))&&(micros<(valeur*1.2)))
		{
			if((s_Pos+1)==(sizeof(g_InterruptVerrouFin)/sizeof(int)))
			{
				s_Pos   = 0;
				s_bDeb = TRUE;

				Flog_Ecrire("Stream %lld", s_Stream);

				if(write(g_writing_pipe, &s_Stream, sizeof(s_Stream))<0) 
					Flog_Ecrire("Echec du write pipe.");
				//Ne pas envoyer les répétitions ?
				return;
			}
		}
		else
		{
			s_bFin = FALSE;
		}
	}

	s_Pos++;

	if(!(s_bBit0|s_bBit1|s_bFin))
	{
		/*
		for(max=0; max<DebugMemoPos; max++)
		{
			Flog_Ecrire("%d", DebugMemoTps[max]);
		}
		*/
		//Flog_Ecrire("Echec sur Tps %d Pos %d", micros, s_Pos-1);
		s_Pos   = 0;
		s_bDeb = TRUE;
	}
}

/****************************************************************************************************************/
/* Traitement d'un message																						*/
int xpp_handler_service()
{
	char i_temp[64];;


	if (xpp_GetCmd("request:state", i_temp)!=0)
	{
		if (STRICMP(i_temp, "stop")==1) g_bServiceStop = TRUE;
		if (STRICMP(i_temp, "start")==1) g_bServicePause = FALSE;
		if (STRICMP(i_temp, "pause")==1) g_bServicePause = TRUE;
		return 0;
	}
	if (xpp_GetCmd("request:init", i_temp)!=0)
	{
		if (STRICMP(i_temp, "config")==1)
		{
			Fcnf_Lire(FichierCnf, &g_LstParam);
		}
		return 0;
	}

	return 1;
}

int xpp_handler_CmdRadio()
{
	char i_temp[128];
	char *Ptr;
	char *IdEmetteur;
	char c;
	int Groupe=0;
	int Bouton;
	int Etat=-1;


	//L'id doit être au delà du : dans target
	if(xpp_GetTargetName(i_temp)==-1)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de lire la target.");
		return -1;
	}

	IdEmetteur = strchr(i_temp, ':');
	if(IdEmetteur==NULL) 
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de trouver le : dans la target.");
		return -1;
	}

	IdEmetteur++;

	//Etat demandé
	if (xpp_GetTargetState(i_temp)==-1)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de lire output.state.1:State.");
		return -1;
	}

	//Fixer l'état
	if (STRICMP(i_temp, "on")==0)	Etat = 1;
	if (STRICMP(i_temp, "off")==0)	Etat = 0;
	if(Etat==-1)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Etat incompréhensible : %s, attendu on/off.", i_temp);
		return -1;
	}

	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Message xAP : Mettre %s sur %s.", IdEmetteur, i_temp);

	//Décortiquer le bouton et le groupe
	Ptr = strchr(IdEmetteur, '-');
	if(Ptr==NULL) Ptr = strchr(IdEmetteur, 'G');
	if(Ptr==NULL)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("L'id (%s) doit contenir un G ou un - pour définir le groupe.", IdEmetteur);
		return -1;
	}
	if(*Ptr=='G') Groupe = 1;
	*Ptr = '\0';
	Ptr++;

	Bouton = atoi(Ptr);
	if((Bouton>16)||(Bouton<0)||((Bouton==0)&&(*Ptr!='0')))
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Le bouton en fin d'id (%s) doit être entre 0 et 15.", IdEmetteur);
		return -1;
	}

	//Emettre
	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Emettre Id %s, Groupe %d, Etat %d, Bouton %d.", IdEmetteur, Groupe, Etat, Bouton);
	if(RadioHE_Emettre(atol(IdEmetteur), Groupe, Etat, Bouton)==-1)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Echec de l'émission.");
		return -1;
	}

	return 0;
}


/****************************************************************************************************************/
/* Fonctions services : start																					*/
int ServiceStart()
{
	fd_set i_rdfs;
	struct timeval i_tv;
	char i_xpp_buff[1500+1];
	char Fichier[256];
	int pipefd[2];
	int MaxFD;
	long long i_Stream;

	//******************************************************************************************************************
	//*** Initialisation générale
	FichierInit(g_ServiceChemin, g_ServiceNom);
	FichierStd(Fichier, TYPFIC_LOG);
	Flog_Init(Fichier);

	//******************************************************************************************************************
	//*** Bavardage
	Flog_Ecrire("Démarrage de %s", XAP_SOURCE);

	//******************************************************************************************************************
	//*** Lecture du fichier INI
	FichierStd(FichierCnf, TYPFIC_CNF);
	if(!Fcnf_Lire(FichierCnf, &g_LstParam))
	{
		FichierStd(FichierCnf, TYPFIC_LOC+TYPFIC_CNF);
		Fcnf_Lire(FichierCnf, &g_LstParam);
	}
	Flog_Ecrire("Fichier de conf : %s", FichierCnf);
	g_receiver_sockfd = Radio_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);

	//******************************************************************************************************************
	//*** Petits Contrôles
	if((g_PinEmission == -1)&&(g_PinReception = -1))
	{
		Flog_Ecrire("Arrêt de %s car aucun PIN n'est configuré.", XAP_SOURCE);
		return 0;
	}
	if(g_PinEmission == g_PinReception)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("L'émission et la réception ne peuvent pas se faire sur le même PIN, PIN réception ignoré.");
		g_PinReception = -1;
	}
	if (g_debuglevel>=DEBUG_INFO)
	{
		if(g_PinEmission>0)
			Flog_Ecrire("Pin émission : %d", g_PinEmission);
		else
			Flog_Ecrire("Pin émission non configuré");

		if(g_PinReception>0)
			Flog_Ecrire("Pin réception : %d", g_PinReception);
		else
			Flog_Ecrire("Pin réception non configuré");
	}

	//******************************************************************************************************************
	//*** Initialisation WiringPi/Pipe/Interruption
	if(wiringPiSetup() < 0)
    {
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Echec de l'initialisation librairie WiringPi : %s", strerror(errno));
        return -1;
    }
    if(g_PinEmission  > 0) pinMode(g_PinEmission, OUTPUT);
	if(g_PinReception > 0)
	{
		if (pipe(pipefd) == -1)
		{
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible de créer un pipe de communication (%s). Mode réception abandonné.", strerror(errno));
            g_PinReception = 0;
        }
		g_writing_pipe = pipefd[1];
	}
	if(g_PinReception > 0)
	{
		pinMode(g_PinReception, INPUT);
		gettimeofday(&g_InterruptTime, NULL);
		if(wiringPiISR(g_PinReception, INT_EDGE_BOTH,  &Radio_Interrupt) < 0)
		{
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible d'accrocher une interruption : %s", strerror(errno));
		}
	}

	if(g_receiver_sockfd>pipefd[0])
		MaxFD = g_receiver_sockfd+1;
	else
		MaxFD = pipefd[0]+1;

	//******************************************************************************************************************
	//*** Boucle principale
	while(!g_bServiceStop) 
	{ 
		// Send heartbeat periodically
		xpp_heartbeat_tick(HBEAT_INTERVAL);
		
		FD_ZERO(&i_rdfs);
		FD_SET(g_receiver_sockfd, &i_rdfs);
		FD_SET(pipefd[0], &i_rdfs);

		i_tv.tv_sec=10;
		i_tv.tv_usec=0;
	
		select(MaxFD, &i_rdfs, NULL, NULL, &i_tv);
		
		// Select either timed out, or there was data - go look for it.	
		if (FD_ISSET(g_receiver_sockfd, &i_rdfs))
		{
			// there was an incoming message, not that we care
			if (xpp_PollIncoming(g_receiver_sockfd, i_xpp_buff, sizeof(i_xpp_buff))>0)
			{
				switch(xpp_DispatchReception(i_xpp_buff))
				{
					case XPP_RECEP_CAPTEUR_CMD :
						if(!g_bServicePause) xpp_handler_CmdRadio();
						break;
					case XPP_RECEP_SERVICE_CMD :
						xpp_handler_service();
						break;
				}
			}
		}

		// Info sur le pipe de l'interruption
		if (FD_ISSET(pipefd[0], &i_rdfs))
		{
			if(read(pipefd[0], &i_Stream, sizeof(i_Stream))<0)
				Flog_Ecrire(">Echec du read pipe.");
			Flog_Ecrire(">Stream %lld", i_Stream);
		}
		if(read(pipefd[0], &i_Stream, sizeof(i_Stream))<sizeof(i_Stream))
			Flog_Ecrire(">>Echec du read pipe.");
		else
			Flog_Ecrire(">>Stream %lld", i_Stream);

	} // while    

	close(pipefd[0]);
	close(pipefd[1]);
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
	g_bServiceStop = TRUE;
	shutdown(g_receiver_sockfd, 1);
	close(g_receiver_sockfd);
	return;
}
