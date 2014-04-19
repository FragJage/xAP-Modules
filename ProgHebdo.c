#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include <time.h>
#include "Service.h"
#include "Fichier.h"
#include "xpp.h"
#include "ProgHebdo.h"

#ifdef WIN32
#define close closesocket
#endif

int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_receiver_sockfd;
int g_IntervalHbeat;

PARAM *g_LstParam = NULL;
PRGHBDO *g_LstPrgHdbo = NULL;
char FichierCnf[256];


/****************************************************************************************************************/
/* Initialisation du module ProgHebdo																			*/
int ProgHebdo_init()
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
	if(Fcnf_Valeur(g_LstParam,	"ProgHebdo_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);		//0
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalHbeat", i_tmp)==1) g_IntervalHbeat = atoi(i_tmp);

	if(g_IntervalHbeat==0) g_IntervalHbeat = 60;

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);
}

/****************************************************************************************************************/
/* Fonctions de gestion des plages																				*/
int Plage_Jour2Num(char *Jour)
{
	if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Jour vers Num %s.",Jour);

	if(STRICMP(Jour, "Lu") == 0) return 1;
	if(STRICMP(Jour, "Ma") == 0) return 2;
	if(STRICMP(Jour, "Me") == 0) return 3;
	if(STRICMP(Jour, "Je") == 0) return 4;
	if(STRICMP(Jour, "Ve") == 0) return 5;
	if(STRICMP(Jour, "Sa") == 0) return 6;
	if(STRICMP(Jour, "Di") == 0) return 7;

	Flog_Ecrire("Jour inconnu %s (Jours possibles : Lu, Ma, Me, Je, Ve, Sa, Di).", Jour);
	return 0;
}

int Plage_Heure2Num(char *Heure)
{
	int h,m;

	if(Heure[2]=!':')
	{
		Flog_Ecrire("Format d'heure incorrect %s (attendu HH:MM).", Heure);
		return -1;
	}

	h = (Heure[0]-'0')*10+(Heure[1]-'0');
	m = (Heure[3]-'0')*10+(Heure[4]-'0');

	return h*100+m;
}

int Plage_ParseJoursPlage(char *PlageJour, PLAGE *stPlage, int Ind)
{
	if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Parse d'une plage de jours %s.",PlageJour);

	char *Pos;
	int i, j1, j2;

	Pos = strchr(PlageJour, '-');
	if(Pos==NULL) return Ind;

	*Pos='\0';
	j1 = Plage_Jour2Num(PlageJour);
	j2 = Plage_Jour2Num(Pos+1);


	if(j1==0)
	{
		Flog_Ecrire("Jour début %s inconnu dans une plage.", PlageJour);
		return Ind;
	}
	if(j2==0)
	{
		Flog_Ecrire("Jour fin %s inconnu dans une plage.", Pos+1);
		return Ind;
	}

	if(j1>j2) { i=j1; j1=j2; j2=i; }
	for(i=j1; i<=j2; i++) stPlage->Jours[++Ind] = i;

	return Ind;
}

int Plage_ParseJours(char *Jours, PLAGE *stPlage, int Ind)
{
	// Ex : Lu,Ma-Je,Ve ou Lu-Ve ou *
	char *Pos, *Jour, Semaine[]="Lu-Di";
	int	 bFin;
	int j;

	if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Parse des jours %s.",Jours);

	bFin = FALSE;
	Jour = Jours;
	do
	{
		Pos = strchr(Jour, ',');
		if(Pos!=NULL)
			*Pos = '\0';
		else
			bFin = TRUE;

		if(Jour[0] == '*') return Plage_ParseJours(Semaine, stPlage, 0);
		
		if(strchr(Jour, '-'))
			Ind = Plage_ParseJoursPlage(Jour, stPlage, Ind);
		else
		{
			j = Plage_Jour2Num(Jour);
			if(j!=0) stPlage->Jours[++Ind] = j;
		}

		Jour = Pos+1;
	} while(!bFin);

	return Ind;
}

HORAIRE *Plage_ParseHorairePlage(char *Horaire)
{
	HORAIRE	*stHoraire;
	int		Deb,Fin;

	// Ex : 07:00-08:00 ou 20:30-21:30
	if((Horaire[2]!=':')||(Horaire[5]!='-')||(Horaire[8]!=':'))
	{
		Flog_Ecrire("Format de plage d'heure incorrect %s (attendu HH:MM-HH:MM).", Horaire);
		return NULL;
	}

	Horaire[5]='\0';
	Deb = Plage_Heure2Num(Horaire);
	Fin = Plage_Heure2Num(Horaire+6);

	if( (Deb==-1) || (Fin==-1) ) return NULL;

	//Créer l'horaire
	stHoraire = malloc(sizeof(HORAIRE));
	if(stHoraire==NULL)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour stocker les plages d'horaires.");
		return NULL;
	}

	stHoraire->Debut = Deb;
	stHoraire->Fin = Fin;
	stHoraire->Suivant = NULL;

	return stHoraire;
}

HORAIRE *Plage_ParseHoraires(char *Horaires)
{
	// Ex : 07:00-08:00,20:30-21:30
	char	*Pos, *Horaire;
	int		bFin;
	HORAIRE	*stHoraire;
	HORAIRE	*LstHoraire=NULL;

	bFin = FALSE;
	Horaire = Horaires;
	do
	{
		Pos = strchr(Horaire, ',');
		if(Pos!=NULL)
			*Pos = '\0';
		else
			bFin = TRUE;

		stHoraire = Plage_ParseHorairePlage(Horaire);
		if(stHoraire!=NULL)
		{
			stHoraire->Suivant = LstHoraire;
			LstHoraire = stHoraire;
		}

		Horaire = Pos+1;
	} while(!bFin);

	return LstHoraire;
}

/****************************************************************************************************************/
/* Fonction principale de gestion des plages																	*/
/* Ex : Plage1=Lu,Ma,Me,Je,Ve;07:00-08:00,20:30-21:30;22														*/
/*		Plage2=Lu-Ve;08:00-20:30;19																				*/
/*		Plage3=Sa,Di;08:00-09:00,21:00-22:00;22																	*/
/*		Plage4=Sa,Di;09:00-21:00;19																				*/
/*		Plage5=*;09:00-21:00;20																					*/
int Plage_Parse(char *chPlage, PRGHBDO *PrgHbdo)
{
	char *pos;
	char *Jours, *Horaires, *Consigne;
	PLAGE *stPlage;

	if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Parse de la plage %s.",chPlage);

	//Isoler les jours
	Jours = chPlage;
	pos = strchr(Jours, ';');
	if(pos==NULL)
	{
		Flog_Ecrire("La plage %s ne contient pas de jours (absence de ';')", chPlage);
		return FALSE;
	}
	*pos = '\0';

	//Isoler les Horaires
	Horaires = pos+1;
	pos = strchr(Horaires, ';');
	if(pos==NULL)
	{
		Flog_Ecrire("La plage %s ne contient pas d'horaires (absence de ';')", chPlage);
		return FALSE;
	}
	*pos = '\0';

	//Isoler la consigne
	Consigne = pos+1;

	//Créer la plage
	stPlage = malloc(sizeof(PLAGE));
	if(stPlage==NULL)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour stocker les plages.");
		return FALSE;
	}

	stPlage->Suivant = PrgHbdo->Plages;
	PrgHbdo->Plages = stPlage;

	stPlage->Valeur = atof(Consigne);
	if(STRICMP(Consigne, "ON") == 0) stPlage->Valeur = 1; //atof("OFF") donne déjà 0
	Plage_ParseJours(Jours, stPlage, 0);
	stPlage->Horaire = Plage_ParseHoraires(Horaires);

	return TRUE;
}

/****************************************************************************************************************/
/* Lire le paramétrage des plages dans le fichier de config														*/
int ProgHebdo_LireIni()
{
	int		i,j,No;
	char	Cle[128];
	char	Section[128];
	char	Temp[256];
	PRGHBDO	*PrgHdbo;
	

	if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Parcours des sections du fichier de conf.");

	//Faire le ménage
	while(g_LstPrgHdbo != NULL)
	{
		PrgHdbo = g_LstPrgHdbo;
		g_LstPrgHdbo = PrgHdbo->Suivant;
		free(PrgHdbo);
	}

	//Parcourir les sections
	i=1;
	No=1;
	while(Fcnf_Section(g_LstParam, i, Section))
	{
		//Sauter la section XAP et ProgHebo
		if(STRICMP(Section, "XAP")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section XAP ignorée.");
			i++;
			continue;
		}
		if(STRICMP(Section, "ProgHebdo")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section ProgHebdo ignorée.");
			i++;
			continue;
		}

		//Section valide ?
		sprintf(Cle, "%s_Sortie", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Temp))
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Section %s ignorée car aucune sortie n'est défini.", Section);
			i++;
			continue;
		}

		//Allocation structure
		PrgHdbo = malloc(sizeof(PRGHBDO));
		if(PrgHdbo==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour stocker les programmations hebdomadaires.");
			return FALSE;
		}

		//Lecture des paramètres
		strcpy(PrgHdbo->Nom, Section);
		strcpy(PrgHdbo->Sortie, Temp);
		PrgHdbo->Plages = NULL;
		PrgHdbo->bInit = FALSE;
		PrgHdbo->No = No;
		No++;

		sprintf(Cle, "%s_Defaut", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Temp))
		{
			PrgHdbo->Defaut = atof(Temp);
			if(STRICMP(Temp, "ON") == 0) PrgHdbo->Defaut = 1; //atof("OFF") donne déjà 0
		}
		else
			PrgHdbo->Defaut = 0;

		//Lecture des plages
		j=1;
		do
		{
			sprintf(Cle, "%s_Plage%d", Section, j);
			if(Fcnf_Valeur(g_LstParam, Cle, Temp))
			{
				if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Lecture de la plage %s.",Cle);
				Plage_Parse(Temp, PrgHdbo);
			}
			else
			{
				j=-1;
			}
			j++;
		} while(j!=0);

		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Programmation %s : %s = %f par défaut", PrgHdbo->Nom, PrgHdbo->Sortie, PrgHdbo->Defaut);

		PrgHdbo->Suivant = g_LstPrgHdbo;
		g_LstPrgHdbo = PrgHdbo;

		i++;
	}

	if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Fin de parcours des sections du fichier de conf.");
	return TRUE;
}

/****************************************************************************************************************/
/* Déclenchement																								*/
PLAGE *CherchePlage(int Jour, int Heure, PLAGE *Plages)
{
	int		i, bOK;
	PLAGE	*Plage;
	HORAIRE	*Horaire;


	Plage = Plages;

	while(Plage!=NULL)
	{
		bOK = FALSE;
		for(i=0; i<8; i++) if(Plage->Jours[i]==Jour)
		{
			bOK=TRUE;
			i=8;
		}
		if(bOK==FALSE)
		{
			Plage = Plage->Suivant;
			continue;
		}
		Horaire = Plage->Horaire;
		while(Horaire != NULL)
		{
			if( (Horaire->Debut <= Heure) && (Horaire->Fin >= Heure) ) return Plage;
			Horaire = Horaire->Suivant;
		}
		Plage = Plage->Suivant;
	}
	return NULL;
}

int ProgHebdo()
{
	time_t timestamp;
    struct tm * t;
	int Jour;
	int Heure;
	double Valeur;
	PRGHBDO	*PrgHdbo;
	PLAGE	*Plage;
	char	tState[8];
	char	tValeur[16];


	if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Début de traitement des programmations.");

	//Calcul Jour et heure actuel
    timestamp = time(NULL);
    t = localtime(&timestamp);

	Jour = t->tm_wday; //0->Di à 6->Sa à convertir vers 1->Lu à 7->Di
	if(Jour==0) Jour = 7;
	Heure = t->tm_hour*100+t->tm_min;
    
    //Parcourir les plages
	PrgHdbo = g_LstPrgHdbo;
	while(PrgHdbo != NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Traitement de %s.",PrgHdbo->Nom);
		Plage = CherchePlage(Jour, Heure, PrgHdbo->Plages);
		if(Plage!=NULL)
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Une plage trouvée, valeur %f.",Plage->Valeur);
			Valeur = Plage->Valeur;
		}
		else
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas de plage trouvée, valeur par défaut %f.",PrgHdbo->Defaut);
			Valeur = PrgHdbo->Defaut;
		}

		if((PrgHdbo->bInit==FALSE)||(PrgHdbo->Valeur!=Valeur))
		{
			PrgHdbo->bInit=TRUE;
			PrgHdbo->Valeur=Valeur;
			strcpy(tState, "OFF");
			if(Valeur>0) strcpy(tState, "ON");
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fixe le capteur %s à %s (%s)",PrgHdbo->Sortie, tValeur, tState);
			sprintf(tValeur, "%f", Valeur);
			xpp_cmd(PrgHdbo->Sortie, tState, "", tValeur);
		}

		PrgHdbo = PrgHdbo->Suivant;
	}

	if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Fin de traitement des programmations.");
    return 0;
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
			ProgHebdo_LireIni();
		}
		return 0;
	}

	return 1;
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
	g_receiver_sockfd = ProgHebdo_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);

	//******************************************************************************************************************
	//*** Lecture des plage
	ProgHebdo_LireIni();

	//******************************************************************************************************************
	//*** Boucle principale
	while(!g_bServiceStop) 
	{ 
		// Send heartbeat periodically
		xpp_heartbeat_tick(g_IntervalHbeat);
		
		ProgHebdo();

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
