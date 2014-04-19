#include "xpp.h"
#include "xapdef.h"
#include "Fichier.h"

int xpp_init(char *uid, char *interfacename, int interfaceport, char *instance, int debuglevel)
{
	xap_init(uid, interfacename, interfaceport, instance, debuglevel);
	return g_xap_receiver_sockfd;
}

int xpp_heartbeat_tick(int interval)
{
	return xap_heartbeat_tick(interval);
}

int xpp_cmd(char *Capteur, char *State, char *Level, char *Text)
{
	char i_buff[256];

	sprintf(i_buff, "xap-header\n{\nv=12\nhop=1\nuid=%s\nclass=xAPBSC.cmd\nsource=%s.%s.%s\ntarget=%s\n}\noutput.state.1\n{\n", g_uid, XAP_ME, XAP_SOURCE, g_instance, Capteur);
	if(State!=NULL) sprintf(i_buff, "%sState=%s\n", i_buff, State);
	if(Level!=NULL) sprintf(i_buff, "%sLevel=%s\n", i_buff, Level);
	if(Text!=NULL)  sprintf(i_buff, "%sText=%s\n",   i_buff, Text);
	strcat(i_buff, "}\n");
	return xap_send_message(i_buff);

}
int xpp_query(char *Capteur)
{
	char i_buff[256];

	sprintf(i_buff, "xap-header\n{\nv=12\nhop=1\nuid=%s\nclass=xAPBSC.query\nsource=%s.%s.%s\ntarget=%s\n}\nrequest\n{\n}\n", g_uid, XAP_ME, XAP_SOURCE, g_instance, Capteur);
	return xap_send_message(i_buff);
}

int xpp_event(int No, char *Capteur, int State, int Level, double Temp)
{
	char i_OffOn[16];
	char i_buff[1500];
	char i_uid[16];


	strcpy(i_uid, g_uid);
	i_uid[6] = '\0';

	strcpy(i_OffOn, "");
	if(State==0) strcpy(i_OffOn, "\nState=off");
	if(State>0) strcpy(i_OffOn, "\nState=on");

	sprintf(i_buff, "xap-header\n{\nv=12\nhop=1\nuid=%s%02X\nclass=xAPBSC.event\nsource=%s.%s.%s:%s\n}\ninput.state\n{%s\nDisplayText=%f\nText=%3.2f\nLevel=%d\n}\n", i_uid, No, XAP_ME, XAP_SOURCE, g_instance, Capteur, i_OffOn, Temp, Temp, Level);
	xap_send_message(i_buff);

	return TRUE;
}

int xpp_info(int No, char *Capteur, int State, int Level, double Temp, char *Destinataire)
{
	char i_OffOn[16];
	char i_buff[1500];
	char i_uid[16];
	char *ptr;


	strcpy(i_uid, g_uid);
	i_uid[6] = '\0';

	strcpy(i_OffOn, "");
	if(State==0) strcpy(i_OffOn, "\nState=off");
	if(State>0) strcpy(i_OffOn, "\nState=on");

	ptr = i_buff;
	ptr += sprintf(ptr, "xap-header\n{\nv=12\nhop=1\nuid=%s%02X\nclass=xAPBSC.info\nsource=%s.%s.%s:%s\n", i_uid, No, XAP_ME, XAP_SOURCE, g_instance, Capteur);
	if(Destinataire != NULL) ptr += sprintf(ptr, "target=%s\n", Destinataire);
	ptr+= sprintf(ptr, "}\ninput.state\n{%s\nDisplayText=%f\nText=%3.2f\nLevel=%d\n}\n", i_OffOn, Temp, Temp, Level);
	xap_send_message(i_buff);

	return TRUE;
}

int xpp_GetTargetId()
{
	char i_temp[XAP_MAX_KEYVALUE_LEN];

	if(xapmsg_getvalue("output.state.1:id", i_temp)==0) return -1;
	return atoi(i_temp);
}

int xpp_GetTargetName(char *Capteur)
{
	if(xapmsg_getvalue("xap-header:target", Capteur)==0) return -1;
	return 1;
}

int xpp_GetSourceName(char *Source)
{
	if(xapmsg_getvalue("xap-header:source", Source)==0) return -1;
	return 1;
}

int xpp_GetClassName(char *Class)
{
	if(xapmsg_getvalue("xap-header:class", Class)==0) return -1;
	return 1;
}

int xpp_GetTargetState(char *State)
{
	if(xapmsg_getvalue("output.state.1:State", State)!=0) return 1;
	if(xapmsg_getvalue("input.state:State", State)!=0) return 1;
	return -1;
}

int xpp_GetTargetText(char *Text)
{
	if(xapmsg_getvalue("input.state:Text", Text)==0) return -1;
	return 1;
}

int xpp_GetCmd(char *Cle, char *Valeur)
{
	if(xapmsg_getvalue(Cle, Valeur)==0) return -1;
	return 1;
}

int xpp_compare(const char* a_item1, const char* a_item2)
{
	return xap_compare(a_item1, a_item2);
}

int xpp_PollIncoming(int a_server_sockfd, char* a_buffer, int a_buffer_size)
{

	int i;  
    	// Check for incoming message, write to buffer return byte count
	i=recvfrom(a_server_sockfd, a_buffer, a_buffer_size-1, 0, 0, 0);
	
	if (i!=-1) a_buffer[i]='\0'; // terminate the buffer so we can treat it as a conventional string

	return i;		
}

int xpp_MessageType()
{
	switch(xapmsg_gettype())
	{
		case XAP_MSG_HBEAT :			return XPP_MSG_HBEAT;
		case XAP_MSG_ORDINARY :			return XPP_MSG_ORDINARY;
		case XAP_MSG_CONFIG_REQUEST :	return XPP_MSG_CONFIG_REQUEST;
		case XAP_MSG_CACHE_REQUEST :	return XPP_MSG_CACHE_REQUEST;
		case XAP_MSG_CACHE_REPLY :		return XPP_MSG_CACHE_REPLY;
		case XAP_MSG_CONFIG_REPLY :		return XPP_MSG_CONFIG_REPLY;
	}
	return -1;
}

int xpp_DispatchReception(const char* a_buf)
{
	char i_identity[XAP_MAX_KEYVALUE_LEN];
	char i_target[XAP_MAX_KEYVALUE_LEN];
	char i_class[XAP_MAX_KEYVALUE_LEN];
	char *ptr;


	//Message ordinaire ?
	xapmsg_parse(a_buf);
	if(xapmsg_gettype()!=XAP_MSG_ORDINARY)
	{
		if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Réception d'un message sans xap-header");
		return -1;
	}

	//Controler la cible
	if (xapmsg_getvalue("xap-header:target", i_target)!=0)
	{
		ptr = strchr(i_target, ':');
		if(ptr!=NULL) *ptr=0;
		sprintf(i_identity, "%s.%s.%s", XAP_ME, XAP_SOURCE, g_instance);
		if (xap_compare(i_target, i_identity)!=1)
		{
			if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Réception d'un message pas pour moi=%s, destinataire=%s", i_identity, i_target);
			return 0;
		}
	}

	//Controler la classe
	if (xapmsg_getvalue("xap-header:class", i_class)==0)
	{
		if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Réception d'un message sans classe");
		return -1;
	}
	if (g_debuglevel>=DEBUG_DEBUG) Flog_Ecrire("Réception d'un message de classe : %s", i_class);

	if (xap_compare(i_class, "xAPservice.cmd")==1)	return XPP_RECEP_SERVICE_CMD;
	if (xap_compare(i_class, "xAPBSC.cmd")==1)		return XPP_RECEP_CAPTEUR_CMD;
	if (xap_compare(i_class, "xAPBSC.query")==1)	return XPP_RECEP_CAPTEUR_QUERY;
	if (xap_compare(i_class, "xAPBSC.info")==1)		return XPP_RECEP_CAPTEUR_INFO;
	if (xap_compare(i_class, "xAPBSC.event")==1)	return XPP_RECEP_CAPTEUR_EVENT;

	return 0;
}

int xpp_GetHbeat(char *Source, int *Interval)
{
	char cInterval[8];

	if(xapmsg_getvalue("xap-hbeat:source", Source)==0) return -1;
	if(xapmsg_getvalue("xap-hbeat:interval", cInterval)==0) return -1;
	*Interval = atoi(cInterval);
	return 1;
}


/****************************************************************************************************************/
/* Identifier un capteur (format *.*.*:*)																		*/
int xpp_bIdCapteur(char *Capteur)
{
	char *ptr;

	ptr = strchr(Capteur, '.');
	if(ptr==NULL) return FALSE;

	ptr = strchr(ptr+2, '.');
	if(ptr==NULL) return FALSE;

	ptr = strchr(ptr+2, ':');
	if(ptr==NULL) return FALSE;

	return TRUE;
}
