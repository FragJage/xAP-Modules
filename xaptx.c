#include "Fichier.h"
#include "xapdef.h"

int xap_send_tick(int a_interval) {

	 
	 time_t i_timenow;
	 static time_t i_sendtick=0;
	 
	 i_timenow = time((time_t*)0);

		// Check timer for heartbeat send

		if ((i_timenow-i_sendtick>=a_interval)||(i_sendtick==0)) {												
 			i_sendtick=i_timenow;
			return 1;
		} // send tick
	return 0;
}



int xap_heartbeat_tick(int a_interval) {

	 // Call this function periodically.
	 // Every HBEAT_INTERVAL, it will send a heartbeat

	 time_t i_timenow;
	 static time_t i_heartbeattick=0;
	 
	 i_timenow = time((time_t*)0);

		// Check timer for heartbeat send

		if ((i_timenow-i_heartbeattick>=a_interval)||(i_heartbeattick==0)) {												
 			i_heartbeattick=i_timenow;
			// Send heartbeat to all external listeners
			xap_broadcast_heartbeat(g_xap_sender_sockfd, &g_xap_sender_address,a_interval); 
			return 1;
		} // heartbeat tick
	return 0;
}


// Send a heartbeat to the real world

int xap_broadcast_heartbeat(int a_sock,  struct sockaddr_in* a_addr, int interval) {

	int i;
	char i_buff[1500];

	// Construct the heartbeat message (to say we are alive)
	// If using a hub, this must be sent on startup, because
	// that is how the hub knows who we are
	sprintf(i_buff, "xap-hbeat\n{\nv=12\nhop=1\nuid=%s\nclass=xap-hbeat.alive\nsource=%s.%s.%s\ninterval=%d\nport=%d\n}\n", g_uid, XAP_ME, XAP_SOURCE,  g_instance, interval, ntohs(g_xap_receiver_address.sin_port));
	if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Heartbeat source=%s, instance=%s, interval=%d, port=%d",XAP_SOURCE, g_instance, interval, 	ntohs(g_xap_receiver_address.sin_port) );

	// Send it...
    i = sendto(a_sock, i_buff, (int)strlen(i_buff), 0, (struct sockaddr *)a_addr, sizeof(struct sockaddr));	
	if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Broadcasting heartbeat");
	
	return i;
}


// Build an outgoing xap (broadcast) message 

int xap_send_message(const char* a_buff) {

	// Send it...
    sendto(g_xap_sender_sockfd, a_buff, (int)strlen(a_buff), 0, (struct sockaddr *)&g_xap_sender_address, sizeof(struct sockaddr));	
	if (g_debuglevel_xap>=DEBUG_VERBOSE) Flog_Ecrire("Outgoing xAP message sent");	
	return 1;
}
