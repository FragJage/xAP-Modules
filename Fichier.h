//
//  MODULE: fichier.h
//
#include<string.h>
#ifndef _FICHIER_H
#define _FICHIER_H

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_OFF 0
#define DEBUG_INFO 1
#define DEBUG_VERBOSE 2
#define DEBUG_DEBUG 3
#define DEBUG_TXRX 4

extern g_debuglevel;

//////////////////////////////////////////////////////////////////////////////
typedef struct _Param {
	struct _Param *Suivant;
	char Cle[128];
	char Valeur[1024];
} PARAM;

//////////////////////////////////////////////////////////////////////////////
#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
	typedef int    BOOL;
#endif

#ifdef WIN32
	#define STRICMP		_stricmp
	#define STRNICMP	_strnicmp
#endif
#ifdef LINUX
	#define STRICMP		strcasecmp
	#define STRNICMP	strncasecmp
#endif

//////////////////////////////////////////////////////////////////////////////
char g_FichierChemin[256];
char g_FichierNom[256];
char g_FichierLog[256];

//////////////////////////////////////////////////////////////////////////////
#define TYPFIC_LOC	1
#define TYPFIC_CNF	2
#define TYPFIC_LOG	4
#define TYPFIC_DATA	6

//////////////////////////////////////////////////////////////////////////////
void FichierInit(char *Chemin, char *Nom);
void FichierExt(char *Fichier, char *Ext);
void FichierStd(char *Fichier, int Type);
void CheminStd(char *Rep, int Lng, int Type);

int Fcnf_Lire(char *Fichier, PARAM **LstParam);
int Fcnf_Valeur(PARAM *LstParam, char *Cle, char *Valeur);
int Fcnf_Section(PARAM *LstParam, int No, char *Section);

void Flog_Init(char *Fichier);
int Flog_Ecrire(char *Format, ...);

#ifdef __cplusplus
}
#endif

#endif
