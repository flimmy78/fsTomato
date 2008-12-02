
/*
 *********************************************************
 *   Copyright 2003, CyberTAN  Inc.  All Rights Reserved *
 *********************************************************

 This is UNPUBLISHED PROPRIETARY SOURCE CODE of CyberTAN Inc.
 the contents of this file may not be disclosed to third parties,
 copied or duplicated in any form without the prior written
 permission of CyberTAN Inc.

 This software should be used as a reference only, and it not
 intended for production use!


 THIS SOFTWARE IS OFFERED "AS IS", AND CYBERTAN GRANTS NO WARRANTIES OF ANY
 KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE.  CYBERTAN
 SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>

#include <broadcom.h>


/* Dump route in <tr><td>IP</td><td>MASK</td><td>GW</td><td>Hop Count</td><td>interface</td></tr> format */
int
ej_dump_route_table(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	int count = 0;
	char *format;
	FILE *fp, *fp1;
	int flgs, ref, use, metric;
        unsigned long dest, gw, netmask;
	char line[256];
	struct in_addr dest_ip;
        struct in_addr gw_ip;
        struct in_addr netmask_ip;
	char sdest[16], sgw[16];
	int debug = 0, blank = 1;

	if (ejArgs(argc, argv, "%s", &format) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	/* open route table*/
	if ((fp = fopen("/tmp/routed_gb", "r")) == NULL) {
		if ((fp = fopen("/proc/net/route", "r")) == NULL) {
       			websError(wp, 400, "No route table\n");
        		return -1;
		}
	}

        /* Read the route cache entries. */
	//Iface   Destination     Gateway         Flags   RefCnt  Use     Metric  Mask            MTU     Window  IRTT                                                       
	//vmnet1  004410AC        00000000        0001    0       0       0       00FFFFFF        40      0       0                   

	while( fgets(line, sizeof(line), fp) != NULL ) {
		if(count) {
			int ifl = 0;
			while(line[ifl]!=' ' && line[ifl]!='\t' && line[ifl]!='\0')
			ifl++;
			line[ifl]=0;    /* interface */
			if(sscanf(line+ifl+1, "%lx%lx%X%d%d%d%lx", &dest, &gw, &flgs, &ref, &use, &metric, &netmask)!=7) {
				break;
		        }  
			debug = 0;
			dest_ip.s_addr = dest;
			gw_ip.s_addr   = gw;
			netmask_ip.s_addr = netmask;

			strcpy(sdest,  (dest_ip.s_addr==0 ? "0.0.0.0" : inet_ntoa(dest_ip)));    //default
			strcpy(sgw,    (gw_ip.s_addr==0   ? "0.0.0.0" : inet_ntoa(gw_ip)));      //*

#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(): route table[%d] dest[%lx][%s] gw[%lx][%s] flags[%X] ref[%d] use[%d] metric[%d] netmask[%lx][%s] [%s]",__FUNCTION__,count,dest,sdest,gw,sgw,flgs,ref,use,metric,netmask,inet_ntoa(netmask_ip),(flgs&RTF_UP ? "" : "deleted"));
#endif
		
			/* not 0x0001 route usable */
			if(!(flgs&RTF_UP))	continue; 

			/* filter ppp pseudo interface for DOD */
			if(!strcmp(sdest,PPP_PSEUDO_IP) && !strcmp(sgw,PPP_PSEUDO_GW))	debug = 1;
			
			/* Don't show loopback device */
			if(!strcmp(line,"lo"))	debug = 1;

			/* Don't show eth1 information for pppoe mode */
			if(!strcmp(line, nvram_safe_get("wan_ifname")) && nvram_match("wan_proto","pppoe"))	debug = 1;

#ifdef UNNUMBERIP_SUPPORT
			/* Don't show eth1 information for Unnumber ip mode */
                        if(!strcmp(line, nvram_safe_get("wan_ifname")) && nvram_match("wan_proto","unnumberip")) debug = 1;
#endif
#ifdef WAN_AUTO_DETECT_SUPPORT
			/* Don't show eth1 information for pppoe mode */
                        if(!strcmp(line, nvram_safe_get("wan_ifname")) && nvram_match("wan_proto","auto_pppoe")) debug = 1;
#endif

#if 1
			/* Don't show pseudo interface */
                        if(!strncmp(line, nvram_safe_get("pppoe_ifname0"), 4)){
                                fp1 = fopen("/tmp/ppp/link", "r");
                                if(!fp1)        debug = 1;
                                else            fclose(fp1);
                        }

			/* Don't show pseudo interface */
                        if(!strncmp(line, nvram_safe_get("pppoe_ifname1"), 4)){
                                fp1 = fopen("/tmp/ppp/link_1", "r");
                                if(!fp1)        debug = 1;
                                else            fclose(fp1);
                        }
#else
			/* Don't show pseudo interface */
			if(!strncmp(line, "ppp", 3)){
				fp1 = fopen("/tmp/ppp/link", "r");
				if(!fp1)	debug = 1;
				else		fclose(fp1);
			}
#endif

			ret += websWrite(wp,"%s%c'%s','%s','%s','%d','%s'\n",
					debug ? "//" : "",
					blank ? ' ' : ',', 
					sdest,
					inet_ntoa(netmask_ip),
					sgw,
                                        ((metric + 1) > 16) ? 16 : (metric + 1),//2006.10.09 add by zg
					(strcmp(line,"br0") ? "WAN" : "LAN"));    

			if(debug && blank)	blank = 1;
			else		blank = 0;


		}

		count++;
	}

	return ret;
}

void
validate_dynamic_route(webs_t wp, char *value, struct variable *v)
{
	struct variable dr_variables[] = {
		{ longname: "Danamic Route", argv: ARGV("0", "1", "2", "3") },
	}, *which;
	char *dr_setting;
	
	which = &dr_variables[0];

	if (valid_choice(wp, value, v)) 
		nvram_set(v->name, value);

	dr_setting = websGetVar(wp, "dr_setting", NULL);
	if(!dr_setting)	return;

	if (!valid_choice(wp, dr_setting, &which[0])) 
		return;
		nvram_set("dr_setting",dr_setting);
	if(!dr_setting || atoi(dr_setting) == 0){
		nvram_set("dr_lan_tx","0");
		nvram_set("dr_lan_rx","0");
		nvram_set("dr_wan_tx","0");
		nvram_set("dr_wan_rx","0");
	}
	else if(atoi(dr_setting) == 1){
		nvram_set("dr_lan_tx","1 2");
		nvram_set("dr_lan_rx","1 2");
		nvram_set("dr_wan_tx","0");
		nvram_set("dr_wan_rx","0");
	}
	else if(atoi(dr_setting) == 2){
		nvram_set("dr_lan_tx","0");
		nvram_set("dr_lan_rx","0");
		nvram_set("dr_wan_tx","1 2");
		nvram_set("dr_wan_rx","1 2");
	}
	else if(atoi(dr_setting) == 3){ 
		nvram_set("dr_lan_tx","1 2");
		nvram_set("dr_lan_rx","1 2");
		nvram_set("dr_wan_tx","1 2");
		nvram_set("dr_wan_rx","1 2");
	}
	else{
		nvram_set("dr_lan_tx","0");
		nvram_set("dr_lan_rx","0");
		nvram_set("dr_wan_tx","0");
		nvram_set("dr_wan_rx","0");
	}

}