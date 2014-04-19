const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-Http";
const char* XAP_GUID = "FFF00A00";
const char* XAP_DEFAULT_INSTANCE = "xAP-Http";

typedef struct _Capteur 
{
	struct _Capteur *Suivant;
	char	Id[64];				//Nom du capteur
	time_t	TimeRecu;			//Heure de reception de la valeur
	char	Valeur[32];			//Valeur du capteur
} CAPTEUR;

typedef struct _Service
{
	struct _Service *Suivant;
	char	Id[64];				//Nom du service
	time_t	TimeRecu;			//Heure de reception de la valeur
	int		Interval;			//Interval du Hbeat
} SERVICE;

typedef struct _Client
{
	struct _Client *Suivant;
	char	Type;				//Type de demande
	char	Capteur[64];		//Nom du capteur
	time_t	TimeDemande;		//Timestamp de la demande
	SOCKET	Socket;				//Socket Client
} CLIENT;

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
#endif