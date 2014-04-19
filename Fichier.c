#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "Fichier.h"

int g_debuglevel=0;

/****************************************************************************************/
/* Initialisation du module																*/
void FichierInit(char *Chemin, char *Nom)
{
	strcpy(g_FichierChemin, Chemin);
	strcpy(g_FichierNom, Nom);
}

/****************************************************************************************/
/* Modifier l'extention																	*/
void FichierExt(char *Fichier, char *Ext)
{
	char *ptr;

	ptr = strrchr(Fichier, '.');
	if(ptr!=NULL) strcpy(ptr+1, Ext);
}

/****************************************************************************************/
/* Retrouver le profile windows															*/
#ifdef WIN32
#include <windows.h>
typedef BOOL (STDMETHODCALLTYPE FAR * LPFNGETUSERPROFILEDIR) (HANDLE hToken, LPTSTR lpProfileDir, LPDWORD lpcchSize);
BOOL TrouveProfile(char *Repertoire, int *Longueur)
{
    HANDLE  hToken;
	LPFNGETUSERPROFILEDIR   GetUserProfileDirectory = NULL;
	HMODULE                 hUserEnvLib             = NULL;


    hUserEnvLib = LoadLibrary("userenv.dll");
    if(!hUserEnvLib) return FALSE;

    GetUserProfileDirectory = (LPFNGETUSERPROFILEDIR) GetProcAddress( hUserEnvLib, "GetUserProfileDirectoryA" );
    if(!GetUserProfileDirectory) return FALSE;
    
    if((OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) == 0) return FALSE;

    if((GetUserProfileDirectory(hToken, Repertoire, (LPDWORD) Longueur)) == 0)
    {
		CloseHandle(hToken);
		return FALSE;
    }

    CloseHandle(hToken);

    return TRUE;
}
#endif

/****************************************************************************************/
/* Chemin et nom d'un fichier standard													*/
#ifdef WIN32
void FichierStd(char *Fichier, int Type)
{
	char Rep[256];
	char Ext[16];
	int	 Typ;
	int	 Lng;


	Typ = Type;
	if((Typ&TYPFIC_LOC)!=TYPFIC_LOC)
	{
		strcpy(Ext, "ini");
		if(Typ&TYPFIC_LOG) strcpy(Ext, "log");
		Typ = TYPFIC_LOC;
	}

	switch(Typ)
	{
		case TYPFIC_LOC: 
			sprintf(Fichier, "%s%s.%s", g_FichierChemin, g_FichierNom, Ext);
			break;
		case TYPFIC_CNF:
			TrouveProfile(Rep, &Lng);
			sprintf(Fichier, "%s%s.ini", Rep, g_FichierNom);
			break;
		case TYPFIC_LOG:
			TrouveProfile(Rep, &Lng);
			sprintf(Fichier, "%s%s.log", Rep, g_FichierNom);
			break;
		case TYPFIC_DATA:
			TrouveProfile(Rep, &Lng);
			sprintf(Fichier, "%s%s.txt", Rep, g_FichierNom);
			break;
	}
}
#else
void FichierStd(char *Fichier, int Type)
{
	char Rep[256];
	char Ext[16];
	int	 Typ;

	Typ = Type;
	if(Typ&TYPFIC_LOC!=TYPFIC_LOC)
	{
		strcpy(Ext, "conf");
		if(Typ&TYPFIC_LOG) strcpy(Ext, "log");
		Typ = TYPFIC_LOC;
	}

	switch(Typ)
	{
		case TYPFIC_LOC: 
			sprintf(Fichier, "%s%s.%s", g_FichierChemin, g_FichierNom, Ext);
			break;
		case TYPFIC_CNF:
			sprintf(Fichier, "/etc/%s.conf", g_FichierNom);
			break;
		case TYPFIC_LOG:
			sprintf(Fichier, "/var/log/%s.log", g_FichierNom);
			break;
		case TYPFIC_DATA:
			sprintf(Fichier, "/var/lib/%s.txt", g_FichierNom);
			break;
	}
}
#endif

/****************************************************************************************/
/* Chemins standard													*/
#ifdef WIN32
void CheminStd(char *Rep, int Lng, int Type)
{
	switch(Type)
	{
		case TYPFIC_LOC: 
			strncpy(Rep, g_FichierChemin, Lng);
			break;
		case TYPFIC_CNF:
			TrouveProfile(Rep, &Lng);
			break;
		case TYPFIC_LOG:
			TrouveProfile(Rep, &Lng);
			break;
	}
}
#else
void CheminStd(char *Rep, int Lng, int Type)
{
	switch(Type)
	{
		case TYPFIC_LOC: 
			strncpy(Rep, g_FichierChemin, Lng);
			break;
		case TYPFIC_CNF:
			sprintf(Rep, "/etc/%s/", g_FichierNom);
			break;
		case TYPFIC_LOG:
			strncpy(Rep, "/var/log/", Lng);
			break;
	}
}
#endif

/****************************************************************************************/
/* Libérer mémoire INI																	*/
void Fcnf_Free(PARAM **pParam)
{
	PARAM	*Param;
	PARAM	*LstParam;
	

	//Initialisation
	LstParam = *pParam;
	while(LstParam != NULL)
	{
		Param = LstParam;
		LstParam = Param->Suivant;
		free(Param);
	}

	*pParam = NULL;
}

/****************************************************************************************/
/* Lire le fichier INI																	*/
int Fcnf_Lire(char *Fichier, PARAM **pParam)
{
	int		i;
	char	msg[1024];
	char	block[128];
	char	*p;
	FILE	*hFic;
	PARAM	*Param;
	PARAM	*LstParam;
	

	//Initialisation
	block[0]='\0';
	Fcnf_Free(pParam);
	LstParam = NULL;
	*pParam = NULL;

	//Parcourir le fichier
    if((hFic = fopen(Fichier, "r")) == NULL) return FALSE;
	while( (!feof(hFic)) && (fgets(msg, sizeof(msg), hFic)) )
	{
		p = msg+strlen(msg)-1;
		while((*p=='\n')||(*p=='\r')) p--;
		*(p+1) = '\0';
		if(msg[0]=='\0') continue;

		if(msg[0]=='[')
		{
			msg[strlen(msg)-1] = '\0';
			strcpy(block, msg+1);
			strcat(block, "_");
		}
		else
		{
			Param = (PARAM *)malloc(sizeof(PARAM));
			if(Param==NULL) { fclose(hFic); return TRUE; }
			strcpy(Param->Cle, block);
			Param->Valeur[0] = '\0';
			Param->Suivant = NULL;
			
			if(*pParam == NULL) *pParam = Param;
			if(LstParam!= NULL) LstParam->Suivant = Param;
			LstParam = Param;

			p = msg;
			i = (int)strlen(Param->Cle);
			while((*p!='=')&&(*p!='\0')) Param->Cle[i++] = *(p++); Param->Cle[i]='\0';
			if(*p!='\0')
			{
				*(p++);
				i=0; 
				while((*p!='\0')&&(*p!='\n')&&(*p!='\r')) Param->Valeur[i++] = *(p++); 
				Param->Valeur[i]='\0';
			}
		}
	}

	//Fermer le fichier
	fclose(hFic);
	return TRUE;
}

/****************************************************************************************/
/* Retrouver une valeur lu dans le fichier INI											*/
int Fcnf_Valeur(PARAM *LstParam, char *Cle, char *Valeur)
{
	int		bFin;
	PARAM	*Param;	


	if(LstParam==NULL) return FALSE;

	//Initialisation
	bFin = 0;
	Param = LstParam;

	while(bFin==0)
	{
		if(STRICMP(Cle, Param->Cle)==0)
		{
			bFin = 1;
			strcpy(Valeur, Param->Valeur);
		}
		
		if(bFin==0)
		{
			Param = Param->Suivant;
			if(Param==NULL) return FALSE;
		}
	}

	return TRUE;
}

/****************************************************************************************/
/* Retrouver une section dans le fichier INI											*/
int Fcnf_Section(PARAM *LstParam, int No, char *Section)
{
	int		bFin;
	int		i;
	char	SectionAct[128], *Ptr;
	PARAM	*Param;	


	if(LstParam==NULL) return FALSE;

	//Initialisation
	i = 0;
	bFin = 0;
	Param = LstParam;
	Section[0] = '\0';

	while(bFin==0)
	{
		strcpy(SectionAct, Param->Cle);
		Ptr = strchr(SectionAct, '_');
		if(Ptr!=NULL) *Ptr = '\0';
		if(STRICMP(Section, SectionAct)!=0)
		{
			i++;
			strcpy(Section, SectionAct);
		}

		if(i==No) bFin = 1;
		
		if(bFin==0)
		{
			Param = Param->Suivant;
			if(Param==NULL) return FALSE;
		}
	}

	return TRUE;
}

/****************************************************************************************/
/* Définir le fichier de log																	*/
void Flog_Init(char *Fichier)
{
	strcpy(g_FichierLog, Fichier);
}

/****************************************************************************************/
/* Ecrire dans le log																	*/
int Flog_Ecrire(char *Format, ...)
{
	FILE	*hFic;
	time_t temps;
    struct tm	*tp;
    va_list Args;

	//Controle
	if(g_FichierLog[0]=='\0') return FALSE;
    
	//Ouvrir le fichier
	if((hFic = fopen(g_FichierLog, "a")) == NULL) return FALSE;

	temps = time(NULL);
	tp = localtime(&temps);
	fprintf(hFic, "%02d/%02d/%04d\t%02d:%02d:%02d\t", tp->tm_mday, tp->tm_mon + 1, tp->tm_year+1900, tp->tm_hour, tp->tm_min, tp->tm_sec);

	va_start(Args, Format);
    vfprintf(hFic, Format, Args);
    va_end(Args);

	fputs("\n", hFic);

	//Fermer le fichier
	fclose(hFic);
	return TRUE;
}
