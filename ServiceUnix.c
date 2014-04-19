//
//  MODULE:   ServiceUnix.c
//
//  PURPOSE:  Implements functions required by all daemon Unix.
//
//  FUNCTIONs:
//    main(int argc, char **argv);
//    daemon_start()
//    daemon_stop()
//    daemon_pause()

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "Service.h"

#ifndef SOMAXCONN
	#define SOMAXCONN	32
#endif

#ifndef __linux__
    //extern		char *sys_siglist[];	adaptation pour DLINK DNS320
#else
    #undef  OPEN_MAX
    #define OPEN_MAX	512
#endif

#ifndef OPEN_MAX
    #define OPEN_MAX	FOPEN_MAX
#endif

#ifndef FALSE
	#define FALSE	0
	#define TRUE	1
#endif
typedef int    BOOL;

typedef struct sigaction    sigact_t;

// internal variables
static	int	SignalArret = 0;
static	int	bFinDemon   = FALSE;

// internal function prototypes
static	void	Arrete();


//
//  FUNCTION: main
//
//  PURPOSE: entrypoint for service
//
//  PARAMETERS:
//    argc - number of command line arguments
//    argv - array of command line arguments
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//    This routine performs the service initialization and then calls
//    the user defined ServiceStart() routine to perform majority
//    of the work.
//
/*
 *  Main - Traite les arguments et transforme notre process en demon
 */
int main(int argc, char *argv[])
{
    sigact_t	ActionSig;
    sigset_t	MasqueSig;
	int			dwErr = 0;
	char		*p;
	int			pidFilehandle;
	char		Str[64];
	int			i;
	char		*ConfigFile=NULL;
	char		*LogFile=NULL;
	char		*PidFile=NULL;


	/*
     *	Reconstruction du nom du service
     */
	strcpy(g_ServiceChemin, *argv);
	p = strrchr(g_ServiceChemin, '/');
	if(p != NULL)
	{
		p++;
		strcpy(g_ServiceNom, p);
		*p = '\0';
	}
	else
	{
		strcpy(g_ServiceNom, g_ServiceChemin);
		g_ServiceChemin[0]='\0';
	}

	p = strrchr(g_ServiceNom, '.');
	if(p != NULL) *p = '\0';

	/*
     *	Lecture des paramètres
     */
	for (i=1; i < argc; i++)
	{
		if(strncmp(argv[i], "--config-file=", 14)==0)	ConfigFile = argv[i]+14;
		if(strncmp(argv[i], "--log-file=", 11)==0)		LogFile = argv[i]+11;
		if(strncmp(argv[i], "--pid-file=", 11)==0)		PidFile = argv[i]+11;
	}

	/*
     *	Créer le fichier PID
     */
	if(LogFile==NULL)
	{
		sprintf(Str, "/var/run/%s.pid",g_ServiceNom);
		LogFile = Str;
	}
	pidFilehandle = open(LogFile, O_RDWR|O_CREAT, 0600);
    if(pidFilehandle != -1)
    {
        sprintf(Str,"%d\n",getpid());
		write(pidFilehandle, Str, strlen(Str));
        close(pidFilehandle);
	}

	/*
     *	Ignorer les signaux
     */
    ActionSig.sa_handler = SIG_IGN;
    ActionSig.sa_flags = 0;
    (void)sigemptyset(&ActionSig.sa_mask);

    if (   sigaction(SIGHUP,  &ActionSig, NULL) != 0
	|| sigaction(SIGINT,  &ActionSig, NULL) != 0
	|| sigaction(SIGQUIT, &ActionSig, NULL) != 0
	|| sigaction(SIGPIPE, &ActionSig, NULL) != 0
	|| sigaction(SIGCHLD, &ActionSig, NULL) != 0
	|| sigaction(SIGTSTP, &ActionSig, NULL) != 0
	|| sigaction(SIGTTIN, &ActionSig, NULL) != 0
	|| sigaction(SIGTTOU, &ActionSig, NULL) != 0)
	{
		dwErr = errno;
		goto Cleanup;
	}

    /*
     *	Gestion du signal d'arret
     */
    (void)sigemptyset(&MasqueSig);
    (void)sigaddset(&MasqueSig, SIGTERM);

    if (pthread_sigmask(SIG_BLOCK, &MasqueSig, NULL) != 0)
	{
		dwErr = errno;
		goto Cleanup;
	}

    ActionSig.sa_handler = Arrete;
    if ( sigaction(SIGTERM, &ActionSig, NULL) != 0)
	{
		dwErr = errno;
		goto Cleanup;
	}

    if (pthread_sigmask(SIG_UNBLOCK, &MasqueSig, NULL) != 0)
	{
		dwErr = errno;
		goto Cleanup;
	}

    /*
     * --- Boucle principale du thread
     */
	dwErr = ServiceStart(ConfigFile, LogFile);

    /*
     *	Sortie du service, normale ou en erreur
     */
    if (SignalArret != SIGTERM)
		printf("Arret du service par reception du signal %d (%s)", SignalArret, strsignal(SignalArret)); //adaptation pour DLINK DNS320
		//printf("Arret du service par reception du signal %d (%s)", SignalArret, sys_siglist[SignalArret]);

Cleanup:


    /*
     *	On suppose que les ressources thread sont
     *	automatiquement liberees a la fin du process.
     */
    if (dwErr != 0)
		printf("Le service s'est arrêté suite à une erreur : %d", dwErr);
    else
		printf("Le service a été arrêté correctement");

    return 0;
}

/*
 *  Arrete - Interception des signaux d'arret par le thread principal
 */
void Arrete(int Signal)
{
	switch (Signal)
	{
		case SIGTERM :
			if (!bFinDemon)				/* Si l'on n'a pas déjà une demande de fin du service	    */
				SignalArret = SIGTERM;	/* c'est qu'il ne s'agit pas d'un kill dù à un autre signal */
			bFinDemon = TRUE;
			ServiceStop();
			break;

		default :
			SignalArret = Signal;		/* Conserver le n° du Signal initial */
			bFinDemon = TRUE;			/* Forcer la fin du Service	     */
			kill(getpid(), SIGTERM);
			break;
	}

	return;
}

/*
 *  OsJournalise - Ecrit dans le journal de l'OS
 */
void OsJournalise(int Code, char *Message)
{
#ifdef _AIX
    static  struct syslog_data	LogData = SYSLOG_DATA_INIT;
#endif
    static  BOOL    PremierAppel = TRUE;

    int	    Level = LOG_ERR;	/* Par défaut	*/
    int	    Facility = LOG_LOCAL1;
    char    *Type;
    char    Info[32];

    if (PremierAppel)
    {
#ifdef _AIX
	(void)openlog_r("", LOG_CONS | LOG_PID, Facility, &LogData);
#else
	(void)openlog("", LOG_CONS | LOG_PID, Facility);
#endif
	PremierAppel = FALSE;
    }

    switch (Code)
    {
	case LOG_INFO:	Level = LOG_INFO;	Type = "Info.";	break;
	case LOG_WARNING:	Level = LOG_WARNING;	Type = "Warn.";	break;
	case LOG_ERR:	Level = LOG_ERR;	Type = "ERROR";	break;
    }
	sprintf(Info, "(%s) ", Type);

#ifdef _AIX
    (void)syslog_r(Level | Facility, &LogData, "%s%s", Info, Message);
#else
    (void)syslog(Level | Facility, "%s%s", Info, Message);
#endif

    return;
}
