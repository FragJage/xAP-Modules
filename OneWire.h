const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-ONEWIRE";
const char* XAP_GUID = "FFF00200";
const char* XAP_DEFAULT_INSTANCE = "xAP-ONEWIRE";

#ifdef WIN32
	#define PORT1WIRE "\\\\.\\DS2490-1"
#else
	#define PORT1WIRE "DS2490-1"
#endif

/*
 *  La structure CAPTEUR
 */
typedef struct _Capteur 
{
	struct _Capteur *Suivant;
	int		No;					//Numéro de capteur dans la structure
	char	Id[32];				//Chemin du capteur
	char	Nom[64];			//Nom logique du capteur
	float	ValeurFloat;		//Valeur
	int		Valeur1;			//Valeur
	int		Valeur2;			//Valeur
	time_t	DerniereLecture;	//Timestamp de dernière lecture
	int		Interval;			//Interval de lecture
	int		bMasquer;			//Masquer le capteur
	char	Code;				//Code fictif
} CAPTEUR;
