#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include <time.h>
#include "Service.h"
#include "Fichier.h"
#include "xpp.h"
#include "Rss.h"

#ifdef WIN32
#define close closesocket
#endif


int g_bServiceStop = FALSE;
int g_bServicePause = FALSE;
int g_receiver_sockfd;

PARAM *g_LstParam = NULL;
FICHIERRSS *g_LstFichierRss = NULL;
char FichierCnf[256];


/****************************************************************************************************************/
/* Filtrer les capteurs et classe grâce à la liste de filtres													*/
CAPTEUR *Capteur_Chercher(CAPTEUR *LstCapteur, char *i_Source, char*i_Classe)
{
	int		bFin;
	CAPTEUR	*Capteur;	


	if(LstCapteur==NULL) return NULL;

	//Initialisation
	bFin = FALSE;
	Capteur = LstCapteur;

	while(bFin==FALSE)
	{
		bFin = TRUE;
		if(Capteur->xapClasse[0] != '\0') bFin = xpp_compare(i_Classe, Capteur->xapClasse);
		if((bFin==TRUE)&&(Capteur->xapSource[0] == '*')) strcpy(Capteur->xapSource, i_Source);
		if((bFin==TRUE)&&(Capteur->xapSource[0] != '\0')) bFin = xpp_compare(i_Source, Capteur->xapSource);
		
		if(bFin==FALSE)
		{
			Capteur = Capteur->Suivant;
			if(Capteur==NULL) bFin = TRUE;
		}
	}

	return Capteur;
}

/****************************************************************************************************************/
/* Initialisation du module Rss																					*/
int Rss_init()
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

	return xpp_init(i_uniqueID, i_interfacename, i_interfaceport, i_instance, i_debuglevel);
}

/****************************************************************************************************************/
/* Lire le paramétrage des flux rss dans le fichier de config													*/
int Rss_LireIni()
{
	int			i,j;
	char		Cle[128];
	char		Section[128];
	char		Temp[256];
	FICHIERRSS	*FichierRss;
	CAPTEUR		*Capteur;
	

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Parcours des sections du fichier de conf.");

	//Parcourir les sections
	i=1;
	while(Fcnf_Section(g_LstParam, i, Section))
	{
		//Sauter la section XAP
		if(STRICMP(Section, "XAP")==0)
		{
			i++;
			continue;
		}

		//Section valide ?
		sprintf(Cle, "%s_Fichier", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, Temp))
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Section %s ignorée car pas de fichier rss.", Section);
			i++;
			continue;
		}

		//Allocation structure
		FichierRss = malloc(sizeof(FICHIERRSS));
		if(FichierRss==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour stocker les params rss.");
			return FALSE;
		}
		FichierRss->LstCapteurs = NULL;
		

		//Lecture des paramètres
		strcpy(FichierRss->Fichier, Temp);

		sprintf(Cle, "%s_Titre", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, FichierRss->Titre)) strcpy(FichierRss->Titre, "xAP2Rss");

		sprintf(Cle, "%s_Description", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, FichierRss->Description)) strcpy(FichierRss->Description, "Conversion de messages xAP en flux RSS");

		sprintf(Cle, "%s_Lien", Section);
		if(!Fcnf_Valeur(g_LstParam, Cle, FichierRss->Lien)) FichierRss->Lien[0]='\0';

		sprintf(Cle, "%s_Validite", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Temp))
			FichierRss->Validite = atoi(Temp);
		else
			FichierRss->Validite = 3600;

		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Flux Rss %s %s %s", FichierRss->Titre, FichierRss->Description, FichierRss->Lien);

		//Lecture du fichier de conf du flux
		sprintf(Cle, "%s_Conf", Section);
		if(Fcnf_Valeur(g_LstParam, Cle, Temp)) Rss_LireIniRss(Temp, FichierRss);

		//Config auto du flux
		if(FichierRss->LstCapteurs==NULL) for(j=0;j<20;j++)
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Affectation automatique pour les 20 premiers capteurs détectés.");

			//Allocation structure
			Capteur = malloc(sizeof(CAPTEUR));
			if(Capteur==NULL)
			{
				j=20;
				continue;
			}
			//Initialisation structure
			strcpy(Capteur->xapSource, "*");
			strcpy(Capteur->xapClasse, "*.event");
			strcpy(Capteur->xapValeur, "input.state:Text");
			strcpy(Capteur->Valeur, "<--Not-Set-->");
			Capteur->Unite[0] = '\0';
			Capteur->rssNom[0] = '\0';
			Capteur->rssLien[0] = '\0';

			Capteur->Suivant = FichierRss->LstCapteurs;
			FichierRss->LstCapteurs = Capteur;
		}

		FichierRss->Suivant = g_LstFichierRss;
		g_LstFichierRss = FichierRss;

		i++;
	}

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de parcours des sections du fichier de conf.");
	return TRUE;
}

/****************************************************************************************************************/
/* Lire le paramétrage des flux rss dans le fichier de config													*/
int Rss_LireIniRss(char *FicConf, FICHIERRSS *FichierRss)
{
	int			i;
	char		Cle[128];
	char		Section[128];
	char		Fichier[128];
	PARAM		*LstParam;
	CAPTEUR		*Capteur;

	//Charger le fichier en mémoire
	CheminStd(Cle, sizeof(Cle), TYPFIC_CNF);
	sprintf(Fichier, "%s%s", Cle, FicConf);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Lecture du fichier %s.", Fichier);
	LstParam = NULL;
	if(!Fcnf_Lire(Fichier, &LstParam))
	{
			Flog_Ecrire("Impossible de lire le fichier %s.", Fichier);
			return FALSE;
	}

	//Parcourir les sections
	i=1;
	while(Fcnf_Section(LstParam, i, Section))
	{
		//Allocation structure
		Capteur = malloc(sizeof(CAPTEUR));
		if(Capteur==NULL)
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Pas assez de mémoire pour stocker les capteurs.");
			return FALSE;
		}

		sprintf(Cle, "%s_Capteur", Section);
		if(!Fcnf_Valeur(LstParam, Cle, Capteur->xapSource))
		{
			if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Section %s ignorée car pas de capteur.", Section);
			free(Capteur);
			i++;
			continue;
		}

		sprintf(Cle, "%s_Classe", Section);
		if(!Fcnf_Valeur(LstParam, Cle, Capteur->xapClasse)) strcpy(Capteur->xapClasse, "*.event");

		sprintf(Cle, "%s_Valeur", Section);
		if(!Fcnf_Valeur(LstParam, Cle, Capteur->xapValeur)) strcpy(Capteur->xapValeur, "input.state:Text");

		sprintf(Cle, "%s_Unite", Section);
		if(!Fcnf_Valeur(LstParam, Cle, Capteur->Unite)) Capteur->Unite[0] = '\0';

		sprintf(Cle, "%s_Nom", Section);
		if(!Fcnf_Valeur(LstParam, Cle, Capteur->rssNom)) strcpy(Capteur->xapClasse, Capteur->xapSource);

		sprintf(Cle, "%s_Lien", Section);
		if(!Fcnf_Valeur(LstParam, Cle, Capteur->rssLien)) Capteur->rssLien[0] = '\0';

		strcpy(Capteur->Valeur, "<--Not-Set-->");

		Capteur->Suivant = FichierRss->LstCapteurs;
		FichierRss->LstCapteurs = Capteur;

		if (g_debuglevel>=DEBUG_VERBOSE)
		{
			Flog_Ecrire("Ajout du capteur %s xapClasse:%s xapValeur:%s.", Capteur->xapSource, Capteur->xapClasse, Capteur->xapValeur);
			Flog_Ecrire("	-> nom:%s unité:%s lien:%s", Capteur->rssNom, Capteur->Unite, Capteur->rssLien);
		}

		i++;
	}

	Fcnf_Free(LstParam);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de lecture du fichier %s.", FicConf);
}

/****************************************************************************************************************/
/* Génération du fichier Rss																					*/
int Rss_Generer(FICHIERRSS *FichierRss)
{
	FILE	*hFic;
	CAPTEUR	*Capteur;
	char	i_buff[128];
	char	i_date[64];
	time_t	current_date;


	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Génération du fichier rss : %s.", FichierRss->Fichier);

	if((hFic = fopen(FichierRss->Fichier, "w")) == NULL)
	{
		if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible d'ouvrir le fichier %s en écriture.", FichierRss->Fichier);
		return FALSE;
	}

	current_date = time(NULL);
	strftime(i_date, sizeof(i_date), "%a, %m %b %Y %H:%M:%S", localtime(&current_date));

	fputs("<?xml version='1.0' encoding='iso-8859-1'?>\n<rss version='2.0'>\n<channel>\n", hFic);
	fprintf(hFic, "\t<title>%s</title>\n\t<description>%s</description>\n\t<lastBuildDate>%s</lastBuildDate>\n", FichierRss->Titre, FichierRss->Description, i_date);
	if(FichierRss->Lien[0]!='\0') fprintf(hFic, "\t<link>%s</link>\n", FichierRss->Lien);

	Capteur = FichierRss->LstCapteurs;
	while(Capteur!=NULL)
	{
		if(strcmp(Capteur->Valeur, "<--Not-Set-->")==0)
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le capteur %s (%s) n'a pas encore été vu.", Capteur->xapSource, Capteur->rssNom);
			Capteur = Capteur->Suivant;
			continue;
		}

		if(time(NULL)-Capteur->timestamp>FichierRss->Validite)
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("La valeur %s du capteur %s n'est plus valide.", Capteur->Valeur, Capteur->xapSource);
			Capteur = Capteur->Suivant;
			continue;
		}

		if(Capteur->Unite[0] == '\0')
			sprintf(i_buff, "%s %s", Capteur->rssNom, Capteur->Valeur);
		else
			sprintf(i_buff, "%s %s%s", Capteur->rssNom, Capteur->Valeur, Capteur->Unite);
		fputs("\t<item>\n", hFic);
		fprintf(hFic, "\t\t<title>%s</title>\n\t\t<description>%s</description>\n\t\t<pubDate>%s GMT</pubDate>\n", i_buff, i_buff, i_date);
		if(Capteur->rssLien[0]!='\0') fprintf(hFic, "\t\t<link>%s</link>\n", Capteur->rssLien);
		fputs("\t</item>\n", hFic);
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Ajout du capteur %s au flux rss : %s %s°", Capteur->xapSource, Capteur->rssNom, Capteur->Valeur);
		
		Capteur = Capteur->Suivant;
	}

    fputs("</channel>\n</rss>\n", hFic);
	fclose(hFic);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Fin de génération du fichier rss : %s.", FichierRss->Fichier);
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
			Rss_LireIni();
		}
		return 0;
	}

	return 1;
}

int xpp_handler_Rss(int i_TypMsg)
{
	FICHIERRSS	*FichierRss;
	CAPTEUR		*Capteur;
	char		*p;

	char i_source[XPP_MAX_KEYVALUE_LEN];
	char i_class[XPP_MAX_KEYVALUE_LEN];
	char i_msg[XPP_MAX_KEYVALUE_LEN];
	char i_temp[XPP_MAX_KEYVALUE_LEN];

	i_msg[0]='\0';

	if (xpp_GetTargetName(i_temp)!=-1) return 0; //C'est pour un destinataire précis
	if(g_bServicePause) return 0;

	//Lire la source et la classe
	if (xpp_GetSourceName(i_source)==-1) return 0;
	if (xpp_GetClassName(i_class)==-1) return 0;

	//Rechercher le capteur parmi les flux rss
	FichierRss=g_LstFichierRss;
	while(FichierRss!=NULL)
	{
		//Rechercher le capteur dans le flux
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Recherche du capteur %s class %s est dans le flux %s.", i_source, i_class, FichierRss->Titre);
		Capteur = Capteur_Chercher(FichierRss->LstCapteurs, i_source, i_class);
		if(Capteur==NULL)
		{
			if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le capteur %s n'est pas dans le flux %s.", i_source, FichierRss->Titre);
			FichierRss = FichierRss->Suivant;
			continue;
		}
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Le capteur %s est dans le flux %s.", Capteur->xapSource, FichierRss->Titre);

		//Lire la valeur du capteur
		if (xpp_GetCmd(Capteur->xapValeur, i_temp)==-1)
		{
			Flog_Ecrire("Impossible de lire la valeur '%s' du capteur '%s'.", Capteur->xapValeur, Capteur->xapSource);
			FichierRss = FichierRss->Suivant;
			continue;
		}

		strcpy(Capteur->Valeur, i_temp);
		Capteur->timestamp = time(NULL);
		if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Valeur de %s : %s", Capteur->xapSource, Capteur->Valeur);

		//Générer le flux
		Rss_Generer(FichierRss);

		FichierRss = FichierRss->Suivant;
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


	//******************************************************************************************************************
	//*** Initialisation générale
	FichierInit(g_ServiceChemin, g_ServiceNom);
	FichierStd(Fichier, TYPFIC_LOG);
	Flog_Init(Fichier);

	//******************************************************************************************************************
	//*** Bavardage
	Flog_Ecrire("Démarrage de xAP-Rss");

	//******************************************************************************************************************
	//*** Lecture du fichier INI
	FichierStd(FichierCnf, TYPFIC_CNF);
	if(!Fcnf_Lire(FichierCnf, &g_LstParam))
	{
		FichierStd(FichierCnf, TYPFIC_LOC+TYPFIC_CNF);
		Fcnf_Lire(FichierCnf, &g_LstParam);
	}
	Flog_Ecrire("Fichier de conf : %s", FichierCnf);
	g_receiver_sockfd = Rss_init();
	if (g_debuglevel>0) Flog_Ecrire("DebugLevel : %d", g_debuglevel);

	//******************************************************************************************************************
	//*** Lecture du fichier filtre
	Rss_LireIni();

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
						if(!g_bServicePause) xpp_handler_Rss(XPP_RECEP_CAPTEUR_INFO);
						break;
					case XPP_RECEP_CAPTEUR_EVENT :
						if(!g_bServicePause) xpp_handler_Rss(XPP_RECEP_CAPTEUR_EVENT);
						break;
					case XPP_RECEP_SERVICE_CMD :
						xpp_handler_service();
						break;
				}
			}
		}
	} // while    

	Flog_Ecrire("Arrêt de xAP-Rss");
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
