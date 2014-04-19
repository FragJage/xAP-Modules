#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include <time.h>
#include "Service.h"
#include "Fichier.h"
#include "xpp.h"
#include "Fictif.h"

#ifdef WIN32
#define close closesocket
#endif


int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_receiver_sockfd;
int g_IntervalInfo;

PARAM *g_LstParam = NULL;
CAPTEUR *g_LstCapteur = NULL;
char FichierCnf[256];


/****************************************************************************************************************/
/* Initialisation du module Rss																					*/
int Fictif_init()
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

	if(Fcnf_Valeur(g_LstParam, "XAP_Port", i_tmp)==1) i_interfaceport = atoi(i_tmp);	//3639
	if(Fcnf_Valeur(g_LstParam, "XAP_Debug", i_tmp)==1) i_debuglevel = atoi(i_tmp);		//0
	Fcnf_Valeur(g_LstParam, "XAP_UID", i_uniqueID);										//GUID
	Fcnf_Valeur(g_LstParam, "XAP_Instance", i_instance);								//DEFAULT_INSTANCE
	Fcnf_Valeur(g_LstParam, "XAP_Interface", i_interfacename);							//eth0
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalInfo", i_tmp)==1) g_IntervalInfo = atoi(i_tmp);
	if(Fcnf_Valeur(g_LstParam,	"FICTIF_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);		//0

	if(g_IntervalInfo==0) g_IntervalInfo = 60;

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);
}

/****************************************************************************************************************/
/* Lire le paramétrage des capteurs fictifs																		*/
int Fictif_LireIni()
{
	int			i,Type,No;
	char		Cle[128];
	char		Section[128];
	char		Temp[256];
	CAPTEUR		*Capteur;
	

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Parcours des sections du fichier de conf.");

	//Faire le ménage
	while(g_LstCapteur != NULL)
	{
		Capteur = g_LstCapteur;
		g_LstCapteur = Capteur->Suivant;
		free(Capteur);
	}

	//Parcourir les sections
	i=1;
	No=1;
	while(Fcnf_Section(g_LstParam, i, Section))
	{
		//Sauter les sections XAP et FICTIF
		if(STRICMP(Section, "XAP")==0)
		{
			i++;
			continue;
		}
		if(STRICMP(Section, "FICTIF")==0)
		{
			i++;
			continue;
		}

		//Section valide ?
		sprintf(Cle, "%s_Type", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Temp))
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Section %s ignorée car pas de clé Type.", Section);
			i++;
			continue;
		}

		//Type valide ?
		Type = FICTIF_TYPE_INCONNU;
		if(STRICMP(Temp, "Booléen")==0)		Type = FICTIF_TYPE_BOOLEEN;
		if(STRICMP(Temp, "Booleen")==0)		Type = FICTIF_TYPE_BOOLEEN;
		if(STRICMP(Temp, "Boolean")==0)		Type = FICTIF_TYPE_BOOLEEN;
		if(STRICMP(Temp, "Numérique")==0)	Type = FICTIF_TYPE_NUMERIQUE;
		if(STRICMP(Temp, "Numerique")==0)	Type = FICTIF_TYPE_NUMERIQUE;
		if(STRICMP(Temp, "Numeric")==0)		Type = FICTIF_TYPE_NUMERIQUE;
		if(STRICMP(Temp, "Niveau")==0)		Type = FICTIF_TYPE_LEVEL;
		if(STRICMP(Temp, "Level")==0)		Type = FICTIF_TYPE_LEVEL;
		if(Type == FICTIF_TYPE_INCONNU)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Section %s ignorée car le type %s est inconnu.", Section, Temp);
			i++;
			continue;
		}

		//Allocation structure
		Capteur = malloc(sizeof(CAPTEUR));
		if(Capteur==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour stocker les capteurs fictifs.");
			return FALSE;
		}

		//Lecture des paramètres
		strcpy(Capteur->Nom, Section);
		Capteur->Type = Type;
		Capteur->Valeur = 0;
		Capteur->bMemo = FALSE;

		sprintf(Cle, "%s_Defaut", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Temp)) 
		{
			Capteur->Valeur = atof(Temp);
			if (STRICMP(Temp, "on")==0) Capteur->Valeur = 1;
			if (STRICMP(Temp, "off")==0) Capteur->Valeur = 0;
		}

		sprintf(Cle, "%s_Memo", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Temp)) 
			if((Temp[0]=='O')||(Temp[0]=='Y')) Capteur->bMemo = TRUE;

		Capteur->No = No;
		No++;

		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Capteur fictif %s %d %f", Capteur->Nom, Capteur->Type, Capteur->Valeur);

		//Ajout dans la liste
		Capteur->Suivant = g_LstCapteur;
		g_LstCapteur = Capteur;

		i++;
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de parcours des sections du fichier de conf.");
	return TRUE;
}

/****************************************************************************************************************/
/* Recherche d'un capteur																						*/
CAPTEUR *Fictif_ChercherNom(char *Nom)
{
	CAPTEUR *Capteur;
	BOOL bOK;


	//Parcours des capteurs
	for(Capteur=g_LstCapteur; Capteur!=NULL; Capteur = Capteur->Suivant)
	{
		bOK = TRUE;
		if(STRICMP(Nom, Capteur->Nom) != 0) bOK = FALSE;
		if(bOK == TRUE) return Capteur;
	}
	return NULL;
}

int Fictif_BSCInfo(int interval, CAPTEUR *LstCapteur) 
{
	time_t i_timenow;
	static time_t i_intervaltick=0;
	CAPTEUR *Capteur;


	i_timenow = time((time_t*)0);
	if ((i_timenow-i_intervaltick<interval)&&(i_intervaltick!=0)) return 0;

	i_intervaltick=i_timenow;

	//Parcours des capteurs et envoi du message l'info
	for(Capteur=LstCapteur; Capteur!=NULL; Capteur = Capteur->Suivant) 
	{
		switch(Capteur->Type)
		{
			case FICTIF_TYPE_BOOLEEN:	
				xpp_info(Capteur->No, Capteur->Nom, (int)Capteur->Valeur, 0, 0, NULL);
				break;
			case FICTIF_TYPE_NUMERIQUE:	
				xpp_info(Capteur->No, Capteur->Nom, 0, 0, Capteur->Valeur, NULL);
				break;
			case FICTIF_TYPE_LEVEL:		
				xpp_info(Capteur->No, Capteur->Nom, 0, (int)Capteur->Valeur, 0, NULL);
				break;
		}
	}
	return 1;
}

/****************************************************************************************************************/
/* Mémorisation des valeurs																						*/
int ValeursCharge()
{
	char Fichier[256];
	char Nom[256];
	char *Valeur;
	FILE	*hFic;
	CAPTEUR *Capteur;


	FichierStd(Fichier, TYPFIC_DATA);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Lecture des valeurs mémorisées dans %s.", Fichier);

	//Parcourir le fichier
    if((hFic = fopen(Fichier, "r")) == NULL)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible d'ouvrir le fichier des valeurs mémorisées (%s).", Fichier);
		return FALSE;
	}
	while( (!feof(hFic)) && (fgets(Nom, sizeof(Nom), hFic)) )
	{
		Valeur = strchr(Nom, '=');
		if(Valeur==NULL) continue;
		
		*Valeur = '\0';
		Valeur++;
		
		if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Lu %s = %s.", Nom, Valeur);

		Capteur = Fictif_ChercherNom(Nom);
		if(Capteur==NULL)
		{
			if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Le capteur %s est introuvable.", Nom);
			continue;
		}

		if(Capteur->bMemo == TRUE)
		{
			Capteur->Valeur = atof(Valeur);
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Nouvelle valeur de %s : %s.", Nom, Valeur);
		}
		else
		{
			if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Le capteur %s est trouvé, mais il n'est pas à mémoriser.", Nom);
		}
	}
	fclose(hFic);

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de lecture des valeurs mémorisées.");
	return TRUE;
}

int ValeursSauve()
{
	char Fichier[256];
	char Ligne[256];
	FILE	*hFic;
	CAPTEUR *Capteur;


	FichierStd(Fichier, TYPFIC_DATA);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Enregistrement des valeurs à mémoriser dans %s.", Fichier);

	//Parcourir le fichier
    if((hFic = fopen(Fichier, "w")) == NULL)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible d'ouvrir le fichier des valeurs mémorisées (%s).", Fichier);
		return FALSE;
	}

	for(Capteur=g_LstCapteur; Capteur!=NULL; Capteur = Capteur->Suivant)
	{
		if(Capteur->bMemo == TRUE)
		{
			sprintf(Ligne, "%s=%f", Capteur->Nom, Capteur->Valeur);
			fputs(Ligne, hFic);
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Enregistrement de %s = %s.", Capteur->Nom, Capteur->Valeur);
		}
	}
	fclose(hFic);

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de l'enregistrement des valeurs à mémoriser.");
	return TRUE;
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
			Fictif_LireIni();
			ValeursCharge();
		}
		return 0;
	}

	return 1;
}

int xpp_handler_CmdFictif()
{
	char i_temp[128];
	char *ptr;
	int Etat;
	int bEtat;
	int No;
	CAPTEUR *Capteur;
	CAPTEUR *Capi;


	Capteur = NULL;

	//L'id est dans le bloc output.state.* ?
	No = xpp_GetTargetId();
	if(No!=-1)
	{
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Id capteur output.state.1:id = %d.", No);
		//Conversion No -> Id
		for(Capi=g_LstCapteur; Capi!=NULL; Capi = Capi->Suivant)
		{
			if(Capi->No == No) Capteur = Capi;
		}
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur == NULL)) Flog_Ecrire("Impossible de trouver le capteur avec son ID (%d).", No);
	}

	//L'id est au delà du : dans target ?
	if( (Capteur == NULL) && (xpp_GetTargetName(i_temp)!=-1) )
	{
		ptr = strchr(i_temp, ':');
		if(ptr!=NULL) ptr++;
		Capteur = Fictif_ChercherNom(ptr);
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur == NULL)) Flog_Ecrire("Impossible de trouver le capteur par target: %s.", ptr);
	}

	if(Capteur == NULL)
	{
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur == NULL)) Flog_Ecrire("Impossible de trouver le nom du capteur.");
		return 0;
	}

	//Etat demandé
	if (xpp_GetTargetState(i_temp)==-1)
	{
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Impossible de lire output.state.1:State.");
		return 0;
	}
	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Capteur %s à mettre sur %s.", Capteur->Nom, i_temp);

	//Fixer l'état
	bEtat = FALSE;
	if (STRICMP(i_temp, "on")==0)		{ bEtat = TRUE; Etat = 1; }
	if (STRICMP(i_temp, "off")==0)		{ bEtat = TRUE; Etat = 0; }
	if (STRICMP(i_temp, "toogle")==0)	{ bEtat = TRUE; Etat = !Capteur->Valeur; }

	//Envoyer un event
	if(bEtat)
	{
		Capteur->Valeur = Etat;
		xpp_event(Capteur->No, Capteur->Nom, Etat, 0, 0);
	}
	else
	{
		Capteur->Valeur = atof(i_temp);
		xpp_event(Capteur->No, Capteur->Nom, 0, (int)Capteur->Valeur, Capteur->Valeur);
	}

	if(Capteur->bMemo==TRUE) ValeursSauve();

	return 0;
}

int xpp_handler_QueryFictif()
{
	char i_temp[128];
	char *ptr;
	int No;
	CAPTEUR *Capteur;
	CAPTEUR *Capi;


	Capteur = NULL;

	//L'id est dans le bloc output.state.* ?
	No = xpp_GetTargetId();
	if(No!=-1)
	{
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Id capteur output.state.1:id = %d.", No);
		//Conversion No -> Id
		for(Capi=g_LstCapteur; Capi!=NULL; Capi = Capi->Suivant)
		{
			if(Capi->No == No) Capteur = Capi;
		}
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur == NULL)) Flog_Ecrire("Impossible de trouver le nom du capteur avec son ID (%d).", No);
	}

	//L'id est au delà du : dans target ?
	if( (Capteur == NULL) && (xpp_GetTargetName(i_temp)!=-1) )
	{
		ptr = strchr(i_temp, ':');
		if(ptr!=NULL) ptr++;
		Capteur = Fictif_ChercherNom(ptr);
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur == NULL)) Flog_Ecrire("Nom du capteur trouvé dans target: %s.", ptr);
	}

	if(Capteur == NULL)
	{
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur == NULL)) Flog_Ecrire("Impossible de trouver le nom du capteur.");
		return 0;
	}

	//Envoyer l'état demandé
	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Réponse : Le capteur %s = %f.", Capteur->Nom, Capteur->Valeur);

	if(xpp_GetSourceName(i_temp)!=-1)
		if(Capteur->Type = FICTIF_TYPE_BOOLEEN)
			xpp_info(Capteur->No, Capteur->Nom, (int)Capteur->Valeur, 0, 0, i_temp);
		else
			xpp_info(Capteur->No, Capteur->Nom, 0, (int)Capteur->Valeur, Capteur->Valeur, i_temp);
	else
		if(Capteur->Type = FICTIF_TYPE_BOOLEEN)
			xpp_info(Capteur->No, Capteur->Nom, (int)Capteur->Valeur, 0, 0, NULL);
		else
			xpp_info(Capteur->No, Capteur->Nom, 0, (int)Capteur->Valeur, Capteur->Valeur, NULL);

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
	g_receiver_sockfd = Fictif_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);

	//******************************************************************************************************************
	//*** Lecture du fichier de capteurs fictifs
	Fictif_LireIni();

	//******************************************************************************************************************
	//*** Chargement des valeurs mémorisées
	ValeursCharge();

	//******************************************************************************************************************
	//*** Boucle principale
	while(!g_bServiceStop) 
	{ 
		// Send heartbeat periodically
		xpp_heartbeat_tick(HBEAT_INTERVAL);
		
		// Envoi la liste complète des capteurs regulièrement
		Fictif_BSCInfo(g_IntervalInfo, g_LstCapteur);

		FD_ZERO(&i_rdfs);
		FD_SET(g_receiver_sockfd, &i_rdfs);

		i_tv.tv_sec=10;
		i_tv.tv_usec=0;
	
		select(g_receiver_sockfd+1, &i_rdfs, NULL, NULL, &i_tv);
		
		// Select either timed out, or there was data - go look for it.	
		if (FD_ISSET(g_receiver_sockfd, &i_rdfs))
		{
			// there was an incoming message, not that we care
			if (xpp_PollIncoming(g_receiver_sockfd, i_xpp_buff, sizeof(i_xpp_buff))>0)
			{
				switch(xpp_DispatchReception(i_xpp_buff))
				{
					case XPP_RECEP_CAPTEUR_CMD :
						if(!g_bServicePause) xpp_handler_CmdFictif();
						break;
					case XPP_RECEP_CAPTEUR_QUERY :
						if(!g_bServicePause) xpp_handler_QueryFictif();
						break;
					case XPP_RECEP_SERVICE_CMD :
						xpp_handler_service();
						break;
				}
			}
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
	g_bServiceStop = TRUE;
	shutdown(g_receiver_sockfd, 1);
	close(g_receiver_sockfd);
	return;
}
