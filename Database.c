#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include <time.h>
#include "xpp.h"
#include "Database.h"
#include "Service.h"
#include "Fichier.h"
#include "DBcache.h"

int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_IntervalHbeat;
int g_receiver_sockfd;

FILTRE *g_LstFiltre = NULL;
PARAM *g_LstParam = NULL;
int g_TypeDB = 1;
char FichierCnf[256];


/****************************************************************************************************************/
/* Filtrer les capteurs et classe grâce à la liste de filtres													*/
FILTRE *Filtres_Chercher(FILTRE *LstFiltre, char *i_Source, char*i_Classe)
{
	int		bFin;
	FILTRE	*Filtre;	


	if(LstFiltre==NULL) return NULL;

	//Initialisation
	bFin = FALSE;
	Filtre = LstFiltre;

	while(bFin==FALSE)
	{
		bFin = TRUE;
		if(Filtre->Source != '\0') bFin = xpp_compare(i_Source, Filtre->Source);
		if((bFin==TRUE)&&(Filtre->Classe != '\0')) bFin = xpp_compare(i_Classe, Filtre->Classe);
		
		if(bFin==FALSE)
		{
			Filtre = Filtre->Suivant;
			if(Filtre==NULL) bFin = TRUE;
		}
	}

	return Filtre;
}

/****************************************************************************************/
/* Ouvrir la base de données															*/
int DB_OuvrirConnexion()
{
	switch(g_TypeDB)
	{
		case 1 : return DBmysql_Connexion();
	}
	return 0;
}

/****************************************************************************************/
/* Fermer la base de données															*/
int DB_FermerConnexion()
{
	switch(g_TypeDB)
	{
		case 1 : return DBmysql_Deconnexion();
	}
	return 0;
}


/****************************************************************************************/
/* Enregistrer les infos XAP															*/
int DB_EnregistrerValeur(char *Source, char *Valeur)
{
	switch(g_TypeDB)
	{
		case 0 : 
			printf("%s : %s\n", Source, Valeur);
			return 1;
		case 1 : 
			return DBCache_Ajoute(Source, Valeur, '\0');
	}
	return 0;
}

/****************************************************************************************/
/* Enregistrer les infos XAP en mode période											*/
int DB_EnregistrerValeurPeriode(char *Source, char *Valeur, char Periode)
{
	switch(g_TypeDB)
	{
		case 0 : 
			printf("%s : %s\n", Source, Valeur);
			return 1;
		case 1 : 
			return DBCache_Ajoute(Source, Valeur, Periode);
	}
	return 0;
}

/****************************************************************************************/
/* Lire une info XAP en mode période											*/
int DB_LireValeurPeriode(char *Source, char *Valeur, char Periode)
{
	switch(g_TypeDB)
	{
		case 0 : 
			printf("%s : %s\n", Source, Valeur);
			return 1;
		case 1 : 
			return DBmysql_LireValeurPeriode(Source, Valeur, Periode);
	}
	return 0;
}

/****************************************************************************************/
/* Supprimer une info XAP															*/
int DB_SupprimerValeur(char *Source, char *Date)
{
	switch(g_TypeDB)
	{
		case 1 : return DBmysql_SupprimerValeur(Source, Date);
	}
	return 0;
}

/****************************************************************************************************************/
/* Traitement d'un message																						*/
int xpp_handler_service()
{
	char i_temp[64];


	if (xpp_GetCmd("request:state", i_temp)!=0)
	{
		if (STRICMP(i_temp, "stop")==1) g_bServiceStop = TRUE;
		if (STRICMP(i_temp, "start")==1) g_bServicePause = FALSE;
		if (STRICMP(i_temp, "pause")==1) g_bServicePause = TRUE;
		if (STRICMP(i_temp, "RazCache")==1)
		{
			DBCache_VidageMem();
			DBCache_VidageFic();
		}
		return 0;
	}
	if (xpp_GetCmd("request:init", i_temp)!=0)
	{
		if (STRICMP(i_temp, "config")==1) 
		{
			DB_FermerConnexion();
			Database_LireIni();
			DB_OuvrirConnexion();
		}
		return 0;
	}

	return 1;
}

int xpp_handler_Database(int i_TypMsg)
{
	FILTRE	*Filtre;	
	int Ret;
	float Val;
	char i_source[XPP_MAX_KEYVALUE_LEN];
	char i_class[XPP_MAX_KEYVALUE_LEN];
	char i_temp[XPP_MAX_KEYVALUE_LEN];


	//Lire la source et la classe
	if (xpp_GetSourceName(i_source)==-1) return 0;
	if (xpp_GetClassName(i_class)==0) return 0;

	//Filtre
	Filtre = Filtres_Chercher(g_LstFiltre, i_source, i_class);
	if(Filtre==NULL)
	{
		if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Aucun filtre pour ce message.");
		return 0;
	}
	if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Le filtre %s correspond au message (%s-%s).", Filtre->Nom, Filtre->Source, Filtre->Classe);

	//Lire la valeur du capteur
	switch(Filtre->TypeValeur)
	{
		case TYPVAL_TXT :
			Ret = xpp_GetTargetText(i_temp);
			break;
		case TYPVAL_STA :
			Ret = xpp_GetTargetState(i_temp);
			break;
	}
	if (Ret==0)
	{
		Flog_Ecrire("Impossible de lire la valeur du filtre %s.",Filtre->Nom);
		return 0;
	}
	if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Valeur de %s : %s", i_source, i_temp);
	Val = atof(i_temp);

	//Remise à zéro du mini/maxi
	if((Filtre->Mode==MODE_MAX)||(Filtre->Mode==MODE_MIN))
	{
		if(difftime(time(NULL), Filtre->TimeRaz)>0)
		{
			switch(Filtre->Mode)
			{
				case MODE_MAX : 
					Filtre->Memo = -999999;
					break;
				case MODE_MIN : 
					Filtre->Memo = 999999;
					break;
			}
			Database_RazInit(Filtre);
			if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Prochaine RAZ : %s", ctime(&Filtre->TimeRaz));
		}
	}

	//Enregistrement de la valeur
	switch(Filtre->Mode)
	{
		case MODE_MAX :
			if(Filtre->Memo>Val)
			{
				if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Pas de sauvegarde le max est de %f", Filtre->Memo);
				return 1;
			}
			strcat(i_source, "-MAX");
			DB_OuvrirConnexion();
			DB_EnregistrerValeurPeriode(i_source, i_temp, Filtre->Periode);
			DB_FermerConnexion();
			Filtre->Memo = Val;
			break;

		case MODE_MIN :
			if(Filtre->Memo<Val)
			{
				if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Pas de sauvegarde le min est de %f", Filtre->Memo);
				return 1;
			}
			strcat(i_source, "-MIN");
			DB_OuvrirConnexion();
			DB_EnregistrerValeurPeriode(i_source, i_temp, Filtre->Periode);
			DB_FermerConnexion();
			Filtre->Memo = Val;
			break;

		case MODE_ETAT :
			DB_OuvrirConnexion();
			if(Filtre->Mode == MODE_ETAT) DB_SupprimerValeur(i_source, NULL);
			DB_EnregistrerValeur(i_source, i_temp);
			DB_FermerConnexion();
			break;

		case MODE_LOG :
			DB_OuvrirConnexion();
			DB_EnregistrerValeur(i_source, i_temp);
			DB_FermerConnexion();
			break;
	}

	return 1;
}

/****************************************************************************************************************/
/* Database_init																					*/
int Database_init()
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
	g_debuglevel = 0;

	if(Fcnf_Valeur(g_LstParam, "XAP_Port", i_tmp)==1) i_interfaceport = atoi(i_tmp);	//3639
	if(Fcnf_Valeur(g_LstParam, "XAP_Debug", i_tmp)==1) i_debuglevel = atoi(i_tmp);	//0
	Fcnf_Valeur(g_LstParam, "XAP_UID", i_uniqueID);									//XAP_GUID
	Fcnf_Valeur(g_LstParam, "XAP_Instance", i_instance);								//XAP_DEFAULT_INSTANCE
	Fcnf_Valeur(g_LstParam, "XAP_Interface", i_interfacename);						//eth0
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalHbeat", i_tmp)==1) g_IntervalHbeat = atoi(i_tmp);
	if(g_IntervalHbeat==0) g_IntervalHbeat = 60;

	if(Fcnf_Valeur(g_LstParam,	"DATABASE_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);		//0

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);
}

/****************************************************************************************************************/
/* Calcul l'heure de RAZ des MIN/MAX																			*/
int Database_RazInit(FILTRE *Filtre)
{
	time_t t1;
	struct tm *t2;
	float n;

	time(&t1);
	t2 = gmtime(&t1);

	switch(Filtre->Periode)
	{
		case 'm':
			t2->tm_sec = 0;
			t2->tm_min++;
			break;
		case 'h': 
			t2->tm_sec = 0;
			t2->tm_min = 0;
			t2->tm_hour++;
			break;
		case 'J': 
			t2->tm_sec = 0;
			t2->tm_min = 0;
			t2->tm_hour= 0;
			t2->tm_mday++;
			t2->tm_yday++;
			if(t2->tm_yday>365)
			{
				t2->tm_yday -= 365;
				t2->tm_year++;
			}
			break;
		case 'M':
			t2->tm_sec = 0;
			t2->tm_min = 0;
			t2->tm_hour= 0;
			t2->tm_mday= 0;
			t2->tm_mon++;
			if(t2->tm_mon>12)
			{
				t2->tm_mon -= 12;
				t2->tm_year++;
			}
			break;
		case 'A': 
			t2->tm_sec = 0;
			t2->tm_min = 0;
			t2->tm_hour= 0;
			t2->tm_mday= 0;
			t2->tm_mon = 0;
			t2->tm_year++;
			break;
	}

	Filtre->TimeRaz = mktime(t2);

	return TRUE;
}

/****************************************************************************************************************/
/* Lire les filtres dans le fichier de config													*/
int Database_LireIni()
{
	int		i;
	char	Cle[128];
	char	Section[128];
	char	Temp[256];
	FILTRE	*Filtre;
	FILTRE	*LstFiltre;
	

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Parcours des sections du fichier de conf.");

	//Faire le ménage
	LstFiltre = g_LstFiltre;
	while(LstFiltre != NULL)
	{
		Filtre = LstFiltre;
		LstFiltre = Filtre->Suivant;
		free(Filtre);
	}
	LstFiltre = NULL;
	g_LstFiltre = NULL;

	//Parcourir les sections
	i=1;
	while(Fcnf_Section(g_LstParam, i, Section))
	{
		i++;

		//Sauter les sections de paramétrage
		if(STRICMP(Section, "XAP")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section XAP ignorée.");
			continue;
		}
		if(STRICMP(Section, "MYSQL")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section MYSQL ignorée.");
			continue;
		}
		if(STRICMP(Section, "DATABASE")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section DATABASE ignorée.");
			continue;
		}

		//Section valide ?
		sprintf(Cle, "%s_Source", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Temp))
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Section %s ignorée car aucune source n'est défini.", Section);
			continue;
		}

		//Allocation structure
		Filtre = malloc(sizeof(FILTRE));
		if(Filtre==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour stocker les filtres.");
			return FALSE;
		}

		Filtre->Nom[0] = '\0';
		Filtre->Source[0] = '\0';
		Filtre->Classe[0] = '\0';
		Filtre->Periode = '\0';
		Filtre->TypeValeur= 0;
		Filtre->Memo = 0;
		Filtre->Mode = 0;
		Filtre->Suivant = NULL;

		//Lecture des paramètres
		strcpy(Filtre->Nom, Section);
		strcpy(Filtre->Source, Temp);

		sprintf(Cle, "%s_Classe", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Filtre->Classe)) strcpy(Filtre->Classe, "*");

		sprintf(Cle, "%s_Periode", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Temp)) Filtre->Periode = Temp[0];

		sprintf(Cle, "%s_Mode", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Temp)) strcpy(Temp, "LOG");
		if(STRICMP(Temp, "LOG")==0)		Filtre->Mode = MODE_LOG;
		if(STRICMP(Temp, "ETAT")==0)	Filtre->Mode = MODE_ETAT;
		if(STRICMP(Temp, "MIN")==0)		Filtre->Mode = MODE_MIN;
		if(STRICMP(Temp, "MAX")==0)		Filtre->Mode = MODE_MAX;

		sprintf(Cle, "%s_Valeur", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Temp)) strcpy(Temp, "Text");
		if(STRICMP(Temp, "TEXT")==0)	Filtre->TypeValeur = TYPVAL_TXT;
		if(STRICMP(Temp, "TEXTE")==0)	Filtre->TypeValeur = TYPVAL_TXT;
		if(STRICMP(Temp, "STATE")==0)	Filtre->TypeValeur = TYPVAL_STA;
		if(STRICMP(Temp, "ETAT")==0)	Filtre->TypeValeur = TYPVAL_STA;

		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Filtre %s : %s -> %s -> %s", Filtre->Nom, Filtre->Source, Filtre->Classe, Temp);

		//Lecture des mini/maxi dans la DB et calcul du prochain RAZ
		if((Filtre->Mode == MODE_MIN)||(Filtre->Mode == MODE_MAX))
		{
			strcpy(Cle, Filtre->Source);
			if(Filtre->Mode == MODE_MAX)
				strcat(Cle, "-MAX");
			else
				strcat(Cle, "-MIN");

			DB_OuvrirConnexion();
			DB_LireValeurPeriode(Cle, Temp, Filtre->Periode);
			DB_FermerConnexion();

			Filtre->Memo = atof(Temp);
			if(Filtre->Memo==0)
			{
				if(Filtre->Mode == MODE_MAX)
					Filtre->Memo = -999999;
				else
					Filtre->Memo = 999999;
			}
			else
			{
				if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("      Valeur dans la DB %f", Filtre->Memo);
			}
			if(Filtre->Periode != '\0')
			{
				Database_RazInit(Filtre);
				if (g_debuglevel>=DEBUG_VERBOSE)
				{
					Flog_Ecrire("      Période %c", Filtre->Periode);
					Flog_Ecrire("      Prochaine RAZ : %s", ctime(&Filtre->TimeRaz));
				}
			}
		}

		if(LstFiltre != NULL) LstFiltre->Suivant = Filtre;
		if(g_LstFiltre == NULL) g_LstFiltre = Filtre;
		LstFiltre = Filtre;
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de parcours des sections du fichier de conf.");
	return TRUE;
}

/****************************************************************************************************************/
/* Fonctions services : start																					*/
int ServiceStart(char *ConfigFile, char *LogFile)
{
	fd_set i_rdfs;
	struct timeval i_tv;
	char i_xpp_buff[1500+1];

	char Fichier[256];


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

	Flog_Ecrire("Fichier de conf : %s", FichierCnf);
	g_receiver_sockfd = Database_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);
	Database_LireIni();
	
	DBCache_Init();

	//******************************************************************************************************************
	//*** Boucle principale
	while(!g_bServiceStop) 
	{ 
		// Send heartbeat periodically
		xpp_heartbeat_tick(g_IntervalHbeat);
		
		FD_ZERO(&i_rdfs);
		FD_SET(g_receiver_sockfd, &i_rdfs);

		i_tv.tv_sec=10;
		i_tv.tv_usec=0;
	
		select(g_receiver_sockfd+1, &i_rdfs, NULL, NULL, &i_tv);
		
		// Select either timed out, or there was data - go look for it.	
		if (FD_ISSET(g_receiver_sockfd, &i_rdfs))
		{
			// there was an incoming xAP message, not that we care
			if (xpp_PollIncoming(g_receiver_sockfd, i_xpp_buff, sizeof(i_xpp_buff))>0)
			{
				switch(xpp_DispatchReception(i_xpp_buff))
				{
					case XPP_RECEP_CAPTEUR_EVENT :
						if(!g_bServicePause) xpp_handler_Database(XPP_RECEP_CAPTEUR_EVENT);
						break;
					case XPP_RECEP_SERVICE_CMD :
						xpp_handler_service();
						break;
				}
			}
		}
	} // while    

	DBCache_Stop();
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
