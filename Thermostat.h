const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-Thermostat";
const char* XAP_GUID = "FFF00400";
const char* XAP_DEFAULT_INSTANCE = "xAP-Thermostat";

/*
 *  Les structures THERMOSTAT
 */
typedef struct _Thermovalo {
	int bActif;
	float Entree;
	float Consigne;
	float Hysteresis;
	float Differentiel;
	float DiffEcartON;
	float DiffEcartOFF;
} THERMOVALO;

typedef struct _Thermostat {
	struct _Thermostat *Suivant;
	char Actif[128];
	char Nom[128];
	char Entree[128];
	char Consigne[128];
	char Hysteresis[128];
	char Sortie[128];
	char CommandeON[128];
	char CommandeOFF[128];
	char Differentiel[128];
	char DiffEcartON[128];
	char DiffEcartOFF[128];
	BOOL Inverser;
	struct _Thermovalo Valeurs;
	int Etat;
} THERMOSTAT;

typedef struct _LstThermo {
	struct _Thermostat *Thermostat;
	struct _LstThermo *Suivant;
} LSTTHERMO;

typedef struct _Cache {
	struct _Cache *Suivant;
	char Nom[128];
	char Valeur[48];
	int bLu;
	struct _LstThermo *LstThermo;
} CACHE;

#ifndef ETAT_ON
	#define ETAT_NC		0
	#define ETAT_ON		1
	#define ETAT_OFF	-1
#endif

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
#endif