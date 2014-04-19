/*
 *  La structure CACHE
 */
typedef struct _DBCache {
	struct _DBCache *Suivant;
	char *Source;
	char *Valeur;
	char Periode;
	time_t Time;
} DBCACHE;

#define CACHE_DST_FICHIER 1
#define CACHE_DST_SQL 2

int DBCache_Stop();
int DBCache_Init();
int DBCache_VidageFic();
int DBCache_VidageMem();
int DBCache_Ajoute(char *i_source, char *i_valeur, char i_periode);

int DBmysql_Connexion();
int DBmysql_Deconnexion();
int DBmysql_EnregistrerValeur(char *i_source, char *i_valeur, time_t i_time);
int DBmysql_EnregistrerValeurPeriode(char *i_source, char *i_valeur, char i_periode, time_t i_time);
int DBmysql_LireValeurPeriode(char *i_source, char *i_valeur, char i_periode);
int DBmysql_SupprimerValeur(char *i_source, char *i_date);
