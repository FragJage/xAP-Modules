//TODO 1 : Que faire s'il n'y a plus de place pour le cache mémoire
//TODO 2 : Que faire s'il n'y a plus de place pour le cache fichier

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "Fichier.h"
#include "DBcache.h"

extern PARAM *g_LstParam;

DBCACHE *g_DbCache = NULL;
int Cache_TailleMemMax;
int Cache_TailleFicMax;
int Cache_TailleMemActuelle;
int Cache_TailleFicActuelle;
FILE *Cache_Hdl;

int DBCache_VidageMem();
int DBCache_VidageFic();
int DBCache_Ajoute(char *i_source, char *i_valeur, char i_periode);
int DBCache_AjouteMem(char *i_source, char *i_valeur, char i_periode, time_t i_time);
int DBCache_AjouteFic(char *i_source, char *i_valeur, char i_periode, time_t i_time);


int DBCache_Init()
{
	char temp[32];
	char Fichier[256];

	Cache_TailleMemMax = 0;
	Cache_TailleFicMax = 0;
	Cache_TailleMemActuelle = 0;
	Cache_TailleFicActuelle = 0;
	Fichier[0]='\0';

	if(Fcnf_Valeur(g_LstParam, "DATABASE_CacheTailleMem", temp)!=0) Cache_TailleMemMax = atoi(temp)*1024;
	if(Fcnf_Valeur(g_LstParam, "DATABASE_CacheTailleFic", temp)!=0) Cache_TailleFicMax = atoi(temp)*1024;
	Fcnf_Valeur(g_LstParam, "DATABASE_CacheFichier", Fichier);

	if(Cache_TailleFicMax>0)
	{
		if(Fichier[0]=='\0')
		{
			FichierStd(Fichier, TYPFIC_DATA);
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Localisation du fichier cache %s.", Fichier);
		}

		if((Cache_Hdl = fopen(Fichier, "w")) == NULL)
		{
			if (g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Impossible d'ouvrir le fichier cache (%s) : Cache fichier abandonné.", Fichier);
			Cache_TailleFicMax=0;
		}
	}

	if (g_debuglevel>=DEBUG_VERBOSE) 
	{
		Flog_Ecrire("Taille du cache mémoire %d Ko.", Cache_TailleMemMax/1024);
		Flog_Ecrire("Taille du cache fichier %d Ko.", Cache_TailleFicMax/1024);
		if(Fichier[0]!='\0') Flog_Ecrire("Localisation du fichier cache %s.", Fichier);
	}
}

int DBCache_Stop()
{
	DBCache_VidageMem();

	if(Cache_Hdl!=NULL)
	{
		DBCache_VidageFic();
		fclose(Cache_Hdl);
	}
}

int DBCache_AjouteFic(char *i_source, char *i_valeur, char i_periode, time_t i_time)
{
	char Ligne[512];
	int err;


	sprintf(Ligne, "%s %s %c %lu", i_source, i_valeur, i_periode, i_time);
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Ecriture dans le cache fichier de %s.", Ligne);
	err = fputs(Ligne, Cache_Hdl);
	if ((g_debuglevel>=DEBUG_INFO)&&(err<0)) Flog_Ecrire("Impossible d'écrire dans le fichier de cache (erreur %d).", err);

	//*** Calcul taille du cache
	Cache_TailleFicActuelle += strlen(Ligne)+1;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Taille du cache fichier : %d octets.", Cache_TailleFicActuelle);

	//*** Controle taille du cache
	if(Cache_TailleFicActuelle>Cache_TailleFicMax)
	{
		if(g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Vidage du cache fichier.");
		DBCache_VidageFic();
	}
}

int DBCache_MemToXXX(int i_mode)
{
	DBCACHE *DbCache;

	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Vidage du cache mémoire.");
	while(g_DbCache != NULL)
	{
		DbCache = g_DbCache;
		g_DbCache = DbCache->Suivant;

		switch(i_mode)
		{
			case CACHE_DST_FICHIER :
				DBCache_AjouteFic(DbCache->Source, DbCache->Valeur, DbCache->Periode, DbCache->Time);
				break;
			case CACHE_DST_SQL :
				DBmysql_EnregistrerValeurPeriode(DbCache->Source, DbCache->Valeur, DbCache->Periode, DbCache->Time);
				break;
		}

		free(DbCache->Source);
		free(DbCache->Valeur);
		free(DbCache);
	}
	Cache_TailleMemActuelle = 0;
}

int DBCache_VidageMem()
{
	if(Cache_TailleFicMax>0) 
		return DBCache_MemToXXX(CACHE_DST_FICHIER);
	else
		return DBCache_MemToXXX(CACHE_DST_SQL);
	
}

int DBCache_VidageFic()
{
	int fd;
	char Ligne[512];
	DBCACHE DbCache;


	if(Cache_TailleFicMax==0) return;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Vidage du cache fichier.");
	rewind(Cache_Hdl);
	while( (!feof(Cache_Hdl)) && (fgets(Ligne, sizeof(Ligne), Cache_Hdl)) )
	{
		sscanf(Ligne, "%s %s %c %lu", DbCache.Source, DbCache.Valeur, DbCache.Periode, DbCache.Time);
		DBmysql_EnregistrerValeurPeriode(DbCache.Source, DbCache.Valeur, DbCache.Periode, DbCache.Time);
	}

	fd = fileno(Cache_Hdl);
	ftruncate(fd, 0);		//_chsize sous windows via io.h
	Cache_TailleFicActuelle = 0;
}

int DBCache_AjouteMem(char *i_source, char *i_valeur, char i_periode, time_t i_time)
{
	DBCACHE *DbCache;
	BOOL bTrouve;

	//*** Cas période, vérifier si nous ne l'avons pas déjà
	if(i_periode>'\0')
	{
		bTrouve = FALSE;
		DbCache = g_DbCache;
		while((DbCache!=NULL)&&(bTrouve==FALSE))
		{
			if((i_periode==DbCache->Periode)&&(strcmp(i_source, DbCache->Source)==0))
			{
				free(DbCache->Valeur);
				DbCache->Valeur = strdup(i_valeur);
				DbCache->Time   = i_time;
				bTrouve = TRUE;
				if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Mise à jour du cache fichier pour %s.", DbCache->Source);
			}
			DbCache = DbCache->Suivant;
		}
		if(bTrouve == TRUE) return TRUE;
	}

	//*** Allocation mémoire
	DbCache = malloc(sizeof(DBCACHE));
	if(DbCache==NULL)
	{
		if(g_debuglevel>=DEBUG_INFO) Flog_Ecrire("Plus assez de mémoire pour mettre le valeur en cache.");
		return FALSE;
	}

	//*** Affectation de la structure
	DbCache->Source = strdup(i_source);
	DbCache->Valeur = strdup(i_valeur);
	DbCache->Periode= i_periode;
	DbCache->Time   = i_time;
	DbCache->Suivant= g_DbCache;
	g_DbCache = DbCache;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Ecriture dans le cache mémoire de %s=%s.", DbCache->Source, DbCache->Valeur);

	//*** Calcul taille du cache
	Cache_TailleMemActuelle += sizeof(DBCACHE);
	Cache_TailleMemActuelle += strlen(DbCache->Source)+1;
	Cache_TailleMemActuelle += strlen(DbCache->Valeur)+1;
	if (g_debuglevel>=DEBUG_VERBOSE) Flog_Ecrire("Taille du cache mémoire : %d octets.", Cache_TailleMemActuelle);

	//*** Controle taille du cache
	if(Cache_TailleMemActuelle>Cache_TailleMemMax)
		DBCache_VidageMem();

	return TRUE;
}

int DBCache_Ajoute(char *i_source, char *i_valeur, char i_periode)
{
	time_t	i_time;

	i_time = time((time_t*)0);

	if(Cache_TailleMemMax>0) return DBCache_AjouteMem(i_source, i_valeur, i_periode, i_time);
	if(Cache_TailleFicMax>0) return DBCache_AjouteFic(i_source, i_valeur, i_periode, i_time);
	return DBmysql_EnregistrerValeurPeriode(i_source, i_valeur, i_periode, i_time);
}
