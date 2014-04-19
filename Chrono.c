#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include "Service.h"
#include "Fichier.h"
#include "xpp.h"
#include "Chrono.h"

#ifdef WIN32
#define close closesocket
#endif

int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_receiver_sockfd;
int g_IntervalInfo;
int g_IntervalHbeat;

CHRONO *g_LstChrono = NULL;
PARAM *g_LstParam = NULL;
char FichierCnf[256];


/****************************************************************************************************************/
/* Initialisation du module chrono																			*/
int Chrono_init()
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
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalHbeat", i_tmp)==1) g_IntervalHbeat = atoi(i_tmp);
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalInfo", i_tmp)==1) g_IntervalInfo = atoi(i_tmp);

	if(g_IntervalHbeat==0) g_IntervalHbeat = 60;
	if(g_IntervalInfo==0) g_IntervalInfo = 60;

	if(Fcnf_Valeur(g_LstParam, "CHRONO_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);
}

/****************************************************************************************************************/
/* Lire les	Chrono																							*/
int Chrono_LireIni()
{
	int		nSection;
	int		i;
	int		No;
	char	Cle[128];
	char	Section[128];
	char	Tmp[128];
	char	*Ptr;
	CHRONO	*Chrono;
	

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Parcours des sections du fichier de conf.");

	//Initialisation
	while(g_LstChrono != NULL)
	{
		Chrono = g_LstChrono;
		g_LstChrono = Chrono->Suivant;
		free(Chrono);
	}
	Chrono = NULL;

	//Parcourir les sections
	nSection=1;
	No = 1;
	while(Fcnf_Section(g_LstParam, nSection, Section))
	{
		nSection++;

		//Sauter les sections de paramétrage
		if(STRICMP(Section, "XAP")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section XAP ignorée.");
			continue;
		}
		if(STRICMP(Section, "CHRONO")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section CHRONO ignorée.");
			continue;
		}

		Chrono = malloc(sizeof(CHRONO));
		if(Chrono==NULL) return TRUE;

		strcpy(Chrono->Nom, Section);
		Chrono->No = No++;
		Chrono->Etat = ETAT_NC;
		Chrono->Duree = 0;
		Chrono->RazUnite = '\0';
		Chrono->RazMultiple = 0;
		Chrono->RazHeure = 0;
		Chrono->RazMinute = 0;
		Chrono->Unite = 's';
		Chrono->Time = time((time_t*)0);
		Chrono->TimeRaz = time((time_t*)0);
		Chrono->bMemo = FALSE;

		sprintf(Cle, "%s_Entree", Section);
		Fcnf_Valeur(g_LstParam, Cle, Chrono->Entree);

		sprintf(Cle, "%s_Unite", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Tmp)) Chrono->Unite = Tmp[0];

		sprintf(Cle, "%s_Raz", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Tmp))
		{
			i = strlen(Tmp);
			Chrono->RazUnite = Tmp[i-1];
			Tmp[i-1] = '\0';
			Chrono->RazMultiple = atoi(Tmp);
		}

		sprintf(Cle, "%s_RazHeure", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Tmp))
		{
			Ptr = strchr(Tmp, ':');
			if(Ptr==NULL)
			{
				Flog_Ecrire("Le paramètre RazHeure est mal formaté, absence de ':', valeur lu %s.", Tmp);
			}
			else
			{
				Chrono->RazHeure = atoi(Tmp);
				Chrono->RazMinute = atoi(Ptr);
			}
		}

		sprintf(Cle, "%s_Memo", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Tmp)) 
			if((Tmp[0]=='O')||(Tmp[0]=='Y')) Chrono->bMemo = TRUE;

		ChronoRazInit(Chrono);

		Chrono->Suivant = g_LstChrono;
		g_LstChrono = Chrono;

		if (g_debuglevel>=DEBUG_VERBOSE)
		{
			strcpy(Tmp, "non");
			if(Chrono->bMemo) strcpy(Tmp, "oui");
			Flog_Ecrire("CHRONO %s", Chrono->Nom);
			Flog_Ecrire("      Entrée : %s", Chrono->Entree);
			Flog_Ecrire("      Mémo : %s, Unité : %c", Tmp, Chrono->Unite);
			Flog_Ecrire("      Remise à 0 : %d %c", Chrono->RazMultiple, Chrono->RazUnite);
			if(Chrono->TimeRaz!=0) Flog_Ecrire("      Prochaine RAZ : %s", ctime(&Chrono->TimeRaz));
		}
	}

	//Pour mémoriser l'état des entrées au démarrage du module
	Chrono = g_LstChrono;
	while(Chrono != NULL)
	{
		xpp_query(Chrono->Entree);
		Chrono = Chrono->Suivant;
	}

	return TRUE;
}

/****************************************************************************************************************/
/* Recherche dans la liste des chronos																		*/
CHRONO *Chrono_RechercheParNom(char *Nom)
{
	CHRONO	*Chrono;

	Chrono = g_LstChrono;
	while(Chrono != NULL)
	{
		if(STRICMP(Chrono->Nom, Nom)==0)	return Chrono;
		Chrono = Chrono->Suivant;
	}
	return NULL;
}

CHRONO *Chrono_Recherche(char *Capteur)
{
	CHRONO	*Chrono;

	Chrono = g_LstChrono;
	while(Chrono != NULL)
	{
		if(STRICMP(Chrono->Entree, Capteur)==0)	return Chrono;
		Chrono = Chrono->Suivant;
	}
	return NULL;
}

CHRONO *Chrono_RechercheNext(char *Capteur, CHRONO *Debut)
{
	CHRONO	*Chrono;

	Chrono = Debut->Suivant;
	while(Chrono != NULL)
	{
		if(STRICMP(Chrono->Entree, Capteur)==0)	return Chrono;
		Chrono = Chrono->Suivant;
	}
	return NULL;
}

/****************************************************************************************************************/
/* Arrondir la valeur avant affichage																			*/
double Chrono_Arrondir(double Duree)
{
	double d;

	d = Duree + 0.05;
	return floor(d*10)/10;
}

/****************************************************************************************************************/
/* Gestion des RAZ de chrono																				*/
int ChronoRazInit(CHRONO *Chrono)
{
	struct tm *t;
	float n;

	t = gmtime(&Chrono->TimeRaz);

	switch(Chrono->RazUnite)
	{
		case 'm':
			t->tm_sec = 0;
			n = (float) t->tm_min;
			n/= Chrono->RazMultiple;
			n++;
			t->tm_min = Chrono->RazMultiple*(int)n;
			break;
		case 'h': 
			t->tm_sec = 0;
			t->tm_min = Chrono->RazMinute;
			n = (float) t->tm_hour;
			n/= Chrono->RazMultiple;
			n++;
			t->tm_hour = Chrono->RazMultiple*(int)n;
			break;
		case 'J': 
			t->tm_sec = 0;
			t->tm_min = Chrono->RazMinute;
			t->tm_hour= Chrono->RazHeure;
			n = (float) t->tm_yday;
			n/= Chrono->RazMultiple;
			n++;
			t->tm_yday = Chrono->RazMultiple*(int)n;
			if(t->tm_yday>365)
			{
				t->tm_yday -= 365;
				t->tm_year++;
			}
			break;
		case 'M':
			t->tm_sec = 0;
			t->tm_min = Chrono->RazMinute;
			t->tm_hour= Chrono->RazHeure;
			t->tm_mday= 0;
			n = (float) t->tm_mon;
			n/= Chrono->RazMultiple;
			n++;
			t->tm_mon = Chrono->RazMultiple*(int)n;
			if(t->tm_mon>12)
			{
				t->tm_mon -= 12;
				t->tm_year++;
			}
			break;
		case 'A': 
			t->tm_sec = 0;
			t->tm_min = Chrono->RazMinute;
			t->tm_hour= Chrono->RazHeure;
			t->tm_mday= 0;
			t->tm_mon = 0;
			n = (float) t->tm_year;
			n/= Chrono->RazMultiple;
			n++;
			t->tm_year = Chrono->RazMultiple*(int)n;
			break;
	}

	Chrono->TimeRaz = mktime(t);
	ChronoRazNext(Chrono);

	return TRUE;
}

int ChronoRazNext(CHRONO *Chrono)
{
	struct tm *t;

	switch(Chrono->RazUnite)
	{
		case 'm': 
			Chrono->TimeRaz += Chrono->RazMultiple*60;
			break;
		case 'h': 
			Chrono->TimeRaz += Chrono->RazMultiple*60*60;
			break;
		case 'J': 
			Chrono->TimeRaz += Chrono->RazMultiple*60*60*24;
			break;
		case 'M':
			t = gmtime(&Chrono->TimeRaz);
			t->tm_mon += Chrono->RazMultiple;
			if(t->tm_mon>12)
			{
				t->tm_mon -= 12;
				t->tm_year++;
			}
			Chrono->TimeRaz = mktime(t);
			break;
		case 'A': 
			t = gmtime(&Chrono->TimeRaz);
			t->tm_year += Chrono->RazMultiple;
			Chrono->TimeRaz = mktime(t);
			break;
	}
	return TRUE;
}

int ChronoTraite(CHRONO *Chrono, int Etat)
{
	int		Old_Etat;
	time_t	Time;
	double	Duree;
	double	DureeOld;


	//Identifier l'état
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Traitement du chrono %s état av %d, état ap %d).",Chrono->Nom, Chrono->Etat, Etat);
	Old_Etat = Chrono->Etat;
	Chrono->Etat = Etat;
	Time = Chrono->Time;
	Chrono->Time = time(NULL);

	//Traiter le RAZ
	if((Chrono->RazUnite != '\0')&&(difftime(Chrono->Time, Chrono->TimeRaz)>0))
	{
		Chrono->Duree = 0;
		ChronoRazNext(Chrono);
	}

	//Cumuler le temp
	if(Old_Etat==ETAT_ON)
	{
		DureeOld = Chrono_Arrondir(Chrono->Duree);
		Duree = difftime(Chrono->Time, Time);
		switch(Chrono->Unite)
		{
			case 'm' : 
				Duree = Duree/60;
				break;
			case 'h' : 
				Duree = Duree/3600;
				break;
		}
		Chrono->Duree += Duree;
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le chrono %s passe à %f %c.",Chrono->Nom, Chrono->Duree, Chrono->Unite);
		Duree = Chrono_Arrondir(Chrono->Duree);
		if(DureeOld!=Duree)
			xpp_event(Chrono->No, Chrono->Nom, -1, 0, Duree);
	}
}

/****************************************************************************************************************/
/* Traitement d'un message																						*/
int xpp_handler_service()
{
	char i_temp[64];;


	if (xpp_GetCmd("request:state", i_temp)!=0)
	{
		if (STRICMP(i_temp, "stop")==0) g_bServiceStop = TRUE;
		if (STRICMP(i_temp, "start")==0) g_bServicePause = FALSE;
		if (STRICMP(i_temp, "pause")==0) g_bServicePause = TRUE;
		return 0;
	}
	if (xpp_GetCmd("request:init", i_temp)!=0)
	{
		if (STRICMP(i_temp, "config")==0)
		{
			Fcnf_Lire(FichierCnf, &g_LstParam);
			Chrono_LireIni();
		}
		return 0;
	}

	return 1;
}

int xpp_handler_Chrono(int i_TypMsg)
{
	CHRONO	*Chrono;
	BOOL bSauve;
	char i_source[128];
	char i_temp[64];
	int	Etat;


	//Rechercher dans la liste des chrono
	if (xpp_GetSourceName(i_source)==-1) return 0;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche dans la liste des Chronos de %s.",i_source);
	Chrono = Chrono_Recherche(i_source);
	if(Chrono == NULL) return 0;

	//Lire la valeur
	if (xpp_GetTargetText(i_temp)==-1) return 0;
	if(atof(i_temp)==0) xpp_GetTargetState(i_temp);
	if(STRICMP(i_temp, "ON")==0) Etat = ETAT_ON;
	if(STRICMP(i_temp, "OFF")==0) Etat = ETAT_OFF;

	//Traiter tous les chronos
	bSauve = FALSE;
	while(Chrono!=NULL)
	{
		ChronoTraite(Chrono, Etat);
		if((Chrono->bMemo==TRUE)&&(Chrono->Etat==ETAT_ON)) bSauve=TRUE;
		Chrono=Chrono_RechercheNext(i_source,Chrono);
	}
	if(bSauve==TRUE) ValeursSauve();

	return 1;
}

int xpp_handler_QueryChrono()
{
	char i_temp[128];
	char *ptr;
	int No;
	CHRONO *Chrono;
	CHRONO *Chroni;


	Chrono = NULL;

	//L'id est dans le bloc output.state.* ?
	No = xpp_GetTargetId();
	if(No!=-1)
	{
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Id chrono output.state.1:id = %d.", No);
		//Conversion No -> Id
		for(Chroni=g_LstChrono; Chroni!=NULL; Chroni = Chroni->Suivant)
		{
			if(Chroni->No == No) Chrono = Chroni;
		}
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Chrono == NULL)) Flog_Ecrire("Impossible de trouver le nom du chrono avec son ID (%d).", No);
	}

	//L'id est au delà du : dans target ?
	if( (Chrono == NULL) && (xpp_GetTargetName(i_temp)!=-1) )
	{
		ptr = strchr(i_temp, ':');
		if(ptr!=NULL) ptr++;
		Chrono = Chrono_RechercheParNom(ptr);
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Chrono == NULL)) Flog_Ecrire("Nom du chrono trouvé dans target: %s.", ptr);
	}

	if(Chrono == NULL)
	{
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Chrono == NULL)) Flog_Ecrire("Impossible de trouver le nom du chrono.");
		return 0;
	}

	//Envoyer l'état demandé
	if(xpp_GetSourceName(i_temp)!=-1)
		xpp_info(Chrono->No, Chrono->Nom, -1, 0, Chrono_Arrondir(Chrono->Duree), i_temp);
	else
		xpp_info(Chrono->No, Chrono->Nom, -1, 0, Chrono_Arrondir(Chrono->Duree), NULL);

	return 0;
}

int Chrono_BSCInfo(int interval, CHRONO *LstChrono) 
{
	time_t i_timenow;
	static time_t i_intervaltick=0;
	double	Duree;
	CHRONO *Chrono;


	i_timenow = time((time_t*)0);
	if ((i_timenow-i_intervaltick<interval)&&(i_intervaltick!=0)) return 0;

	i_intervaltick=i_timenow;

	//Parcours des capteurs et envoi du message l'info
	for(Chrono=LstChrono; Chrono!=NULL; Chrono = Chrono->Suivant)
	{
		Duree = Chrono->Duree*10;
		Duree = ceil(Duree);
		Duree = Duree/10;
		xpp_info(Chrono->No, Chrono->Nom, -1, 0, Duree, NULL);
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
	CHRONO *Chrono;


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

		Chrono = Chrono_RechercheParNom(Nom);
		if(Chrono==NULL)
		{
			if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Le Chrono %s est introuvable.", Nom);
			continue;
		}

		if(Chrono->bMemo == TRUE)
		{
			Chrono->Duree = atof(Valeur);
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Nouvelle valeur de %s : %s.", Nom, Valeur);
		}
		else
		{
			if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Le chrono %s est trouvé, mais il n'est pas à mémoriser.", Nom);
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
	CHRONO *Chrono;
	int err;


	FichierStd(Fichier, TYPFIC_DATA);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Enregistrement des valeurs à mémoriser dans %s.", Fichier);

	//Parcourir le fichier
    if((hFic = fopen(Fichier, "w")) == NULL)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible d'ouvrir le fichier des valeurs mémorisées (%s).", Fichier);
		return FALSE;
	}

	for(Chrono=g_LstChrono; Chrono!=NULL; Chrono = Chrono->Suivant)
	{
		if(Chrono->bMemo == TRUE)
		{
			sprintf(Ligne, "%s=%f", Chrono->Nom, Chrono->Duree);
			err = fputs(Ligne, hFic);
			if ((g_debuglevel>=DEBUG_INFO)&&(err<0)) Flog_Ecrire("Impossible d'écrire dans le fichier des valeurs mémorisées (erreur %d).", err);
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Enregistrement de %s.", Ligne);
		}
	}
	fclose(hFic);

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de l'enregistrement des valeurs à mémoriser.");
	return TRUE;
}

/****************************************************************************************************************/
/* Fonctions services : start																					*/
int ServiceStart()
{
	fd_set i_rdfs;
	struct timeval i_tv;
	char i_xpp_buff[1500+1];

	char Fichier[256];

float tst;

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
	g_receiver_sockfd = Chrono_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);
	Chrono_LireIni();

	//******************************************************************************************************************
	//*** Chargement des valeurs mémorisées
	ValeursCharge();

	//******************************************************************************************************************
	//*** Boucle principale
	while(!g_bServiceStop) 
	{ 
		// Send heartbeat periodically
		xpp_heartbeat_tick(g_IntervalHbeat);
		
		// Envoi la liste complète des chronos regulièrement
		Chrono_BSCInfo(g_IntervalInfo, g_LstChrono);

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
					case XPP_RECEP_CAPTEUR_INFO :
						if(!g_bServicePause) xpp_handler_Chrono(XPP_RECEP_CAPTEUR_INFO);
						break;
					case XPP_RECEP_CAPTEUR_EVENT :
						if(!g_bServicePause) xpp_handler_Chrono(XPP_RECEP_CAPTEUR_EVENT);
						break;
					case XPP_RECEP_CAPTEUR_QUERY :
						if(!g_bServicePause) xpp_handler_QueryChrono();
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
