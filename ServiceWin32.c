//
//  MODULE:   ServiceWin32.c
//
//  PURPOSE:  Implements functions required by all services windows.
//
//  FUNCTIONS:
//    main(int argc, char **argv);
//    service_ctrl(DWORD dwCtrlCode);
//    service_main(DWORD dwArgc, LPTSTR *lpszArgv);
//    CmdInstallService();
//    CmdRemoveService();
//    CmdDebugService(int argc, char **argv);
//    ControlHandler ( DWORD dwCtrlType );
//    GetLastErrorText( LPTSTR lpszBuf, DWORD dwSize );
//

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <tchar.h>

#include "Service.h"

#define EVMSG_FONCTION_FAILED	100
#define EVMSG_DEMARRE			101
#define EVMSG_ARRETE			102
#define EVMSG_ERREUR_ARRET		103
#define EVMSG_INFO				200
#define EVMSG_WARNING			201
#define EVMSG_ERROR				202

#define SEVERITY_CODE(MSG_ID)	(MSG_ID & 0xC0000000)

#define MSG_SUCCESS			0x00000000
#define MSG_INFORMATIONAL	0x40000000
#define MSG_WARNING			0x80000000
#define MSG_ERROR			0xC0000000

#define CATEGORY_CODE(MSG_ID)	(WORD)((MSG_ID & 0x0E000000) >> 25)


// internal variables
SERVICE_STATUS          ssStatus;       // current status of the service
SERVICE_STATUS_HANDLE   sshStatusHandle;
BOOL                    bDebug = FALSE;
TCHAR                   szErr[256];

// internal function prototypes
VOID WINAPI service_main(DWORD dwArgc, LPTSTR *lpszArgv);
VOID WINAPI service_ctrl(DWORD dwCtrlCode);

VOID CmdInstallService(char *User, char *PassWord);
VOID CmdRemoveService();
VOID CmdDebugService(int argc, char **argv);

BOOL WINAPI ControlHandler(DWORD dwCtrlType);
LPTSTR GetLastErrorText(LPTSTR lpszBuf, DWORD dwSize);

BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
VOID AddToMessageLog(DWORD IDEvent, LPTSTR Param);


void InitName()
{
	char *p;

	GetModuleFileName(NULL, g_ServiceChemin, 256);
	p = strrchr(g_ServiceChemin, '\\');
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
}

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
//    main() either performs the command line task, or
//    call StartServiceCtrlDispatcher to register the
//    main service thread.  When this call returns,
//    the service has stopped, so exit.
//
void main(int argc, char *argv[])
{
	SERVICE_TABLE_ENTRY dispatchTable[2];

	InitName();

	if (argc > 1 && (*argv[1] == '-' || *argv[1] == '/'))
	{
		if (_stricmp("install", argv[1]+1) == 0 && argc > 3)
		{
			CmdInstallService(argv[2], argv[3]);
		}
		else if (_stricmp("remove", argv[1]+1) == 0)
		{
			CmdRemoveService();
		}
		else if (_stricmp("debug", argv[1]+1) == 0)
		{
			bDebug = TRUE;
			CmdDebugService(argc, argv);
		}
		else
			goto dispatch;

		exit(0);
    }

    // if it doesn't match any of the above parameters
    // the service control manager may be starting the service
    // so we must call StartServiceCtrlDispatcher
dispatch:
    printf( "%s -install Domain\\User Password  to install the service\n", g_ServiceNom);
    printf( "%s -remove		    to remove the service\n", g_ServiceNom);
    printf( "%s -debug <params>	    to run as a console app for debugging\n", g_ServiceNom);
    printf( "\nStartServiceCtrlDispatcher being called.\n" );
    printf( "This may take several seconds.  Please wait.\n" );

    dispatchTable[0].lpServiceName = TEXT(g_ServiceNom);
    dispatchTable[0].lpServiceProc = service_main;
    dispatchTable[1].lpServiceName = NULL;
    dispatchTable[1].lpServiceProc = NULL;

    if (StartServiceCtrlDispatcher(dispatchTable) == FALSE) 
    	AddToMessageLog(EVMSG_FONCTION_FAILED, TEXT("StartServiceCtrlDispatcher"));
}


//
//  FUNCTION: service_main
//
//  PURPOSE: To perform actual initialization of the service
//
//  PARAMETERS:
//    dwArgc   - number of command line arguments
//    lpszArgv - array of command line arguments
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//    This routine performs the service initialization and then calls
//    the user defined ServiceStart() routine to perform majority
//    of the work.
//
void WINAPI service_main(DWORD dwArgc, LPTSTR *lpszArgv)
{
	int dwErr = 0;

	InitName();
	// register our service control handler:
	//
	sshStatusHandle = RegisterServiceCtrlHandler(TEXT(g_ServiceNom), service_ctrl);
	if (sshStatusHandle == 0) goto cleanup;

	// SERVICE_STATUS members that don't change
	//
	ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ssStatus.dwServiceSpecificExitCode = 0;

	// report the status to the service control manager.
	// arguments:   service state, exit code, wait hint
	//
	if (ReportStatusToSCMgr(SERVICE_START_PENDING, NO_ERROR, 3000) == FALSE) goto cleanup;
	if (ReportStatusToSCMgr(SERVICE_RUNNING, NO_ERROR, 0) == FALSE) goto cleanup;
	dwErr = ServiceStart(NULL, NULL);

cleanup:

	// try to report the stopped status to the service control manager.
	//
	if (sshStatusHandle != 0) (VOID)ReportStatusToSCMgr(SERVICE_STOPPED, (DWORD)dwErr, 0);

	return;
}


//
//  FUNCTION: service_ctrl
//
//  PURPOSE: This function is called by the SCM whenever
//           ControlService() is called on this service.
//
//  PARAMETERS:
//    dwCtrlCode - type of control requested
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//
VOID WINAPI service_ctrl(DWORD dwCtrlCode)
{
	// Handle the requested control code.
	//
	switch(dwCtrlCode)
	{
		case SERVICE_CONTROL_PAUSE: 
			ServicePause(TRUE);
			break; 
 
		case SERVICE_CONTROL_CONTINUE: 
			ServicePause(FALSE);
			break; 
 
		case SERVICE_CONTROL_STOP:
			ssStatus.dwCurrentState = SERVICE_STOP_PENDING;
			ServiceStop();
			break;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;

    }
    ReportStatusToSCMgr(ssStatus.dwCurrentState, NO_ERROR, 0);
}


//
//  FUNCTION: ReportStatusToSCMgr()
//
//  PURPOSE: Sets the current status of the service and
//           reports it to the Service Control Manager
//
//  PARAMETERS:
//    dwCurrentState - the state of the service
//    dwWin32ExitCode - error code to report
//    dwWaitHint - worst case estimate to next checkpoint
//
//  RETURN VALUE:
//    TRUE  - success
//    FALSE - failure
//
//  COMMENTS:
//
BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;
	BOOL fResult = TRUE;

	if (bDebug == TRUE) return TRUE;

	if (dwCurrentState == SERVICE_START_PENDING)
		ssStatus.dwControlsAccepted = 0;
	else
		ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;

	ssStatus.dwCurrentState = dwCurrentState;
	ssStatus.dwWin32ExitCode = dwWin32ExitCode;
	ssStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED || dwCurrentState == SERVICE_PAUSED)
		ssStatus.dwCheckPoint = 0;
	else
		ssStatus.dwCheckPoint = dwCheckPoint++;


	// Report the status of the service to the service control manager.
	//
	if ((fResult = SetServiceStatus(sshStatusHandle, &ssStatus)) == FALSE)
	AddToMessageLog(EVMSG_FONCTION_FAILED, TEXT("SetServiceStatus"));

	return fResult;
}


//
//  FUNCTION: AddToMessageLog(LPTSTR lpszMsg)
//
//  PURPOSE: Allows any thread to log an error message
//
//  PARAMETERS:
//    lpszMsg - text for message
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//
VOID AddToMessageLog(DWORD IDEvent, LPTSTR Param)
{
    TCHAR   szMsg[256];
    HANDLE  hEventSource;
    LPTSTR  lpszStrings[2];
    WORD    TypeEvent;
    WORD    Category;
    WORD    NumStrings;

    if (bDebug == TRUE) return;

	// Use event logging to log the error.
	//
	hEventSource = RegisterEventSource(NULL, TEXT(g_ServiceNom));
	if (hEventSource != NULL) return;

	Category = 0;
	switch (IDEvent)
	{
		case EVMSG_FONCTION_FAILED :
			lpszStrings[0] = Param;
			lpszStrings[1] = GetLastErrorText(szMsg, sizeof(szMsg));
			NumStrings = 2;
			break;

		case EVMSG_INFO :
		case EVMSG_WARNING :
		case EVMSG_ERROR :
		/* FALLTHROUGH */

		default :
			lpszStrings[0] = (Param != NULL) ? Param : TEXT(g_ServiceNom);
			lpszStrings[1] = NULL;
			NumStrings = 1;
	}

	switch (SEVERITY_CODE(IDEvent))
	{
	    case MSG_SUCCESS		: /* FALLTHROUGH */
	    case MSG_INFORMATIONAL	: TypeEvent = EVENTLOG_INFORMATION_TYPE; break;
	    case MSG_WARNING		: TypeEvent = EVENTLOG_WARNING_TYPE;	    break;
	    case MSG_ERROR			: /* FALLTHROUGH */
	    default					: TypeEvent = EVENTLOG_ERROR_TYPE;	    break;
	}

	ReportEvent(hEventSource, TypeEvent, Category, IDEvent,	NULL, NumStrings, 0, lpszStrings, NULL);
	(VOID)DeregisterEventSource(hEventSource);
}


///////////////////////////////////////////////////////////////////
//
//  The following code handles service installation and removal
//
//
//  FUNCTION: CmdInstallService(char *User, char *PassWord)
//
//  PURPOSE: Installs the service
//
//  PARAMETERS:
//    none
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//
void CmdInstallService(char *User, char *PassWord)
{
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    HKEY	hk; 
    DWORD	dwData; 
    TCHAR	szPath[512];
    TCHAR	szBuf[80]; 

    /*
     *	Declare service to Service Control Manager
     */
    if (GetModuleFileName(NULL, szPath, 512) == 0)
    {
        _tprintf(TEXT("Unable to install - %s\n"), GetLastErrorText(szErr, 256));
        return;
    }
    
    //	Parameters: machine (NULL == local)
    //		    database (NULL == default)
    //		    access required
    if ((schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL)
    {

        schService = CreateService(
            schSCManager,               // SCManager database
            g_ServiceNom,		        // name of service
            g_ServiceNom,				// name to display
            SERVICE_ALL_ACCESS,         // desired access
            SERVICE_WIN32_OWN_PROCESS,  // service type
            SERVICE_AUTO_START,			// start type
            SERVICE_ERROR_NORMAL,       // error control type
            szPath,                     // service's binary
            NULL,                       // no load ordering group
            NULL,                       // no tag identifier
            "",							// dependencies
            User,						// Administrator account
            PassWord);					// password

        if (schService != NULL)
        {
            _tprintf(TEXT("%s installed.\n"), g_ServiceNom );
            CloseServiceHandle(schService);
        }
        else
        {
            _tprintf(TEXT("CreateService %s failed - %s\n"), g_ServiceNom, GetLastErrorText(szErr, 256));
        }
        CloseServiceHandle(schSCManager);
    }
    else
        _tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr,256));

    /*
     *	Declare Event Log in Registry
     */
    _stprintf(szBuf, TEXT("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s"), g_ServiceNom);

    if (RegCreateKey(HKEY_LOCAL_MACHINE, szBuf, &hk))
    {
	_tprintf(TEXT("could not create registry key\n"));
	goto cleanup;
    }

    /* Add the Event ID message-file name to the subkey. */ 
    if (RegSetValueEx(hk,			/* subkey handle	    */ 
		      "EventMessageFile",	/* value name		    */ 
		      0,			/* must be zero		    */ 
		      REG_EXPAND_SZ,		/* value type		    */ 
		      (LPBYTE)szPath,		/* address of value data    */ 
		      (DWORD)strlen(szPath) + 1))	/* length of value data	    */ 
    {
		_tprintf(TEXT("could not set event message file\n"));
		goto cleanup;
    }

    /* Set the supported types flags. */ 
    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE; 

    if (RegSetValueEx(hk,			/* subkey handle	    */ 
		      "TypesSupported",		/* value name		    */ 
		      0,			/* must be zero		    */ 
		      REG_DWORD,		/* value type		    */ 
		      (LPBYTE) &dwData,		/* address of value data    */ 
		      sizeof(DWORD)))		/* length of value data	    */ 
    {
		_tprintf(TEXT("could not set supported types\n")); 
		goto cleanup;
    }

cleanup :
    RegCloseKey(hk); 

}


//
//  FUNCTION: CmdRemoveService()
//
//  PURPOSE: Stops and removes the service
//
//  PARAMETERS:
//    none
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//
void CmdRemoveService()
{
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    TCHAR	szBuf[80]; 
    HKEY	hk; 


    //	Parameters: machine (NULL == local)
    //		    database (NULL == default)
    //		    access required
    if ((schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL)
    {
        schService = OpenService(schSCManager, g_ServiceNom, SERVICE_ALL_ACCESS);

        if (schService)
        {
            // try to stop the service
            if (ControlService( schService, SERVICE_CONTROL_STOP, &ssStatus))
            {
                _tprintf(TEXT("Stopping %s."), g_ServiceNom);
                Sleep(1000);

                while (QueryServiceStatus(schService, &ssStatus))
                {
                    if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
                    {
                        _tprintf(TEXT("."));
                        Sleep( 1000 );
                    }
                    else
                        break;
                }

                if (ssStatus.dwCurrentState == SERVICE_STOPPED)
                    _tprintf(TEXT("\n%s stopped.\n"), g_ServiceNom);
                else
                    _tprintf(TEXT("\n%s failed to stop.\n"), g_ServiceNom);

            }

            // now remove the service
            if (DeleteService(schService))
                _tprintf(TEXT("%s removed.\n"), g_ServiceNom);
            else
                _tprintf(TEXT("DeleteService failed - %s\n"), GetLastErrorText(szErr,256));

            CloseServiceHandle(schService);
        }
        else
            _tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(szErr,256));

        CloseServiceHandle(schSCManager);
    }
    else
        _tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr,256));

    /* 
     * Open your source name as a subkey under the Application 
     * key in the EventLog service portion of the registry. 
     */ 
    _stprintf(szBuf,
	      TEXT("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s"),
	      TEXT(g_ServiceNom));

    if (RegOpenKey(HKEY_LOCAL_MACHINE, szBuf, &hk))
    {
		_tprintf(TEXT("could not open registry key\n"));
		return;
    }

    /* Sub the Event ID message-file name to the subkey. */ 
    if (RegDeleteValue(hk, "EventMessageFile"))
    {
		_tprintf(TEXT("could not sub event message file\n"));
    }

    /* Sub the supported types flags. */ 
    if (RegDeleteValue(hk, "TypesSupported"))
    {
		_tprintf(TEXT("could not sub supported types\n")); 
    }

    RegCloseKey(hk);

    if (RegDeleteKey(HKEY_LOCAL_MACHINE, szBuf))
    {
		_tprintf(TEXT("could not delete registry key\n"));
    }

}


///////////////////////////////////////////////////////////////////
//
//  The following code is for running the service as a console app
//
//
//  FUNCTION: CmdDebugService(int argc, char ** argv)
//
//  PURPOSE: Runs the service as a console application
//
//  PARAMETERS:
//    argc - number of command line arguments
//    argv - array of command line arguments
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//
void CmdDebugService(int argc, char ** argv)
{
    DWORD dwArgc;
    LPTSTR *lpszArgv;

#ifdef UNICODE
    lpszArgv = CommandLineToArgvW(GetCommandLineW(), &(dwArgc));
#else
    dwArgc   = (DWORD)argc;
    lpszArgv = argv;
#endif

    _tprintf(TEXT("Debugging %s.\n"), TEXT(g_ServiceNom));

    SetConsoleCtrlHandler(ControlHandler, TRUE);

    ServiceStart(dwArgc, lpszArgv);
}


//
//  FUNCTION: ControlHandler ( DWORD dwCtrlType )
//
//  PURPOSE: Handled console control events
//
//  PARAMETERS:
//    dwCtrlType - type of control event
//
//  RETURN VALUE:
//    True - handled
//    False - unhandled
//
//  COMMENTS:
//
BOOL WINAPI ControlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
        case CTRL_BREAK_EVENT:  // use Ctrl+C or Ctrl+Break to simulate
        case CTRL_C_EVENT:      // SERVICE_CONTROL_STOP in debug mode
            _tprintf(TEXT("Stopping %s.\n"), TEXT(g_ServiceNom));
            ServiceStop();
            return TRUE;
            break;
    }
    return FALSE;
}


//
//  FUNCTION: GetLastErrorText
//
//  PURPOSE: copies error message text to string
//
//  PARAMETERS:
//    lpszBuf - destination buffer
//    dwSize - size of buffer
//
//  RETURN VALUE:
//    destination buffer
//
//  COMMENTS:
//
LPTSTR GetLastErrorText(LPTSTR lpszBuf, DWORD dwSize)
{
    DWORD dwRet;
    LPTSTR lpszTemp = NULL;

    dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                          NULL,
                          GetLastError(),
                          LANG_NEUTRAL,
                          (LPTSTR)&lpszTemp,
                          0,
                          NULL );

    // supplied buffer is not long enough
    if (dwRet == 0 || (long)dwSize < (long)dwRet+14)
        lpszBuf[0] = TEXT('\0');
    else
    {
        lpszTemp[lstrlen(lpszTemp)-2] = TEXT('\0');  //remove cr and newline character
        _stprintf(lpszBuf, TEXT("%s (0x%x)"), lpszTemp, GetLastError());
    }

    if (lpszTemp != NULL)
        LocalFree((HLOCAL)lpszTemp);

    return lpszBuf;
}
