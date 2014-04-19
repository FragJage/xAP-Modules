const char* XAP_ME = "FRAGXAP";
const char* XAP_SOURCE = "xAP-Database"; 
const char* XAP_GUID = "FFF00100";
const char* XAP_DEFAULT_INSTANCE = "xAP-Database";

/*
 *  La structure FILTRE
 */
typedef struct _Filtre {
	struct _Filtre *Suivant;
	char Nom[128];
	char Source[128];
	char Classe[128];
	char Periode;			//[m]inute, [h]eure, [J]our, [M]ois, [A]nnee
	time_t TimeRaz;
	int TypeValeur;
	int Mode;
	float Memo;
} FILTRE;

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
	typedef int    BOOL;
#endif

#ifndef MODE_LOG
	#define MODE_LOG	1
	#define MODE_ETAT	2
	#define MODE_MIN	3
	#define MODE_MAX	4
#endif

#ifndef TYPVAL_TXT
	#define TYPVAL_TXT	1
	#define TYPVAL_STA	2
#endif