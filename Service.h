//
//  MODULE: service.h
//

#ifndef _SERVICE_H
#define _SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
char g_ServiceChemin[256];
char g_ServiceNom[256];

//////////////////////////////////////////////////////////////////////////////
int ServiceStart();
void ServicePause(int bPause);
void ServiceStop();

#ifdef __cplusplus
}
#endif

#endif
