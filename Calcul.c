#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
//#include <math.h>
//#include <float.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include "Service.h"
#include "Fichier.h"
#include "xpp.h"
#include "Calcul.h"

#ifdef WIN32
#define close closesocket
#endif

int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_IntervalHbeat;
int g_receiver_sockfd;

PARAM *g_LstParam = NULL;
CACHE *g_LstCache = NULL;
CALCUL *g_LstCalcul = NULL;
char FichierCnf[256];


/****************************************************************************************************************/
/* Initialisation du module Calcul																				*/
int Calcul_init()
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
	if(Fcnf_Valeur(g_LstParam, "XAP_Debug", i_tmp)==1) i_debuglevel = atoi(i_tmp);		//0
	Fcnf_Valeur(g_LstParam, "XAP_UID", i_uniqueID);										//GUID
	Fcnf_Valeur(g_LstParam, "XAP_Instance", i_instance);								//DEFAULT_INSTANCE
	Fcnf_Valeur(g_LstParam, "XAP_Interface", i_interfacename);							//eth0
	if(Fcnf_Valeur(g_LstParam,	"XAP_IntervalHbeat", i_tmp)==1) g_IntervalHbeat = atoi(i_tmp);
	if(g_IntervalHbeat==0) g_IntervalHbeat = 60;

	if(Fcnf_Valeur(g_LstParam,	"CALCUL_Debug", i_tmp)==1) g_debuglevel = atoi(i_tmp);		//0

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);
}

/****************************************************************************************************************/
/* Lire le paramétrage des calculs dans le fichier de config													*/
int Calcul_LireIni()
{
	int		i;
	char	Cle[128];
	char	Section[128];
	char	Temp[256];
	CALCUL	*Calcul;
	

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Parcours des sections du fichier de conf.");

	//Faire le ménage
	while(g_LstCalcul != NULL)
	{
		Calcul = g_LstCalcul;
		g_LstCalcul = Calcul->Suivant;
		free(Calcul);
	}

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
		if(STRICMP(Section, "CALCUL")==0)
		{
			if(g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Section CALCUL ignorée.");
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
		Calcul = malloc(sizeof(CALCUL));
		if(Calcul==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour stocker les params de calcul.");
			return FALSE;
		}

		//Lecture des paramètres
		strcpy(Calcul->Nom, Section);
		strcpy(Calcul->Sortie, Temp);
		Calcul->LstCache = NULL;

		sprintf(Cle, "%s_Calcul", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Calcul->Formule)) strcpy(Calcul->Formule, "0");

		sprintf(Cle, "%s_Defaut", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Temp))
			Calcul->Valeur = atof(Temp);
		else
			Calcul->Valeur = 0;

		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Calcul de %s : %s = %s", Calcul->Nom, Calcul->Sortie, Calcul->Formule);

		Calcul->Suivant = g_LstCalcul;
		g_LstCalcul = Calcul;
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de parcours des sections du fichier de conf.");
	return TRUE;
}

/****************************************************************************************************************/
/* Recherche dans la liste des thermostats																		*/
CALCUL *Calcul_SortieRecherche(char *Capteur)
{
	CALCUL	*Calcul;

	Calcul = g_LstCalcul;
	while(Calcul != NULL)
	{
		if(STRICMP(Calcul->Sortie, Capteur)==0)	return Calcul;
		Calcul = Calcul->Suivant;
	}
	return NULL;
}

/****************************************************************************************************************/
/* Recherche dans le cache																								*/
CACHE *Calcul_CacheRecherche(char *Capteur)
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
int Calcul_CacheLire(char *Capteur, double *Valeur)
{
	CACHE	*Cache;

	Cache = Calcul_CacheRecherche(Capteur);
	if(Cache == NULL) return FALSE;

	if(Cache->bLu == FALSE)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Le capteur %s n'a pas encore été mis en cache.",Capteur);
		return FALSE;
	}

	*Valeur = atof(Cache->Valeur);
	return TRUE;
}

/****************************************************************************************************************/
/* Ajouter un capteur dans le cache																				*/
CACHE *Calcul_CacheAjoute(char *Capteur, CALCUL *pCalcul)
{
	CACHE		*Cache;
	LSTCALCUL	*LstCalcul;

	//Ce capteur ne fait pas partie du cache ?
	Cache = Calcul_CacheRecherche(Capteur);
	if(Cache == NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Ajout au cache de %s pour %s",Capteur,pCalcul->Nom);
		Cache = malloc(sizeof(CACHE));
		if(Cache==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour construire le cache (1).");
			return NULL;
		}
		Cache->Suivant = g_LstCache;
		g_LstCache = Cache;
		strcpy(Cache->Nom, Capteur);
		Cache->bLu = FALSE;
		Cache->LstCalcul = malloc(sizeof(LSTCALCUL));
		if(Cache->LstCalcul==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour construire le cache (2).");
			return NULL;
		}
		Cache->LstCalcul->Calcul = pCalcul;
		Cache->LstCalcul->Suivant = NULL;
		xpp_query(Cache->Nom);
		return Cache;
	}

	//Un Calcul est déjà lié à ce capteur
	LstCalcul = Cache->LstCalcul;
	while(LstCalcul != NULL)
	{
		if(LstCalcul->Calcul == pCalcul) return Cache;
		LstCalcul = LstCalcul->Suivant;
	}

	//Ajouter le calcul à la liste
	LstCalcul = malloc(sizeof(LSTCALCUL));
	if(LstCalcul==NULL)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour construire le cache (3).");
		return NULL;
	}
	LstCalcul->Calcul = pCalcul;
	LstCalcul->Suivant = Cache->LstCalcul;
	Cache->LstCalcul = LstCalcul;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Maj du cache de %s pour %s",Capteur,pCalcul->Nom);

	 return Cache;
}

/****************************************************************************************************************/
/* Initialiser le cache de capteurs																				*/
int Calcul_CacheInit(CALCUL *pCalcul, CACHE **pCache)
{
	CACHE		*Cache;
	CACHE		*TmpCache;
	CALCUL		*Calcul;
	LSTCACHE	*LstCache;
	LSTCACHE	*DerCache = NULL;
	int			PosDeb, PosFin;
	char		*Pos;
	char		Capteur[128];
	char		Formule[1024];
	char		Signe;


	//Initialisation
	Cache = *pCache;
	while(Cache != NULL)
	{
		TmpCache = Cache;
		Cache = TmpCache->Suivant;
		free(TmpCache);
	}
	Cache = NULL;

	//Construction du cache
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Constitution du cache...");
	Calcul = pCalcul;
	while(Calcul != NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Analyse de %s => %s.", Calcul->Nom, Calcul->Formule);

		//Filtrer la formule
		for(PosDeb=0; PosDeb <= strlen(Calcul->Formule); PosDeb++)
		{
			Signe = Calcul->Formule[PosDeb];
			if((Signe=='+')||(Signe=='*')||(Signe=='/')||(Signe=='&')||(Signe=='|')||(Signe=='>')||(Signe=='<')||(Signe=='(')||(Signe==')')) Signe = '#';
			Formule[PosDeb] = Signe;
		}

		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Formule filtrée %s.", Formule);

		//Rechercher les capteurs 
		PosDeb = 0;
		while(PosDeb<strlen(Formule))
		{
			Pos = strchr(Formule+PosDeb, '#'); 
			if(Pos==NULL)
				PosFin = strlen(Formule);
			else
				PosFin = (int)(Pos-Formule)-1;

			strncpy(Capteur, Formule+PosDeb, PosFin-PosDeb+1);
			Capteur[PosFin-PosDeb+1] = '\0';

			if(xpp_bIdCapteur(Capteur)==FALSE)
			{
				if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("%s n'est pas un capteur.", Capteur);
				PosDeb = PosFin+2;
				continue;
			}
		
			Cache = Calcul_CacheAjoute(Capteur, Calcul);

			LstCache = malloc(sizeof(LSTCACHE));
			if(LstCache==NULL)
			{
				if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour décomposer le calcul.");
				return FALSE;
			}

			LstCache->Capteur = Cache;
			LstCache->PosDeb  = PosDeb;
			LstCache->PosFin  = PosFin;
			LstCache->Suivant = NULL;

			if(Calcul->LstCache==NULL)
				Calcul->LstCache = LstCache;
			else
				DerCache->Suivant = LstCache;

			DerCache = LstCache;
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Cache compléter pour le capteur %s de %s.", Capteur, Calcul->Nom);
			PosDeb = PosFin+2;
		}
		xpp_query(Calcul->Sortie);		//Pour mémoriser l'état de la sortie au démarrage du module
		Calcul = Calcul->Suivant;
	}
	
	return TRUE;
}

/****************************************************************************************************************/
/* Les fonctions d'évaluation d'expression																		*/
/* http://bien-programmer.forum-actif.net/t36-evaluation-d-une-expression-arithmetique							*/
/* http://delahaye.emmanuel.free.fr/clib/																		*/


/* Calcul de 'G op D' */
double effectuer_oper (double G, char op, double D)
{
  double resultat = 0;

  switch (op)
  {
	  case '+':
		  resultat = G + D;
		  break;
	  case '-':
		  resultat = G - D;
		  break;
	  case '*':
		  resultat = G * D;
		  break;
	  case '/':
		  resultat = G / D;
		  break;
	  case '|':
		  resultat = (int)G | (int)D;
		  break;
	  case '&':
		  resultat = (int)G & (int)D;
		  break;
	  case '<':
		  //resultat = G < D;
		  if(G < D)
			  return 1;
		  else
			  return 0;
		  break;
	  case '>':
		  //resultat = G > D;
		  if(G > D)
			  return 1;
		  else
			  return 0;
		  break;
	  default:
		  resultat = 0;
  }

  return resultat;
}

/* Donne le nombre identifiant la priorité de l'opérateur donné */
unsigned int priorite (char op)
{
  unsigned int prio = 0;

  switch (op)
  {
	  case '+':
	  case '-':
		  prio = 1;
		  break;
	  case '*':
	  case '/':
		  prio = 2;
		  break;
	  case '|':
	  case '&':
	  case '>':
	  case '<':
		  prio = 3;
		  break;
  }

  return prio;
}

/* Renvoie une valeur positive si 'op' est un opérateur géré. */
int est_oper (char op)
{
  switch (op)
  {
	  case '+':
	  case '-':
	  case '*':
	  case '/':
	  case '|':
	  case '&':
	  case '>':
	  case '<':
		  return 1;
	  default:
		  return 0;
  }
}

double calculer_r (const char *calcul, double *G_dl, char *Op_dl,
                  unsigned int prio_op_pre, const char **p_calcul)
{
  double G, D;                /* opérande de gauche et opérande de droite */
  char op, op_suivant;

  /* *** initialisations *** */
  /* Si G_dl est un pointeur valide */
  if (G_dl != NULL)
  {
      G = *G_dl;
  }
  else
  {                            /* Sinon on lit G */
      if (*calcul == '(')
        G = calculer_r (calcul + 1, NULL, NULL, 0, &calcul);
      else
        G = strtod (calcul, (char **) &calcul);
  }

  /* Si Op_dl est un pointeur valide */
  if (Op_dl != NULL)
  {
      op = *Op_dl;
  }
  else
  {                            /* Sinon on lit l'opérateur */
      op = *calcul;
      calcul++;
  }

  /* *** boucle des calculs *** */
  while (op != '\0' && op != ')' && priorite (op) > prio_op_pre)
  {
      /* Lecture de l'opérande de droite */
      if (*calcul == '(')
        D = calculer_r (calcul + 1, NULL, NULL, 0, &calcul);
      else
        D = strtod (calcul, (char **) &calcul);

      /* Opérateur suivant */
      op_suivant = *calcul;
      calcul++;

      if (est_oper (op_suivant) && priorite (op_suivant) > priorite (op))
      {
        D = calculer_r (calcul, &D, &op_suivant, priorite (op), &calcul);
      }

      G = effectuer_oper (G, op, D);
      op = op_suivant;
  }
  /* *** fin de la boucle des calculs *** */

  /* Mise à jour de l'opérateur suivant pour la fonction appelante */
  if (Op_dl != NULL)
      *Op_dl = op_suivant;

  /* A pour effet d'indiquer à la fonction appelante jusqu'où
      la fonction appelée a lu la chaine 'calcul' */
  if (p_calcul != NULL)
      *p_calcul = calcul;

  return G;
}

char *str_dup (char const *s)
{
  char *sdup = NULL;
  size_t size = strlen (s) + 1;
  if (size > 1)
  {
      sdup = malloc (size);
      if (sdup != NULL)
      {
        memcpy (sdup, s, size);
      }
  }
  return sdup;
}

void str_nospace (char *s)
{
  char *r = s;
  char *w = s;

  while (*r != 0)
  {
      if (!isspace (*r))
      {
        *w = *r;
        w++;
      }
      r++;
  }
  *w = 0;
}

double calculer (const char *calcul)
{
  double val = 0;
  char *sdup = str_dup (calcul);
  if (sdup != NULL)
  {
      str_nospace (sdup);
      val = calculer_r (sdup, NULL, NULL, 0, NULL);
      free (sdup);
  }
  return val;
}

/****************************************************************************************************************/
/* Evaluer un calcul																							*/
int Calcul_Evaluer(CALCUL *Calcul)
{
	int			PosOri, PosDst;
	char		Formule[1024];
	char		State[16];
	char		Texte[16];
	LSTCACHE	*LstCache;
	double		Valeur;


for(PosOri=0;PosOri<1024;PosOri++) Formule[PosOri] = '\0';


	LstCache = Calcul->LstCache;
	Formule[0] = '\0';
	PosOri = 0;
	PosDst = 0;

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Evaluer le calcul %s => %s.", Calcul->Nom, Calcul->Formule);

	//Insérer les valeurs dans la formule
	while(LstCache!=NULL)
	{
		//La valeur de capteur est connu ?
		if(LstCache->Capteur->bLu == FALSE)
		{
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible d'évaluer le calcul %s car le capteur %s n'a pas encore été lu.", Calcul->Nom, LstCache->Capteur->Nom);
			return FALSE;
		}

		//Copier la portion de la formule situé avant le capteur
		while(PosDst<LstCache->PosDeb) Formule[PosOri++] = Calcul->Formule[PosDst++];

		//Insérer la valeur du capteur
		strcpy(Formule+PosOri, LstCache->Capteur->Valeur);
		PosOri = strlen(Formule);

		PosDst = LstCache->PosFin+1;
		LstCache = LstCache->Suivant;

		//Copier la fin de la formule
		if(LstCache==NULL) strcat(Formule, Calcul->Formule+PosDst);
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Traduction du calcul %s => %s.", Calcul->Nom, Formule);

	//Lancer le calcul
	Valeur = calculer(Formule);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Calcul %s = %f.", Formule, Valeur);

	//Envoyer l'info en cas de changement
	if(Calcul->Valeur == Valeur)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("La valeur reste inchangée.");
		return TRUE;
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fixer l'élément %s à %f", Calcul->Sortie, Valeur);
	Calcul->Valeur = Valeur;
	if(Calcul->Valeur>0)
		strcpy(State, "on");
	else
		strcpy(State, "off");
	sprintf(Texte, "%f", Calcul->Valeur);
	xpp_cmd(Calcul->Sortie, State, NULL, Texte);
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
			Calcul_LireIni();
			Calcul_CacheInit(g_LstCalcul, &g_LstCache);
		}
		return 0;
	}

	return 1;
}

int xpp_handler_Calcul(int i_TypMsg)
{
	CACHE		*Cache;
	LSTCALCUL	*LstCalcul;
	CALCUL		*Calcul;

	char i_source[128];
	char i_temp[64];
	int	Old_bLu;


	//Rechercher dans le cache et dans la liste des calculs
	if (xpp_GetSourceName(i_source)==-1) return 0;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche dans le cache de %s.",i_source);
	Cache = Calcul_CacheRecherche(i_source);
	Calcul= Calcul_SortieRecherche(i_source);
	if((Cache == NULL)&&(Calcul == NULL)) return 0;

	//Lire la valeur
	if (xpp_GetTargetText(i_temp)==-1) return 0;
	if (atof(i_temp) == 0)
	{
		if (xpp_GetTargetState(i_temp)==-1) return 0;
		if (STRICMP(i_temp, "on")==0)
			strcpy(i_temp, "1");
		else
			strcpy(i_temp, "0");
	}
	//Ecrire l'état du Calcul
	if(Calcul != NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Mémorisation de l'état du Calcul %s=%s.",Calcul->Nom, i_temp);
		Calcul->Valeur = atof(i_temp);
		if(Cache == NULL) return 1;
	}

	//Ecrire dans le cache
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Mise en cache %s=%s.",i_source, i_temp);
	strcpy(Cache->Valeur, i_temp);
	Old_bLu = Cache->bLu;
	Cache->bLu = TRUE;

	//Déclencher les calculs uniquement sur message EVENT
	//	sauf au démarrage du service :
	//	déclenchement des calculs tant que le cache n'est pas entièrement chargé.
	if( (i_TypMsg==XPP_RECEP_CAPTEUR_INFO) && (Old_bLu==TRUE) ) return 0;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche des calculs à traiter.");
	LstCalcul = Cache->LstCalcul;
	while(LstCalcul!=NULL)
	{
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Traitement de %s.",LstCalcul->Calcul->Nom);
		Calcul_Evaluer(LstCalcul->Calcul);
		LstCalcul = LstCalcul->Suivant;
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
	g_receiver_sockfd = Calcul_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);
	Calcul_LireIni();

	//******************************************************************************************************************
	//*** Etablir la liste des capteurs à surveiller
	Calcul_CacheInit(g_LstCalcul, &g_LstCache);

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
			// there was an incoming message, not that we care
			if (xpp_PollIncoming(g_receiver_sockfd, i_xpp_buff, sizeof(i_xpp_buff))>0)
			{
				switch(xpp_DispatchReception(i_xpp_buff))
				{
					case XPP_RECEP_CAPTEUR_INFO :
						if(!g_bServicePause) xpp_handler_Calcul(XPP_RECEP_CAPTEUR_INFO);
						break;
					case XPP_RECEP_CAPTEUR_EVENT :
						if(!g_bServicePause) xpp_handler_Calcul(XPP_RECEP_CAPTEUR_EVENT);
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
