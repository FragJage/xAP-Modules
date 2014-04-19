const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-Rss";
const char* XAP_GUID = "FFF00500";
const char* XAP_DEFAULT_INSTANCE = "xAP-Rss";

/*
 *  Les structures RSS
 */
typedef struct _Capteur {
	struct _Capteur *Suivant;
	char xapSource[128];
	char xapClasse[128];
	char xapValeur[128];
	char Valeur[32];
	char Unite[32];
	char rssNom[128];
	char rssLien[256];
	time_t timestamp;
} CAPTEUR;

typedef struct _FichierRss {
	struct _FichierRss *Suivant;
	char Fichier[256];
	char Titre[128];
	char Description[512];
	char Lien[256];
	int	 Validite;
	CAPTEUR *LstCapteurs;
} FICHIERRSS;

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
#endif