const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-ProgHebdo";
const char* XAP_GUID = "FFF00800";
const char* XAP_DEFAULT_INSTANCE = "xAP-ProgHebdo";

/*
 *  Les structures du Programmateur Hebdomadaire
 */
typedef struct _Horaire {
	struct _Horaire *Suivant;
	int Debut;
	int Fin;
} HORAIRE;

typedef struct _Plage {
	struct _Plage *Suivant;
	int Jours[7];
	HORAIRE *Horaire;
	double Valeur;
} PLAGE;

typedef struct _ProgHebdo {
	struct _ProgHebdo *Suivant;
	int No;
	char Nom[128];
	double Defaut;
	double Valeur;
	int bInit;
	char Sortie[128];
	PLAGE *Plages;
} PRGHBDO;

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
#endif

/*
 *  Prototype
 */
int Plage_Jour2Num(char *Jour);
int Plage_Heure2Num(char *Heure);
