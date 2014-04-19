#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include "Service.h"
#include "Fichier.h"
#include "xpp.h"
#include "Thermostat.h"

#ifdef WIN32
#define close closesocket
#endif

int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_receiver_sockfd;

THERMOSTAT *g_LstThermostat = NULL;
CACHE *g_LstCache = NULL;
PARAM *g_LstParam = NULL;
char FichierCnf[256];


/****************************************************************************************************************/
/* Initialisation du module thermostat																			*/
int Thermostat_init()
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

	if(Fcnf_Valeur(g_LstParam, "THERMOSTAT_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);
}

/****************************************************************************************************************/
/* Lire les	Thermostats																							*/
int Thermostat_LireIni()
{
	int			i;
	char		Cle[128];
	char		Section[128];
	char		Tmp[128];
	THERMOSTAT	*Thermostat;
	

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Parcours des sections du fichier de conf.");

	//Initialisation
	while(g_LstThermostat != NULL)
	{
		Thermostat = g_LstThermostat;
		g_LstThermostat = Thermostat->Suivant;
		free(Thermostat);
	}
	Thermostat = NULL;

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
		if(STRICMP(Section, "THERMOSTAT")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section THERMOSTAT ignorée.");
			continue;
		}

		Thermostat = malloc(sizeof(THERMOSTAT));
		if(Thermostat==NULL) return TRUE;

		strcpy(Thermostat->Nom, Section);
		Thermostat->Etat = ETAT_NC;
		Thermostat->Inverser = FALSE;

		sprintf(Cle, "%s_Actif", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Thermostat->Actif)) strcpy(Thermostat->Actif, "\"ON\"");
		sprintf(Cle, "%s_Entree", Section);
		Fcnf_Valeur(g_LstParam, Cle, Thermostat->Entree);
		sprintf(Cle, "%s_Consigne", Section);
		Fcnf_Valeur(g_LstParam, Cle, Thermostat->Consigne);
		sprintf(Cle, "%s_Hysteresis", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Thermostat->Hysteresis)) strcpy(Thermostat->Hysteresis, "0");
		sprintf(Cle, "%s_Sortie", Section);
		Fcnf_Valeur(g_LstParam, Cle, Thermostat->Sortie);
		sprintf(Cle, "%s_Inverser", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Tmp))
			if((Tmp[0]=='O')||(Tmp[0]=='Y')) Thermostat->Inverser = TRUE;

		sprintf(Cle, "%s_Differentiel", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Thermostat->Differentiel)) Thermostat->Differentiel[0] = '\0';
		sprintf(Cle, "%s_DiffEcartON", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Thermostat->DiffEcartON)) Thermostat->DiffEcartON[0] = '\0';
		sprintf(Cle, "%s_DiffEcartOFF", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Thermostat->DiffEcartOFF)) Thermostat->DiffEcartOFF[0] = '\0';

		sprintf(Cle, "%s_CommandeON", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Thermostat->CommandeON)) strcpy(Thermostat->CommandeON, "on");
		sprintf(Cle, "%s_CommandeOFF", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Thermostat->CommandeOFF)) strcpy(Thermostat->CommandeOFF, "off");

		Thermostat->Suivant = g_LstThermostat;
		g_LstThermostat = Thermostat;

		if (g_debuglevel>=DEBUG_VERBOSE)
		{
			Flog_Ecrire("THERMOSTAT %s", Thermostat->Nom);
			Flog_Ecrire("      Entrée : %s", Thermostat->Entree);
			Flog_Ecrire("      Sortie : %s", Thermostat->Sortie);
			Flog_Ecrire("      Consigne : %s, Hysteresis : %s, Actif : %s", Thermostat->Consigne, Thermostat->Hysteresis, Thermostat->Actif);
			if(Thermostat->Inverser == TRUE) Flog_Ecrire("      Mode clim actif");
			Flog_Ecrire("      CommandeON : %s, CommandeOFF : %s", Thermostat->CommandeON, Thermostat->CommandeOFF);
			if(Thermostat->Differentiel[0] != '\0') Flog_Ecrire("      Differentiel : %s, EcartON : %s, EcartOFF : %s", Thermostat->Differentiel, Thermostat->DiffEcartON, Thermostat->DiffEcartOFF);
		}
	}

	return TRUE;
}

/****************************************************************************************************************/
/* Recherche dans la liste des thermostats																		*/
THERMOSTAT *Thermostat_SortieRecherche(char *Capteur)
{
	THERMOSTAT	*Thermostat;

	Thermostat = g_LstThermostat;
	while(Thermostat != NULL)
	{
		if(STRICMP(Thermostat->Sortie, Capteur)==0)	return Thermostat;
		Thermostat = Thermostat->Suivant;
	}
	return NULL;
}

/****************************************************************************************************************/
/* Recherche dans le cache																								*/
CACHE *Thermostat_CacheRecherche(char *Capteur)
{
	CACHE	*Cache;

	Cache = g_LstCache;
	while(Cache != NULL)
	{
		if(STRICMP(Cache->Nom, Capteur)==0)	return Cache;
		Cache = Cache->Suivant;
	}
	return NULL;
}

/****************************************************************************************************************/
/* Lire le cache																								*/
int Thermostat_CacheLire(char *Capteur, char *Valeur)
{
	CACHE	*Cache;

	Cache = Thermostat_CacheRecherche(Capteur);
	if(Cache == NULL) return FALSE;

	if(Cache->bLu == FALSE)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Le capteur %s n'a pas encore été mis en cache.",Capteur);
		return FALSE;
	}

	strcpy(Valeur, Cache->Valeur);
	return TRUE;
}

/****************************************************************************************************************/
/* Ajouter un capteur dans le cache																				*/
int Thermostat_CacheAjoute(char *Capteur, THERMOSTAT *pThermostat)
{
	CACHE		*Cache;
	LSTTHERMO	*LstThermo;

	//Ce capteur ne fait pas partie du cache
	Cache = Thermostat_CacheRecherche(Capteur);
	if(Cache == NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Ajout au cache de %s pour %s",Capteur,pThermostat->Nom);
		Cache = malloc(sizeof(CACHE));
		Cache->Suivant = g_LstCache;
		g_LstCache = Cache;
		strcpy(Cache->Nom, Capteur);
		Cache->bLu = FALSE;
		Cache->LstThermo = malloc(sizeof(LSTTHERMO));
		Cache->LstThermo->Thermostat = pThermostat;
		Cache->LstThermo->Suivant = NULL;
		xpp_query(Cache->Nom);
		return TRUE;
	}

	//Le thermostat est déjà lié à ce capteur
	LstThermo = Cache->LstThermo;
	while(LstThermo != NULL)
	{
		if(LstThermo->Thermostat == pThermostat) return TRUE;
		LstThermo = LstThermo->Suivant;
	}

	//Ajouter le thermostat à la liste
	LstThermo = malloc(sizeof(LSTTHERMO));
	LstThermo->Thermostat = pThermostat;
	LstThermo->Suivant = Cache->LstThermo;
	Cache->LstThermo = LstThermo;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Maj du cache de %s pour %s",Capteur,pThermostat->Nom);

	 return TRUE;
}

/****************************************************************************************************************/
/* Initialiser le cache de capteurs																				*/
int Thermostat_CacheInit(THERMOSTAT *pThermostat, CACHE **pCache)
{
	CACHE	*LstCache;
	CACHE	*Cache;
	THERMOSTAT	*Thermostat;


	//Initialisation
	LstCache = *pCache;
	while(LstCache != NULL)
	{
		Cache = LstCache;
		LstCache = Cache->Suivant;
		free(Cache);
	}
	Cache = NULL;

	//Construction du cache
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Constitution du cache...");
	Thermostat = pThermostat;
	while(Thermostat != NULL)
	{
		if(xpp_bIdCapteur(Thermostat->Actif)==TRUE)			Thermostat_CacheAjoute(Thermostat->Actif, Thermostat);
		if(xpp_bIdCapteur(Thermostat->Entree)==TRUE)		Thermostat_CacheAjoute(Thermostat->Entree, Thermostat);
		if(xpp_bIdCapteur(Thermostat->Consigne)==TRUE)		Thermostat_CacheAjoute(Thermostat->Consigne, Thermostat);
		if(xpp_bIdCapteur(Thermostat->Hysteresis)==TRUE)	Thermostat_CacheAjoute(Thermostat->Hysteresis, Thermostat);
		if(xpp_bIdCapteur(Thermostat->Differentiel)==TRUE)	Thermostat_CacheAjoute(Thermostat->Differentiel, Thermostat);
		if(xpp_bIdCapteur(Thermostat->DiffEcartON)==TRUE)	Thermostat_CacheAjoute(Thermostat->DiffEcartON, Thermostat);
		if(xpp_bIdCapteur(Thermostat->DiffEcartOFF)==TRUE)	Thermostat_CacheAjoute(Thermostat->DiffEcartOFF, Thermostat);
		xpp_query(Thermostat->Sortie);		//Pour mémoriser l'état de la sortie au démarrage du module
		Thermostat = Thermostat->Suivant;
	}
	
	return TRUE;
}

/****************************************************************************************************************/
/* Valoriser le thermostat																						*/
int Thermostat_DecodeVal(char *Code, char *Valeur)
{
	unsigned int i;

	//S'il y a des " on les vire et on renvoie
	if((Code[0] == '"') && (Code[strlen(Code)-1] == '"'))
	{
		for(i=0;i<strlen(Code)-2;i++) Valeur[i] = Code[i+1];
		Valeur[i] = '\0';
		return TRUE;
	}

	//C'est un capteur -> Lire dans le cache
	if(xpp_bIdCapteur(Code)==TRUE)
	{
		return Thermostat_CacheLire(Code, Valeur);
	}

	//Autre cas : Renvoyer tel quel
	strcpy(Valeur, Code);
	return TRUE;
}

BOOL Thermostat_bValorise(char *Code, BOOL *Valeur)
{	//Valorisation type Booleen
	char Cache[48];

	if(!Thermostat_DecodeVal(Code, Cache)) return FALSE;

	*Valeur = FALSE;

	if(atoi(Cache)>0) *Valeur = TRUE;
	if(STRICMP(Cache, "ON") == 0) *Valeur = TRUE;

	return TRUE;
}

BOOL Thermostat_nValorise(char *Code, float *Valeur)
{	//Valorisation type Float
	char Cache[48];

	if(!Thermostat_DecodeVal(Code, Cache)) return FALSE;
	
	*Valeur = (float) atof(Cache);
	return TRUE;
}

int Thermostat_Valorise(THERMOSTAT *pThermostat)
{
	//Mettre à jour les valeurs grâce au cache
	if(!Thermostat_bValorise(pThermostat->Actif,		&pThermostat->Valeurs.bActif)) return FALSE;
	if(!Thermostat_nValorise(pThermostat->Entree,		&pThermostat->Valeurs.Entree)) return FALSE;
	if(!Thermostat_nValorise(pThermostat->Consigne,		&pThermostat->Valeurs.Consigne)) return FALSE;
	if(!Thermostat_nValorise(pThermostat->Hysteresis,	&pThermostat->Valeurs.Hysteresis)) return FALSE;
	if(!Thermostat_nValorise(pThermostat->Differentiel,	&pThermostat->Valeurs.Differentiel)) return FALSE;
	if(!Thermostat_nValorise(pThermostat->DiffEcartON,	&pThermostat->Valeurs.DiffEcartON)) return FALSE;
	if(!Thermostat_nValorise(pThermostat->DiffEcartOFF,	&pThermostat->Valeurs.DiffEcartOFF)) return FALSE;
	return TRUE;
}

/****************************************************************************************************************/
/* Basculer le thermostat																						*/
int Thermostat_Bascule(THERMOSTAT *pThermostat, int Etat, char *Commande)
{
	int MonEtat;

	MonEtat = Etat;											//Cas standard
	if(STRICMP(Commande, "ON")==0) MonEtat = ETAT_ON;		//Utile pour les thermostats inversés :
	if(STRICMP(Commande, "OFF")==0) MonEtat = ETAT_OFF;		// Lorsque CommandeON=OFF & CommandeOFF=ON

	if(pThermostat->Etat == MonEtat) 
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Pas de bascule, le %s est déjà sur le bon état.",pThermostat->Nom);
		return TRUE;
	}

	if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("THERMOSTAT %s bascule %s sur %s", pThermostat->Nom, pThermostat->Sortie, Commande);

	xpp_cmd(pThermostat->Sortie, Commande, NULL, NULL);

	pThermostat->Etat = MonEtat;
	return TRUE;
}

/****************************************************************************************************************/
/* Traiter le thermostat																						*/
int Thermostat_Traite(THERMOSTAT *pThermostat)
{
	int bOk;


	//Mettre à jour les valeurs grace au cache
	if(!Thermostat_Valorise(pThermostat))
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Echec de la valorisation du thermostat %s.",pThermostat->Nom);
		return FALSE;
	}

	//Ne traiter que s'il est actif
	if(!pThermostat->Valeurs.bActif)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat %s n'est pas actif.",pThermostat->Nom);
		if(!Thermostat_Bascule(pThermostat, ETAT_OFF, pThermostat->CommandeOFF)) return FALSE;
		return TRUE;
	}

	//Traiter la position OFF
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat %s, comparaison %f > %f ?",pThermostat->Nom, pThermostat->Valeurs.Entree, pThermostat->Valeurs.Consigne+pThermostat->Valeurs.Hysteresis);
	if(pThermostat->Valeurs.Entree > pThermostat->Valeurs.Consigne+pThermostat->Valeurs.Hysteresis)
	{
		if(!Thermostat_Bascule(pThermostat, ETAT_OFF, pThermostat->CommandeOFF)) return FALSE;
		return TRUE;
	}

	//Traiter la position OFF (Différentiel)
	if(pThermostat->Differentiel[0] != '\0')
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat %s, differentiel %f-%f < %f ?",pThermostat->Nom, pThermostat->Valeurs.Differentiel, pThermostat->Valeurs.Entree, pThermostat->Valeurs.DiffEcartOFF);
		if(pThermostat->Valeurs.Differentiel-pThermostat->Valeurs.Entree < pThermostat->Valeurs.DiffEcartOFF)
		{
			if(!Thermostat_Bascule(pThermostat, ETAT_OFF, pThermostat->CommandeOFF)) return FALSE;
			return TRUE;
		}
	}

	//Traiter la position ON
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat %s, comparaison %f < %f ?",pThermostat->Nom, pThermostat->Valeurs.Entree, pThermostat->Valeurs.Consigne-pThermostat->Valeurs.Hysteresis);
	if(pThermostat->Valeurs.Entree < pThermostat->Valeurs.Consigne-pThermostat->Valeurs.Hysteresis)
	{
		bOk = TRUE;
		if(pThermostat->Differentiel[0] != '\0')		//Controle du differentiel
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat %s, differentiel %f-%f > %f ?",pThermostat->Nom, pThermostat->Valeurs.Differentiel, pThermostat->Valeurs.Entree, pThermostat->Valeurs.DiffEcartON);
			if(pThermostat->Valeurs.Differentiel-pThermostat->Valeurs.Entree > pThermostat->Valeurs.DiffEcartON)
				bOk = TRUE;
			else
				bOk = FALSE;
		}
		if(bOk==TRUE)
		{
			if(!Thermostat_Bascule(pThermostat, ETAT_ON, pThermostat->CommandeON)) return FALSE;
			return TRUE;
		}
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat %s : Rien à faire",pThermostat->Nom);
	return TRUE;
}

int Thermostat_TraiteInv(THERMOSTAT *pThermostat)
{
	int bOk;


	//Mettre à jour les valeurs grace au cache
	if(!Thermostat_Valorise(pThermostat))
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Echec de la valorisation du thermostat (inv) %s.",pThermostat->Nom);
		return FALSE;
	}

	//Ne traiter que s'il est actif
	if(!pThermostat->Valeurs.bActif)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat (inv) %s n'est pas actif.",pThermostat->Nom);
		if(!Thermostat_Bascule(pThermostat, ETAT_OFF, pThermostat->CommandeOFF)) return FALSE;
		return TRUE;
	}

	//Traiter la position OFF
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat (inv) %s, comparaison %f < %f ?",pThermostat->Nom, pThermostat->Valeurs.Entree, pThermostat->Valeurs.Consigne-pThermostat->Valeurs.Hysteresis);
	if(pThermostat->Valeurs.Entree < pThermostat->Valeurs.Consigne-pThermostat->Valeurs.Hysteresis)
	{
		if(!Thermostat_Bascule(pThermostat, ETAT_OFF, pThermostat->CommandeOFF)) return FALSE;
		return TRUE;
	}

	//Traiter la position OFF (Différentiel)
	if(pThermostat->Differentiel[0] != '\0')
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat (inv) %s, differentiel %f-%f < %f ?",pThermostat->Nom, pThermostat->Valeurs.Entree, pThermostat->Valeurs.Differentiel, pThermostat->Valeurs.DiffEcartOFF);
		if(pThermostat->Valeurs.Entree-pThermostat->Valeurs.Differentiel < pThermostat->Valeurs.DiffEcartOFF)
		{
			if(!Thermostat_Bascule(pThermostat, ETAT_OFF, pThermostat->CommandeOFF)) return FALSE;
			return TRUE;
		}
	}

	//Traiter la position ON
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat (inv) %s, comparaison %f < %f ?",pThermostat->Nom, pThermostat->Valeurs.Entree, pThermostat->Valeurs.Consigne+pThermostat->Valeurs.Hysteresis);
	if(pThermostat->Valeurs.Entree > pThermostat->Valeurs.Consigne+pThermostat->Valeurs.Hysteresis)
	{
		bOk = TRUE;
		if(pThermostat->Differentiel[0] != '\0')		//Controle du differentiel
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat (inv) %s, differentiel %f-%f > %f ?",pThermostat->Nom, pThermostat->Valeurs.Entree, pThermostat->Valeurs.Differentiel, pThermostat->Valeurs.DiffEcartON);
			if(pThermostat->Valeurs.Entree-pThermostat->Valeurs.Differentiel > pThermostat->Valeurs.DiffEcartON)
				bOk = TRUE;
			else
				bOk = FALSE;
		}
		if(bOk==TRUE)
		{
			if(!Thermostat_Bascule(pThermostat, ETAT_ON, pThermostat->CommandeON)) return FALSE;
			return TRUE;
		}
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le thermostat (inv) %s : Rien à faire",pThermostat->Nom);
	return TRUE;
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
			Thermostat_LireIni();
			Thermostat_CacheInit(g_LstThermostat, &g_LstCache);
		}
		return 0;
	}

	return 1;
}

int xpp_handler_Th(int i_TypMsg)
{
	CACHE		*Cache;
	LSTTHERMO	*LstThermo;
	THERMOSTAT	*Thermostat;

	char i_source[128];
	char i_temp[64];
	int	Old_bLu;


	//Rechercher dans le cache et dans la liste des thermostats
	if (xpp_GetSourceName(i_source)==-1) return 0;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche dans le cache de %s.",i_source);
	Cache = Thermostat_CacheRecherche(i_source);
	Thermostat = Thermostat_SortieRecherche(i_source);
	if((Cache == NULL)&&(Thermostat == NULL)) return 0;

	//Lire la valeur
	if (xpp_GetTargetText(i_temp)==-1) return 0;
	if(atof(i_temp)==0) xpp_GetTargetState(i_temp);

	//Ecrire l'état du thermostat
	if(Thermostat != NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Mémorisation de l'état du thermostat %s=%s.",Thermostat->Nom, i_temp);
		if(STRICMP(i_temp, "ON")==0) Thermostat->Etat = ETAT_ON;
		if(STRICMP(i_temp, "OFF")==0) Thermostat->Etat = ETAT_OFF;
		return 1;
	}

	//Ecrire dans le cache
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Mise en cache %s=%s.",i_source, i_temp);
	strcpy(Cache->Valeur, i_temp);
	Old_bLu = Cache->bLu;
	Cache->bLu = TRUE;

	//Déclencher les thermostats uniquement sur message EVENT
	//	sauf au démarrage du service :
	//	déclenchement des Th tant que le cache n'est pas entièrement chargé.
	if( (i_TypMsg==XPP_RECEP_CAPTEUR_INFO) && (Old_bLu==TRUE) ) return 0;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche des thermostats à traiter.");
	LstThermo = Cache->LstThermo;
	while(LstThermo!=NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Traitement de %s.",LstThermo->Thermostat->Nom);
		if(LstThermo->Thermostat->Inverser == TRUE)
			Thermostat_TraiteInv(LstThermo->Thermostat);
		else
			Thermostat_Traite(LstThermo->Thermostat);
		LstThermo = LstThermo->Suivant;
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
	g_receiver_sockfd = Thermostat_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);
	Thermostat_LireIni();

	//******************************************************************************************************************
	//*** Etablir la liste des capteurs à surveiller
	Thermostat_CacheInit(g_LstThermostat, &g_LstCache);

	//******************************************************************************************************************
	//*** Boucle principale
	while(!g_bServiceStop) 
	{ 
		// Send heartbeat periodically
		xpp_heartbeat_tick(HBEAT_INTERVAL);
		
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
						if(!g_bServicePause) xpp_handler_Th(XPP_RECEP_CAPTEUR_INFO);
						break;
					case XPP_RECEP_CAPTEUR_EVENT :
						if(!g_bServicePause) xpp_handler_Th(XPP_RECEP_CAPTEUR_EVENT);
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
