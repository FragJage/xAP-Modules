#define XPP_RECEP_CAPTEUR_CMD 1
#define XPP_RECEP_CAPTEUR_QUERY 2
#define XPP_RECEP_SERVICE_CMD 3
#define XPP_RECEP_CAPTEUR_INFO 4
#define XPP_RECEP_CAPTEUR_EVENT 5

#define XPP_MSG_HBEAT 1
#define XPP_MSG_ORDINARY 2
#define XPP_MSG_CONFIG_REQUEST 3
#define XPP_MSG_CACHE_REQUEST 4
#define XPP_MSG_CACHE_REPLY 5
#define XPP_MSG_CONFIG_REPLY 6

#define XPP_MAX_KEYVALUE_LEN 512
#define HBEAT_INTERVAL 60

//Fonctions initialisation
int xpp_init(char *uid, char *interfacename, int interfaceport, char *instance, int debuglevel);

//Fonctions Messages
int xpp_heartbeat_tick(int interval);
int xpp_query(char *Capteur);
int xpp_cmd(char *Capteur, char *State, char *Level, char *Text);
int xpp_event(int No, char *Capteur, int State, int Level, double Temp);
int xpp_info(int No, char *Capteur, int State, int Level, double Temp, char *Destinataire);

//Fonctions extraction d'infos
int xpp_GetSourceName(char *source);
int xpp_GetClassName(char *Class);
int xpp_GetTargetId();
int xpp_GetTargetName(char *Capteur);
int xpp_GetTargetState(char *State);
int xpp_GetTargetText(char *Text);
int xpp_GetCmd(char *Cle, char *Valeur);
int xpp_bIdCapteur(char *Capteur);
int xpp_MessageType();

int xpp_compare(const char* a_item1, const char* a_item2);

//Fonctions Service
int xpp_PollIncoming(int a_server_sockfd, char* a_buffer, int a_buffer_size);
int xpp_DispatchReception(const char *msg);

//Fonctions extraction d'infos sur message hbeat
int xpp_GetHbeat(char *Source, int *Interval);
