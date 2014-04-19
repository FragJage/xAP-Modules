const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-Chrono";
const char* XAP_GUID = "FFF00900";
const char* XAP_DEFAULT_INSTANCE = "xAP-Chrono";

/*
 *  Les structures CHRONO
 */
typedef struct _Chrono {
	struct _Chrono *Suivant;
	int	No;
	char Nom[128];
	char Entree[128];
	int RazMultiple;
	char RazUnite;		//m, h, J, M, A : minute, heure, Jour, Mois, Année.
	int RazHeure;
	int RazMinute;
	char Unite;			//h, m, s : heure, minute, seconde
	BOOL bMemo;
	double Duree;
	time_t Time;
	time_t TimeRaz;
	int Etat;
} CHRONO;

#ifndef ETAT_ON
	#define ETAT_NC		0
	#define ETAT_ON		1
	#define ETAT_OFF	-1
#endif

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
#endif