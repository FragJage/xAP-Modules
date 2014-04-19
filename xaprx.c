#include "xapdef.h"
#include "Fichier.h"

#include <string.h>
#include <ctype.h>
/* 
 $Date: 2004/05/30 18:04:14 $
 $Revision: 1.3 $
 $Log: xaprx.c,v $
 Revision 1.3  2004/05/30 18:04:14  cvsowner
 Temporary fix for large messages

 Revision 1.2  2003/02/05 17:54:50  cvsowner
 Addition of xapmsg_updatevalue, xapmsg_toraw, xapmsg_type functions. Split out section name from keyvalue to allow for recomposition of an outgoing message from the parsed message structure.



Revision History

15-Dec-02	Fixed defect in xap_poll_incoming which caused recvfrom to
		fail / multiple messages to merge & occasionally returned
		incorrect size of message
		
30-Dec-02	Added support for wildcard comparision (xap_compare)
		
*/		

int xap_compare(const char* a_item1, const char* a_item2) {
	
	int i_cursor1=0;
	int i_cursor2=0;
	int i_done=0;
	int i_match=1;
	
	while (!i_done) {
		
		if (a_item1[i_cursor1]=='*') {
			// skip field
			while ((a_item1[i_cursor1]) && (a_item1[i_cursor1]!='.') && (a_item1[i_cursor1]!=':')) i_cursor1++;
			while ((a_item2[i_cursor2]) && (a_item2[i_cursor2]!='.') && (a_item2[i_cursor2]!=':')) i_cursor2++;
		}
		else
		if (a_item2[i_cursor2]=='*') {
			// skip field
			while ((a_item1[i_cursor1]) && (a_item1[i_cursor1]!='.') && (a_item1[i_cursor1]!=':')) i_cursor1++;
			while ((a_item2[i_cursor2]) && (a_item2[i_cursor2]!='.') && (a_item2[i_cursor2]!=':')) i_cursor2++;
		}
		else
		if (a_item1[i_cursor1]=='>') {
			i_done=1;
		}
		else
		if (a_item2[i_cursor2]=='>') {
			i_done=1;
		}
		else
		if (toupper(a_item1[i_cursor1++])!=toupper(a_item2[i_cursor2++])) {
			i_match=0;
			i_done=1;
		}
		////// are we done?
		if ((a_item1[i_cursor1]=='\0')&& (a_item2[i_cursor2]=='\0')) i_done=1;	
		else
		if ((a_item1[i_cursor1]=='\0')&& (a_item2[i_cursor2]!='\0')) { 
			i_match=0;
			i_done=1;	
		}
		else
		if ((a_item1[i_cursor1]!='\0')&& (a_item2[i_cursor2]=='\0')) { 
			i_match=0;
			i_done=1;	
		}
	}
	return i_match;
}


int xap_poll_incoming(int a_server_sockfd, char* a_buffer, int a_buffer_size) {

	int i;  
    	// Check for incoming message, write to buffer return byte count
	i=recvfrom(a_server_sockfd, a_buffer, a_buffer_size-1, 0, 0, 0);
	
	if (i!=-1) {
		a_buffer[i]='\0'; // terminate the buffer so we can treat it as a conventional string
	}
	return i;		
}



int xapmsg_parse(const unsigned char* a_buffer) {

// Parse incoming message.

auto int i_cursor=0;
auto int i;
auto int i_state;

char i_keyname[XAP_MAX_KEYNAME_LEN+1];
char i_keyvalue[XAP_MAX_KEYVALUE_LEN+1];
char i_section[XAP_MAX_SECTION_LEN+1];

#define START_SECTION_NAME 0
#define IN_SECTION_NAME    1
#define START_KEYNAME      2
#define IN_KEYNAME         3
#define START_KEYVALUE     4
#define IN_KEYVALUE        5


i_state=START_SECTION_NAME;
g_xap_index=0;

for (i=0; i<(int)strlen(a_buffer); i++) {

switch (i_state) {

case START_SECTION_NAME:
	if ((a_buffer[i]>32) && (a_buffer[i]<128)) {
		i_state ++;
		i_cursor=0;
		i_section[i_cursor++]=a_buffer[i];
		
		}
	break;

case IN_SECTION_NAME:
	if (a_buffer[i]!='{') {
		if (i_cursor<XAP_MAX_SECTION_LEN) i_section[i_cursor++]=a_buffer[i];
	}
	else {
		while (i_section[i_cursor-1]<=32) {
			i_cursor--; // remove possible single trailing space
		}
		i_section[i_cursor]='\0';
		i_state++;
		
		}
	break;

case START_KEYNAME:
	if (a_buffer[i]=='}') {
		i_state=START_SECTION_NAME; // end of this section
		}
	else
	if ((a_buffer[i]>32) && (a_buffer[i]<128)) {
		i_state ++;
		i_cursor=0;
		if (i_cursor<XAP_MAX_KEYNAME_LEN) i_keyname[i_cursor++]=a_buffer[i];
		
		}
	break;

case IN_KEYNAME:
	// Key name starts with printable character, ends with printable character
	// but may include embedded spaces
	if ((a_buffer[i]>=32) && (a_buffer[i]!='=')) {
		i_keyname[i_cursor++]=a_buffer[i];
	}
	else {
		if (i_keyname[i_cursor-1]==' ') i_cursor--; // remove possible single trailing space
		i_keyname[i_cursor]='\0';
		i_state++;
		
		}
	break;

case START_KEYVALUE:
	if ((a_buffer[i]>32) && (a_buffer[i]<128)) {
		i_state ++;
		
		i_cursor=0;
		if (i_cursor<XAP_MAX_KEYVALUE_LEN) i_keyvalue[i_cursor++]=a_buffer[i];
		}
	break;

case IN_KEYVALUE:
	if (a_buffer[i]>=32) {
		i_keyvalue[i_cursor++]=a_buffer[i];
	}
	else { // end of key value pair
	
	   i_keyvalue[i_cursor]='\0';
		i_state=START_KEYNAME;

		strcpy(g_xap_msg[g_xap_index].section, i_section);
		strcpy(g_xap_msg[g_xap_index].name, i_keyname);
		strcpy(g_xap_msg[g_xap_index].value, i_keyvalue);
		if (g_debuglevel_xap>=DEBUG_DEBUG) Flog_Ecrire("XAPLIB Msg: Name=<%s:%s>, Value=<%s>",g_xap_msg[g_xap_index].section, g_xap_msg[g_xap_index].name, g_xap_msg[g_xap_index].value);
		g_xap_index++;
		
		if (g_xap_index>XAP_MAX_MSG_ELEMENTS) 
		{
			g_xap_index=0;
			
			if (g_debuglevel_xap>=DEBUG_INFO) Flog_Ecrire("XAPLIB Warning: data lost (message too big)");
		}
		}
	
	break;

	} // switch
  } // for
  return g_xap_index;
} // parse


int xapmsg_getvalue(const char* a_keyname, char* a_keyvalue) {

// Retrieve a keyvalue in form <section>:<keyvalue>
// Return 1 on success, 0 on failure

auto int i;
auto int matched;
char i_composite_name[XAP_MAX_SECTION_LEN+XAP_MAX_KEYVALUE_LEN+1];

i=0;
matched=0;

while ((!matched) && (i++<g_xap_index)) {
//	printf("comparing <%s> with <%s> for %d\n",g_xap_msg[i-1].name, a_keyname,i);
	strcpy(i_composite_name, g_xap_msg[i-1].section);
	strcat(i_composite_name,":");
	strcat(i_composite_name, g_xap_msg[i-1].name);
	if (strcasecmp(i_composite_name, a_keyname)==0) {
		matched=1;
		strcpy(a_keyvalue, g_xap_msg[i-1].value);
	} // if
} // while
return matched;
} // getvalue

int xapmsg_updatevalue(const char* a_keyname, const char* a_keyvalue) {

// Change a keyvalue specified in form <section>.<keyvalue>
// Return 1 on success, 0 on failure

auto int i;
auto int matched;
char i_composite_name[XAP_MAX_SECTION_LEN+XAP_MAX_KEYVALUE_LEN+1];

i=0;
matched=0;

while ((!matched) && (i++<g_xap_index)) {
	strcpy(i_composite_name, g_xap_msg[i-1].section);
	strcat(i_composite_name,":");
	strcat(i_composite_name, g_xap_msg[i-1].name);
	if (strcasecmp(i_composite_name, a_keyname)==0) {
		matched=1;
		strncpy( g_xap_msg[i-1].value, a_keyvalue, XAP_MAX_KEYVALUE_LEN);
	} // if
} // while
return matched;
} // getvalue


int xapmsg_toraw(char* a_raw_msg){
	
// Convert the parsed message structure back into raw xAP wireformat
// NB. This assumes that the message structure is correctly ordered
// with header, and subsequent message bodies sorted chronologically.
int i;
char i_active_section[XAP_MAX_SECTION_LEN+1];
const char* XAPMSG_TORAW_ENDBLOCK="}\n";
const char* XAPMSG_TORAW_STARTBLOCK="\n{\n";

i_active_section[0]='\0';
a_raw_msg[0]='\0';

for (i=0; i<g_xap_index; i++) {
	if (strcmp(g_xap_msg[i].section, i_active_section)!=0) {
		// new section
		
		if (i_active_section[0]!='\0') {
			//this isn't the first section
			strcat(a_raw_msg, XAPMSG_TORAW_ENDBLOCK);
		}
		strcat(a_raw_msg, g_xap_msg[i].section); // append new section header
		strcat(a_raw_msg, XAPMSG_TORAW_STARTBLOCK);
		
		strcpy(i_active_section, g_xap_msg[i].section);
	}
	strcat(a_raw_msg, g_xap_msg[i].name);
	strcat(a_raw_msg, "=");
	strcat(a_raw_msg, g_xap_msg[i].value);
	strcat(a_raw_msg, "\n");
} // for i
	strcat(a_raw_msg, "}\n");  // arguable whether last "\n" is required. Spec is vague!
	return (int)strlen(a_raw_msg);
}


int xapmsg_gettype(void) {
	if (g_xap_index==0) return XAP_MSG_NONE;
	if (strcasecmp(g_xap_msg[0].section,"xap-hbeat")==0) return XAP_MSG_HBEAT;
	if (strcasecmp(g_xap_msg[0].section,"xap-header")==0) return XAP_MSG_ORDINARY;
	if (strcasecmp(g_xap_msg[0].section,"xap-config-request")==0) return XAP_MSG_CONFIG_REQUEST;
	if (strcasecmp(g_xap_msg[0].section,"xap-cache-request")==0) return XAP_MSG_CACHE_REQUEST;
	if (strcasecmp(g_xap_msg[0].section,"xap-cache-reply")==0) return XAP_MSG_CACHE_REPLY;
	if (strcasecmp(g_xap_msg[0].section,"xap-config-reply")==0) return XAP_MSG_CONFIG_REPLY;
	
	
	return XAP_MSG_UNKNOWN;
}
