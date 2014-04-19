const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-Calcul";
const char* XAP_GUID = "FFF00700";
const char* XAP_DEFAULT_INSTANCE = "xAP-Calcul";

/*
 *  Les structures de calcul
 */
typedef struct _Cache {
	struct _Cache *Suivant;
	char Nom[128];
	char Valeur[48];
	int bLu;
	struct _LstCalcul *LstCalcul;
} CACHE;

typedef struct _LstCache {
	struct _LstCache *Suivant;
	CACHE *Capteur;
	int PosDeb;
	int PosFin;
} LSTCACHE;

typedef struct _Calcul {
	struct _Calcul *Suivant;
	char Nom[128];
	char Sortie[128];
	char Formule[1024];
	double Valeur;
	struct _LstCache *LstCache;
} CALCUL;

typedef struct _LstCalcul {
	struct _Calcul *Calcul;
	struct _LstCalcul *Suivant;
} LSTCALCUL;

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
#endif