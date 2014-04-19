// A FAIRE
// Compléter les service UNIX
//**********************************************************************
// Toute la doc sur l'API owfs : http://owfs.org/index.php?page=owcapi
#include <time.h>
#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include <owcapi.h> 
#include "xpp.h"
#include "OneWire.h"
#include "Service.h"
#include "Fichier.h"

#ifdef WIN32
#define close closesocket
#endif

int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_receiver_sockfd;

int g_IntervalHbeat;
int g_IntervalInfo;
int g_IntervalVie;

char g_OWDebugLevel[16];
char g_OWDebugOut[16];
char g_OWChnCnx[64];

int g_NbCapteur = 0;
CAPTEUR *g_LstCapteur = NULL;
PARAM *g_LstParam = NULL;
char FichierCnf[256];


int OneWire_init()
{
	char i_tmp[20];
	char i_uniqueID[20];
	char i_instance[20];
	char i_interfacename[20];
	int i_interfaceport;
	int i_xapdebuglevel;

	*i_instance = '\0';
	*i_uniqueID = '\0';
	*i_interfacename = '\0';
	i_interfaceport = 0;
	i_xapdebuglevel = 0;

	if(Fcnf_Valeur(g_LstParam,	"XAP_Port", i_tmp)==1) i_interfaceport = atoi(i_tmp);	//3639
	if(Fcnf_Valeur(g_LstParam,	"XAP_Debug", i_tmp)==1) i_xapdebuglevel = atoi(i_tmp);		//0
	Fcnf_Valeur(g_LstParam,		"XAP_UID", i_uniqueID);									//GUID
	Fcnf_Valeur(g_LstParam,		"XAP_Instance", i_instance);							//DEFAULT_INSTANCE
	Fcnf_Valeur(g_LstParam,		"XAP_Interface", i_interfacename);						//eth0
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalHbeat", i_tmp)==1) g_IntervalHbeat = atoi(i_tmp);
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalInfo", i_tmp)==1) g_IntervalInfo = atoi(i_tmp);

	if(Fcnf_Valeur(g_LstParam,	"ONEWIRE_IntervalVie", i_tmp)==1) g_IntervalVie = atoi(i_tmp);
	if(Fcnf_Valeur(g_LstParam,	"ONEWIRE_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);		//0

	Fcnf_Valeur(g_LstParam,		"OWFS_DebugLevel", g_OWDebugLevel);
	Fcnf_Valeur(g_LstParam,		"OWFS_DebugPrint", g_OWDebugOut);
	Fcnf_Valeur(g_LstParam,		"OWFS_ChnCnx", g_OWChnCnx);

	if(g_IntervalHbeat==0) g_IntervalHbeat = 60;
	if(g_IntervalInfo==0) g_IntervalInfo = 60;
	if(g_IntervalVie==0) g_IntervalVie = 3600;

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_xapdebuglevel);
}

int OneWire_Acquire()
{
	if(*g_OWChnCnx=='\0')
	{
		strcpy(g_OWChnCnx, "u");
		if(OneWire_Acquire()) return TRUE;
		strcpy(g_OWChnCnx, "/dev/ttyS0");
		if(OneWire_Acquire()) return TRUE;
		strcpy(g_OWChnCnx, "");
		return FALSE;
	}

	if(OW_init(g_OWChnCnx)==-1)
	{
		Flog_Ecrire("Echec de connexion au OneWire sur %s.", g_OWChnCnx);
		return FALSE;
	}

	OW_set_error_print(g_OWDebugOut);
	OW_set_error_level(g_OWDebugLevel);
	return TRUE;
}

void OneWire_Release()
{
	OW_finish();
}

CAPTEUR *OneWire_ChercherId(char *Id)
{
	CAPTEUR *Capteur;
	int i;
	BOOL bOK;


	//Parcours des capteurs
	for(Capteur=g_LstCapteur; Capteur!=NULL; Capteur = Capteur->Suivant)
	{
		bOK = TRUE;
		if(STRICMP(Id, Capteur->Id)!=0) bOK = FALSE;
		if(bOK == TRUE) return Capteur;
	}
	return NULL;
}

CAPTEUR *OneWire_ChercherPere(char *Id)
{
	CAPTEUR *Capteur;
	int i;
	BOOL bOK;


	//Parcours des capteurs
	for(Capteur=g_LstCapteur; Capteur!=NULL; Capteur = Capteur->Suivant)
	{
		bOK = TRUE;
		if(STRNICMP(Id, Capteur->Id, 15)!=0) bOK = FALSE;
		if(bOK == TRUE) return Capteur;
	}
	return NULL;
}

CAPTEUR *OneWire_ChercherNom(char *Nom)
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

BOOL OneWire_Serial2Name(char* Id, char *Name, char *Path)
{	int i;

	i = ((int) Id[0]-48)*16+(int) Id[1]-48;

	switch(i)
	{
		case 0x05 : 
			if(Name!=NULL) strcpy(Name, "DS2405");
			if(Path!=NULL) strcpy(Path, "PIO");
			return TRUE;
		case 0x10 : 
			if(Name!=NULL) strcpy(Name, "DS1820");
			if(Path!=NULL) strcpy(Path, "temperature");
			return TRUE;
		case 0x12 : 
			if(Name!=NULL) strcpy(Name, "DS2406/7");
			if(Path!=NULL) strcpy(Path, "PIO.A");
			return TRUE;
		case 0x22 : 
			if(Name!=NULL) strcpy(Name, "DS1822");
			if(Path!=NULL) strcpy(Path, "temperature");
			return TRUE;
		case 0x26 : 
			if(Name!=NULL) strcpy(Name, "DS2438");
			if(Path!=NULL) strcpy(Path, "VAD");
			return TRUE;
		case 0x28 : 
			if(Name!=NULL) strcpy(Name, "DS18B20");
			if(Path!=NULL) strcpy(Path, "temperature");
			return TRUE;
		case 0x29 : 
			if(Name!=NULL) strcpy(Name, "DS2408");
			if(Path!=NULL) strcpy(Path, "PIO.BYTE");
			return TRUE;
		case 0x3A : 
			if(Name!=NULL) strcpy(Name, "DS2413");
			if(Path!=NULL) strcpy(Path, "PIO.A");
			return TRUE;
	}
	return FALSE;
}

/****************************************************************************************************************/
/* Fonctions OneWire Haut niveau																				*/
int OneWire_Lire(CAPTEUR *Capteur)
{
	int i;
	char Txt[24], No;
	float ValFloat = 0;
	int Valeur1 = 0;
	int Valeur2 = 0;
	int Retour;
	char *buf;
	size_t s ; 


	//Lecture du capteur
	//Retour = OW_lread(Capteur->Id, Txt, sizeof(Txt), 0);
	Retour = OW_get(Capteur->Id, &buf, &s); 
	strcpy(Txt, buf);
	free(buf);
	if(Retour<1)
	{
		Flog_Ecrire("Impossible de lire le capteur %s (%s)", Capteur->Nom, Capteur->Id);
		return FALSE;
	}
	Txt[Retour] = '\0';

	//affectation de la valeur
	i = ((int) Capteur->Id[0]-48)*16+(int) Capteur->Id[1]-48;
	switch(i)
	{
		case 0x05 : 	//DS2405
			Valeur1 = atoi(Txt);
			break;
		case 0x10 :		//DS1820
			ValFloat = atof(Txt);
			if(ValFloat == 85.0) ValFloat = Capteur->ValeurFloat;
			break;
		case 0x12 :		//DS2406/07
			Valeur1 = atoi(Txt);
			break;
		case 0x22 :		//DS1822
			ValFloat = atof(Txt);
			if(ValFloat == 85.0) ValFloat = Capteur->ValeurFloat;
			break;
		case 0x26 :		//DS2438
			ValFloat = atof(Txt);
			break;
		case 0x28 : 	//DS18?20
			ValFloat = atof(Txt);
			if(ValFloat == 85.0) ValFloat = Capteur->ValeurFloat;
			break;
		case 0x29 : 	//DS2408
			No = Capteur->Id[strlen(Capteur->Id)-1];
			if( (No>='0') && (No<='9') )
				Valeur1 = atoi(Txt);
			else
				Valeur2 = atoi(Txt);
			break;
		case 0x3A : 	//DS2413
			Valeur1 = atoi(Txt);
			break;
	}


	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Lecture ok pour le capteur %s (%s) = %s", Capteur->Nom, Capteur->Id, Txt);

	if( (Capteur->ValeurFloat != ValFloat) || (Capteur->Valeur1 != Valeur1) || (Capteur->Valeur2 != Valeur2) || (Capteur->DerniereLecture == 0) )
	{
		Capteur->ValeurFloat = ValFloat;
		Capteur->Valeur1 = Valeur1;
		Capteur->Valeur2 = Valeur2;
		if(!Capteur->bMasquer) xpp_event(Capteur->No, Capteur->Nom, Capteur->Valeur1, Capteur->Valeur2, Capteur->ValeurFloat);
	}
	return TRUE;
}

int OneWire_Controle(CAPTEUR *LstCapteur)
{
	CAPTEUR *Capteur;
	int	PortNum;
	time_t i_timenow;


	//Connexion
	if(!OneWire_Acquire()) return FALSE;

	//Parcours des capteurs
	for(Capteur=LstCapteur; Capteur!=NULL; Capteur = Capteur->Suivant)
	{
		i_timenow = time((time_t*)0);
		if( (i_timenow-Capteur->DerniereLecture>=Capteur->Interval) || (Capteur->DerniereLecture==0) )
		{
			if(OneWire_Lire(Capteur)) Capteur->DerniereLecture = i_timenow;
		}
	}

	//Déconnexion
	OneWire_Release();

	return TRUE;
}

int OneWire_BSCInfo(int interval, CAPTEUR *LstCapteur) 
{
	time_t i_timenow;
	static time_t i_intervaltick=0;
	CAPTEUR *Capteur;


	i_timenow = time((time_t*)0);
	if ((i_timenow-i_intervaltick<interval)&&(i_intervaltick!=0)) return 0;

	i_intervaltick=i_timenow;

	//Parcours des capteurs et envoi du message l'info
	for(Capteur=LstCapteur; Capteur!=NULL; Capteur = Capteur->Suivant) if(!Capteur->bMasquer)
	{
		if(i_timenow-Capteur->DerniereLecture>=g_IntervalVie) //Pas de nouvelle du capteur donc pas de message XAP
		{
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas d'envoi d'info pour le capteur %s car il n'a pas été lu depuis plus de %d s.", Capteur->Nom, i_timenow-Capteur->DerniereLecture);
			continue;
		}
		xpp_info(Capteur->No, Capteur->Nom, Capteur->Valeur1, Capteur->Valeur2, Capteur->ValeurFloat, NULL);
	}

	return 1;
}

/****************************************************************************************************************/
/* Lire le paramétrage des capteurs dans le fichier de config													*/
/*			Format du fichier (Ini) : [Nom Technique] Nom=Nom_Logique, Interval=nn								*/
/*			Ex : [10.C35D60018000/temparature]																	*/
/*					Nom=Temperature_Bureau																		*/
/*					Interval=15																					*/
/*					Masquer=O/N																					*/
int Capteurs_LireIniModif(char *Id)
{
	char	Cle[128];
	char	Val[16];
	CAPTEUR	*Capteur;


	Capteur = OneWire_ChercherId(Id);
	if(Capteur==NULL) return FALSE;

	sprintf(Cle, "%s_Nom", Id);
	if(Fcnf_Valeur(g_LstParam, Cle, Capteur->Nom))
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le capteur %s devient %s.", Capteur->Id, Capteur->Nom);
	}

	sprintf(Cle, "%s_Interval", Id);
	if(Fcnf_Valeur(g_LstParam, Cle, Val))
	{
		Capteur->Interval = atoi(Val);
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le capteur %s sera lu toutes les %ss.", Capteur->Nom, Capteur->Interval);
	}

	sprintf(Cle, "%s_Masquer", Id);
	if(Fcnf_Valeur(g_LstParam, Cle, Val))
	{
		if(Val[0] == 'O')
		{
			Capteur->bMasquer = TRUE;
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le capteur %s sera masqué.", Capteur->Id);
		}
	}
	return TRUE;
}

int Capteurs_LireIniNew(char *Id)
{
	char	Cle[128];
	char	Val[16];
	CAPTEUR	*Capteur;


	//Vérification de l'existance du capteur de base
	Capteur = OneWire_ChercherPere(Id);
	if(Capteur==NULL) return FALSE;

	//Allocation
	Capteur = malloc(sizeof(CAPTEUR));
	if(Capteur==NULL)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour créer un capteur.");
		return FALSE;
	}

	//Initialisation du capteur
	Capteur->No = ++g_NbCapteur;
	Capteur->ValeurFloat = 0;
	Capteur->Valeur1 = 0;
	Capteur->Valeur2 = 0;
	Capteur->DerniereLecture = 0;
	Capteur->Interval = 10;
	Capteur->bMasquer = FALSE;
	strcpy(Capteur->Id, Id);
	Capteur->Nom[0] = Id[0];
	Capteur->Nom[1] = Id[1];
	strcpy(Capteur->Nom+2, Id+3);

	sprintf(Cle, "%s_Nom", Id);
	Fcnf_Valeur(g_LstParam, Cle, Capteur->Nom);

	sprintf(Cle, "%s_Interval", Id);
	if(Fcnf_Valeur(g_LstParam, Cle, Val)) Capteur->Interval = atoi(Val);

	sprintf(Cle, "%s_Masquer", Id);
	if(Fcnf_Valeur(g_LstParam, Cle, Val))
	{
		if(Val[0] == 'O')
		{
			Capteur->bMasquer = TRUE;
		}
	}

	//Ajout dans la liste chainée
	Capteur->Suivant  = g_LstCapteur;
	g_LstCapteur = Capteur;

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Création du capteur n°%d : %s (%s).", Capteur->No, Capteur->Nom, Capteur->Id);
	return TRUE;
}

int Capteurs_LireIni()
{
	int			i;
	char		Nom[128];
	char		Cle[128];
	char		Section[128];
	

	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Lecture du fichier de conf des capteurs.");

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
		if(STRICMP(Section, "ONEWIRE")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section ONEWIRE ignorée.");
			continue;
		}
		if(STRICMP(Section, "OWFS")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section OWFS ignorée.");
			continue;
		}

		//Modification d'un capteur existant
		if(Capteurs_LireIniModif(Section)) continue;

		//Création d'un capteur
		if(Capteurs_LireIniNew(Section)) continue;

		//Pas trouvé
		sprintf(Cle, "%s_Nom", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Nom))
			Flog_Ecrire("Le capteur %s (%s) n'existe pas.", Section, Nom);
		else
			Flog_Ecrire("Le capteur %s n'existe pas.", Section);
	}

	if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de lecture du fichier de conf des capteurs.");
	return TRUE;
}

/****************************************************************************************************************/
/* Lire la liste des capteurs du reseau OW																					*/
int OneWire_Liste()
{
	char	Name[20];
	char	ValDef[20];
	CAPTEUR	*Capteur;
	CAPTEUR	*LstCapteur;
	char *buf, *Id, *fin;
	size_t s ; 


	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche automatique des capteurs");

	//Initialisation
	LstCapteur = g_LstCapteur;
	while(LstCapteur != NULL)
	{
		Capteur = LstCapteur;
		LstCapteur = Capteur->Suivant;
		free(Capteur);
	}
	LstCapteur = NULL;

	//Obtenir la liste des capteurs
	//Exemple de liste 10.0429A9010800/,10.E20E4C010800/,10.7E474C010800/,10.13EF5F010800/,10.5349A9010800/,28.494D48010000/,22.12591A000000/,22.558B1A000000/,12.8D7845000000/,29.41C105000000/,bus.0/,uncached/,settings/,system/,statistics/,structure/,simultaneous/,alarm/
	if(!OneWire_Acquire()) return FALSE;
	OW_get("/",&buf,&s) ; 
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Liste des capteurs trouvés : %s", buf);

	//Mémoriser les capteurs
	Id = buf;
	while(*Id!='\0')
	{
		//Isoler le capteur dans la liste
		fin = strchr(Id, '/');
		if(fin==NULL)
		{
			Flog_Ecrire("Fin anormal de la liste des capteurs : '/' non trouvé.");
			goto OneWire_Liste_LblFin;
		}
		*fin = '\0';

		//Contrôle validité
		if(Id[2] != '.')
		{
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Capteur inconnu : %s (Pas de . en 3ème position).", Id);
			Id = fin+2;
			continue;
		}

		//Contrôle gestion
		if(!OneWire_Serial2Name(Id, Name, ValDef))
		{
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Capteur non géré : %s", Id);
			Id = fin+2;
			continue;
		}

		//Ajout dans la liste chainée
		Capteur = malloc(sizeof(CAPTEUR));
		if(Capteur==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour créer un capteur.");
			goto OneWire_Liste_LblFin;
		}

		Capteur->No = ++g_NbCapteur;
		Capteur->ValeurFloat = 0;
		Capteur->Valeur1 = 0;
		Capteur->Valeur2 = 0;
		Capteur->DerniereLecture = 0;
		Capteur->Interval = 10;
		Capteur->bMasquer = FALSE;
		sprintf(Capteur->Id, "%s/%s", Id, ValDef);
		Capteur->Nom[0] = Id[0];
		Capteur->Nom[1] = Id[1];
		strcpy(Capteur->Nom+2, Id+3);
		Capteur->Suivant  = LstCapteur;
		LstCapteur = Capteur;

		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Capteur n°%d: %s (%s)", Capteur->No, Capteur->Id, Name);
		Id = fin+2;
	}

	//Déconnexion
OneWire_Liste_LblFin:
	g_LstCapteur = LstCapteur;
	if((g_debuglevel>=DEBUG_INFO)&&(LstCapteur==NULL)) Flog_Ecrire("Aucun capteur prise en charge");
	free(buf); 
	OW_finish();

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de recherche automatique des capteurs");

	return TRUE;
}

/****************************************************************************************************************/
/* Traitement d'un message XPP																					*/
int xpp_handler_service()
{
	char i_temp[64];

	if (xpp_GetCmd("request:state", i_temp)!=-1)
	{
		if (STRICMP(i_temp, "stop")==0) g_bServiceStop = TRUE;
		if (STRICMP(i_temp, "start")==0) g_bServicePause = FALSE;
		if (STRICMP(i_temp, "pause")==0) g_bServicePause = TRUE;
		return 0;
	}
	if (xpp_GetCmd("request:init", i_temp)!=-1)
	{
		if (STRICMP(i_temp, "config")==0)
		{
			Fcnf_Lire(FichierCnf, &g_LstParam);
			Capteurs_LireIni();
		}
		if (STRICMP(i_temp, "liste")==0) 
		{
			OneWire_Liste(&g_LstCapteur);
		}
	}
	return 0;
}

int xpp_handler_CmdOneWire()
{
	char i_temp[128];
	char *ptr;
	int Etat;
	int No;
	int	Retour;
	CAPTEUR *Capteur;
	CAPTEUR *Capi;


	if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Entrée dans xpp_handler_CmdOneWire()");
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
		Capteur = OneWire_ChercherNom(ptr);
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur != NULL)) Flog_Ecrire("Nom du capteur trouvé dans target: %s.", ptr);
	}

	if(Capteur == NULL)
	{
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Impossible de trouver le nom du capteur.");
		if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Sortie de xpp_handler_CmdOneWire()");
		return FALSE;
	}

	//Etat demandé
	if (xpp_GetTargetState(i_temp)==-1)
	{
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Impossible de lire output.state.1:State.");
		if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Sortie de xpp_handler_CmdOneWire()");
		return FALSE;
	}
	else
	{
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Valeur demandée '%s'.", i_temp);
	}

	Etat = atoi(i_temp);
	if (STRICMP(i_temp, "on")==0)
	{
		Etat = 1;
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Conversion '%s' en %d.", i_temp, Etat);
	}
	if (STRICMP(i_temp, "off")==0)
	{
		Etat = 0;
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Conversion '%s' en %d.", i_temp, Etat);
	}
	if (STRICMP(i_temp, "toogle")==0)
	{
		Etat = !Capteur->Valeur1;
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Conversion '%s' en %d.", i_temp, Etat);
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Mettre le capteur %s à l'état %d.", Capteur->Nom, Etat);

	//Connexion
	if(!OneWire_Acquire()) return FALSE;

	//Fixer la valeur du capteur
	sprintf(i_temp, "%d", Etat);
	Retour = OW_put(Capteur->Id, i_temp, strlen(i_temp));
	if(Retour<1)
	{
		Flog_Ecrire("Impossible de fixer le capteur %s (%s) à %s", Capteur->Nom, Capteur->Id, i_temp);
		if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Sortie de xpp_handler_CmdOneWire()");
		return FALSE;
	}

	//Déconnexion
	OneWire_Release();

	if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Sortie de xpp_handler_CmdOneWire()");
	return TRUE;
}

int xpp_handler_QueryOneWire()
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
		Capteur = OneWire_ChercherNom(ptr);
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur == NULL)) Flog_Ecrire("Nom du capteur trouvé dans target: %s.", ptr);
	}

	if(Capteur == NULL)
	{
		if((g_debuglevel>=DEBUG_VERBOSE)&&(Capteur == NULL)) Flog_Ecrire("Impossible de trouver le nom du capteur.");
		return 0;
	}

	//Envoyer l'état demandé
	if(xpp_GetSourceName(i_temp)!=-1)
		xpp_info(Capteur->No, Capteur->Nom, Capteur->Valeur1, Capteur->Valeur2, Capteur->ValeurFloat, i_temp);
	else
		xpp_info(Capteur->No, Capteur->Nom, Capteur->Valeur1, Capteur->Valeur2, Capteur->ValeurFloat, NULL);

	return 0;
}

/****************************************************************************************************************/
/* Fonctions services : start																					*/
int ServiceStart(char *ConfigFile, char *LogFile) 
{
	char Fichier[256];
	fd_set i_rdfs;
	struct timeval i_tv;
	char i_xpp_buff[1500+1];


	//******************************************************************************************************************
	//*** Initialisation générale (Fichier de log)
	strcpy(g_OWDebugLevel, "");
	strcpy(g_OWDebugOut, "");
	strcpy(g_OWChnCnx, "");

	//******************************************************************************************************************
	//*** Initialisation générale (Fichier de log)
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
	g_receiver_sockfd = OneWire_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);

	//******************************************************************************************************************
	//*** Construction de la liste des capteurs
	OneWire_Liste(&g_LstCapteur);

	//******************************************************************************************************************
	//*** Lecture du fichier capteurs
	Capteurs_LireIni();

	//******************************************************************************************************************
	//*** Boucle principale
	while(!g_bServiceStop) 
	{ 
		// Envoi un heartbeat regulièrement
		xpp_heartbeat_tick(g_IntervalHbeat);
		
		// Parcours les capteurs
		OneWire_Controle(g_LstCapteur);

		// Envoi la liste complète des capteurs regulièrement
		OneWire_BSCInfo(g_IntervalInfo, g_LstCapteur);

		FD_ZERO(&i_rdfs);
		FD_SET(g_receiver_sockfd, &i_rdfs);

		i_tv.tv_sec=5;
		i_tv.tv_usec=0;
	
		select(g_receiver_sockfd+1, &i_rdfs, NULL, NULL, &i_tv);
		
		// Select either timed out, or there was data - go look for it.	
		if (FD_ISSET(g_receiver_sockfd, &i_rdfs))
		{
			// there was an incoming xpp message, not that we care
			if (xpp_PollIncoming(g_receiver_sockfd, i_xpp_buff, sizeof(i_xpp_buff))>0)
			{
				switch(xpp_DispatchReception(i_xpp_buff))
				{
					case XPP_RECEP_CAPTEUR_CMD :
						if(!g_bServicePause) xpp_handler_CmdOneWire();
						break;
					case XPP_RECEP_CAPTEUR_QUERY :
						if(!g_bServicePause) xpp_handler_QueryOneWire();
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
	shutdown(g_receiver_sockfd, 1);
	close(g_receiver_sockfd);
	return;
}
