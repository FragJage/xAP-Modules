//TODO 1 : Créer la database si elle n'existe pas
//TODO 2 : Controler le retour de la requête create table

/* pour integrer l'API WinSock on a le choix entre #include<winsock> et #define __LCC__ 
dans le fichier mysql.h lignes 34(ça depend de la version) nous avons:

#ifdef __LCC__
#include <winsock.h> // For windows 
#endif

alors si on met #define __LCC__ cela engendre l'inclusion de winsock.h
*/

#ifdef WIN32
	#define __LCC__
	#include <conio.h>
#endif
#include <stdio.h>
#include <string.h>
#include <mysql.h>
#include <time.h>
#include "Fichier.h"

extern PARAM *g_LstParam;

MYSQL *g_MysqlCnx;


int DBmysql_Periode(char i_periode)
{
	switch(i_periode)
	{
		case 'A' : return 4;
		case 'M' : return 7;
		case 'J' : return 10;
		case 'h' : return 13;
		case 'm' : return 16;
	}
	return 19;
}

void DBmysql_ToDate(char *i_date, int i_tailleMax, time_t i_time)
{
	strftime(i_date, i_tailleMax, "%Y-%m-%d %H:%M:%S", localtime(&i_time));
}

int DBmysql_EnregistrerValeur(char *i_source, char *i_valeur, time_t i_time)
{
	char sql[256];
	char date[32];


	DBmysql_ToDate(date, sizeof(date), i_time);
	sprintf(sql, "INSERT INTO capteurs (id, dateheure, valeur) VALUES ('%s', '%s', '%s')", i_source, date, i_valeur);
	if(mysql_query(g_MysqlCnx, sql)!=0)
	{
		Flog_Ecrire("Echec de la requête %s : \n%s\n",sql,mysql_error(g_MysqlCnx));
		return -1;
	}
	return 0;
}

int DBmysql_EnregistrerValeurPeriode(char *i_source, char *i_valeur, char i_periode, time_t i_time)
{
	char sql[256];
	char date[32];
	int cut;
	char cutdate[32];


	if(i_periode=='\0') return DBmysql_EnregistrerValeur(i_source, i_valeur, i_time);

	DBmysql_ToDate(date, sizeof(date), i_time);
	cut = DBmysql_Periode(i_periode);
	strncpy(cutdate, date, cut);
	cutdate[cut] = '\0';

	sprintf(sql, "UPDATE capteurs SET dateheure = '%s', valeur = '%s' WHERE Id = '%s' and left(dateheure,%d)='%s'", date, i_valeur, i_source, cut, cutdate);
	if(mysql_query(g_MysqlCnx, sql)!=0)
	{
		Flog_Ecrire("Echec de la requête %s : \n%s\n",sql,mysql_error(g_MysqlCnx));
		return -1;
	}
	if(mysql_affected_rows(g_MysqlCnx)==0) return DBmysql_EnregistrerValeur(i_source, i_valeur, i_time);

	return 0;
}

int DBmysql_LireValeurPeriode(char *i_source, char *i_valeur, char i_periode)
{
	char sql[256];
	char date[32];
	int periode;
	MYSQL_RES *myRES;
	MYSQL_ROW myROW;
	unsigned long *myLEN;

	*i_valeur = '\0';
	
	periode = DBmysql_Periode(i_periode);
	DBmysql_ToDate(date, sizeof(date), time(NULL));
	date[periode] = '\0';
	
	sprintf(sql, "SELECT valeur FROM capteurs WHERE Id = '%s' and left(dateheure,%d)='%s'", i_source, periode, date);
	if(mysql_query(g_MysqlCnx, sql)!=0)
	{
		Flog_Ecrire("Echec de l'exécution de la requête %s : \n%s\n",sql,mysql_error(g_MysqlCnx));
		return -1;
	}

	if((myRES = mysql_store_result(g_MysqlCnx))==NULL)
	{
		Flog_Ecrire("Impossible de stocker la requête %s : \n%s\n",sql,mysql_error(g_MysqlCnx));
		return -1;
	}

	if((myROW = mysql_fetch_row(myRES))==NULL)
	{
		return 0;
	}

	if((myLEN = mysql_fetch_lengths(myRES))==NULL)
	{
		Flog_Ecrire("Impossible d'obtenir la longueur des champs de la requête %s : \n%s\n",sql,mysql_error(g_MysqlCnx));
		return -1;
	}

	strncpy(i_valeur, myROW[0], myLEN[0]);
	i_valeur[myLEN[0]] = '\0';

	mysql_free_result(myRES);

	return 0;
}

int DBmysql_SupprimerValeur(char *i_source, char *i_date)
{
	char sql[256];

	sprintf(sql, "DELETE FROM capteurs WHERE id='%s'", i_source);
	if(i_date != NULL) sprintf(sql, "%s AND dateheure='%s'", sql, i_date);
		
	if(mysql_query(g_MysqlCnx, sql)!=0)
	{
		Flog_Ecrire("Echec de la requête %s : \n%s\n",sql,mysql_error(g_MysqlCnx));
		return -1;
	}
	return 0;
}

int DBmysql_Connexion()
{
	char hostname[64];
	char user[64];
	char password[64];
	char database[64];
	unsigned int port;

	extern int INI_Valeur(char *Cle, char *Valeur);

	//Initialisation des parametres de connexion
	if(Fcnf_Valeur(g_LstParam, "MYSQL_port", hostname)!=0)
		port=3306;
	else
		port = atoi(hostname);
	if(Fcnf_Valeur(g_LstParam, "MYSQL_hostname", hostname)==0) strcpy(hostname, "localhost");
	if(Fcnf_Valeur(g_LstParam, "MYSQL_user", user)==0) strcpy(user, "root");
	if(Fcnf_Valeur(g_LstParam, "MYSQL_password", password)==0) strcpy(password, "");
	if(Fcnf_Valeur(g_LstParam, "MYSQL_database", database)==0) strcpy(database, "xAP_Database");

	//Initialisation du gestionnaire de la connexion à la base de données mySQL
	g_MysqlCnx=mysql_init(NULL);
	if(!g_MysqlCnx) 
	{
		Flog_Ecrire((char *)mysql_error(g_MysqlCnx));
		return -1;
	}  
       
	//Connexion au serveur mySQL
	if(!mysql_real_connect(g_MysqlCnx,hostname,user,password,NULL,port,NULL,0))
	{
		Flog_Ecrire((char *)mysql_error(g_MysqlCnx));
		return -1;
    }

	//Sélection de la base
	if(mysql_select_db(g_MysqlCnx, database)!=0)
	{
		Flog_Ecrire((char *)mysql_error(g_MysqlCnx));
		return -1;
	}

	//Création de la table au besoin
    mysql_query(g_MysqlCnx,"CREATE TABLE IF NOT EXISTS capteurs (id varchar(256) COLLATE ascii_bin NOT NULL, dateheure datetime NOT NULL, valeur varchar(32) COLLATE ascii_bin NOT NULL, PRIMARY KEY  (id,dateheure))");

	return 0;
}

int DBmysql_Deconnexion()
{
	//Fermeture de la connexion et libèration du pointeur de connexion g_MysqlCnx
	mysql_close(g_MysqlCnx);
	g_MysqlCnx = NULL;
	return 0;
}
