const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-Fictif";
const char* XAP_GUID = "FFF00600";
const char* XAP_DEFAULT_INSTANCE = "xAP-Fictif";

#define FICTIF_TYPE_INCONNU		-1
#define FICTIF_TYPE_BOOLEEN		1
#define FICTIF_TYPE_NUMERIQUE	2
#define FICTIF_TYPE_LEVEL		3
/*
 *  Les structures FICTIF
 */
typedef struct _Capteur {
	struct _Capteur *Suivant;
	int	  No;
	char  Nom[128];
	double Valeur;
	int   Type;
	BOOL bMemo;
} CAPTEUR;

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
#endif