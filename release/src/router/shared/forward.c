
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

#include <broadcom.h>
#include <cy_conf.h>

#define DEL_IP_CONNTRACK_ENTRY 1

#ifdef UPNP_FORWARD_SUPPORT
struct upnp {
	char *name;
	char *from;
	char *proto;
};
struct upnp upnp_default[] = {
	{ "FTP", "21" ,"tcp" },
	{ "Telnet", "23" ,"tcp" },
	{ "SMTP", "25" ,"tcp" },
	{ "DNS", "53" ,"udp" },
	{ "TFTP", "69" ,"udp" },
	{ "finger", "79" ,"tcp" },
	{ "HTTP", "80" ,"tcp" },
	{ "POP3", "110" ,"tcp" },
	{ "NNTP", "119" ,"tcp" },
	{ "SNMP", "161" ,"udp" },
};
#endif

#ifdef ALG_FORWARD_SUPPORT
struct alg {
	char *name;
	char *from;
	char *proto;
};
struct alg alg_default[] = {
	{ "FTP", "21" ,"tcp" },
	{ "Telnet", "32" ,"tcp" },
	{ "SMTP", "25" ,"tcp" },
	{ "DNS", "53" ,"udp" },
	{ "TFTP", "69" ,"udp" },
	{ "finger", "79" ,"tcp" },
	{ "HTTP", "80" ,"tcp" },
	{ "POP3", "110" ,"tcp" },
	{ "NNTP", "119" ,"tcp" },
	{ "SNMP", "161" ,"udp" },
};
#endif

/* Example:
 * name:[on|off]:[tcp|udp|both]:8000:80>100
 */

void
validate_forward_proto(webs_t wp, char *value, struct variable *v)
{
	int i, error = 0;
	char buf[1000] = "", *cur = buf;
	#ifdef DEL_IP_CONNTRACK_ENTRY
        char buf_ip_conntrack[1000] = "", *cur_ip_conntrack = buf_ip_conntrack;
        #endif

	struct variable forward_proto_variables[] = {
		{ longname: "Port Forward Application name", argv: ARGV("12") },
		{ longname: "Port Forward from WAN Port", argv: ARGV("0", "65535") },
		{ longname: "Port Forward to LAN Port", argv: ARGV("0", "65535") },
		{ longname: "Port Forward Protocol",NULL },
	 	{ longname: "Port Forward LAN IP Address", argv: ARGV("0","254") },
	}, *which;

	for (i = 0; i < FORWARDING_NUM; i++) {

		char forward_name[] = "nameXXX";
                char forward_from[] = "fromXXX";
                char forward_to[] = "toXXX";
                char forward_ip[] = "ipXXX";
                char forward_tcp[] = "tcpXXX";	// for checkbox
                char forward_udp[] = "udpXXX";	// for checkbox
                char forward_pro[] = "proXXX";	// for select, cisco style UI
                char forward_enable[] = "enableXXX";
                char *name="", new_name[200]="", *from="", *to="", *ip="", *tcp="", *udp="", *enable="", proto[10], *pro="";

                snprintf(forward_name, sizeof(forward_name), "name%d", i);
                snprintf(forward_from, sizeof(forward_from), "from%d", i);
                snprintf(forward_to, sizeof(forward_to), "to%d", i);
                snprintf(forward_ip, sizeof(forward_ip), "ip%d", i);
                snprintf(forward_tcp, sizeof(forward_tcp), "tcp%d", i);
                snprintf(forward_udp, sizeof(forward_udp), "udp%d", i);
                snprintf(forward_enable, sizeof(forward_enable), "enable%d", i);
                snprintf(forward_pro, sizeof(forward_pro), "pro%d", i);
	
		name = websGetVar(wp, forward_name, "");
		from = websGetVar(wp, forward_from, "0");
		to = websGetVar(wp, forward_to, "0");
		ip = websGetVar(wp, forward_ip, "0");
		tcp = websGetVar(wp, forward_tcp, NULL);	// for checkbox
		udp = websGetVar(wp, forward_udp, NULL);	// for checkbox
		pro = websGetVar(wp, forward_pro, NULL);	// for select option
		enable = websGetVar(wp, forward_enable, "off");

		which = &forward_proto_variables[0];

#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(%d): %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s]",__FUNCTION__,i,forward_name,name,forward_from,from,forward_to,to,forward_ip,ip,forward_tcp,tcp,forward_udp,udp,forward_enable,enable);
#endif

		if(!*from && !*to && !*ip)
			continue;
		if(!strcmp(ip,"0") || !strcmp(ip,"")||(atoi(ip) == get_single_ip(nvram_get("lan_ipaddr"), 3)))
			continue;
		if((!strcmp(from,"0") || !strcmp(from,"")) && 
		   (!strcmp(to,"0") || !strcmp(to,"")) && 
		   (!strcmp(ip,"0") || !strcmp(ip,"")))	{
			continue;
		}

		/* check name */
		if(strcmp(name,"")){
			if(!valid_name(wp, name, &which[0])){
				error_value = 1;
				continue;
			}
			else{
				filter_name(name, new_name, sizeof(new_name), SET);
			}
		}
		
		if (!strcmp(from,"")) from = to;
		if (!strcmp(to,"")) to = from;

		if(atoi(from) > atoi(to)){
			SWAP(from, to);
		}

		if(!valid_range(wp, from, &which[1]) || !valid_range(wp, to, &which[2])){
			error_value = 1;
			continue;
		}


		if(pro){	// use select option
			strcpy(proto, pro);
		}
		else{		// use checkbox
			if(tcp && udp)
				strcpy(proto,"both");
			else if(tcp && !udp)
				strcpy(proto,"tcp");
			else if(!tcp && udp)
				strcpy(proto,"udp");
		}
		/* check ip address */

		if(!*ip){
			error = 1;
		//	websWrite(wp, "Invalid <b>%s</b> : must specify a ip<br>",which[4].longname);
                        continue;
		}
		
		if (!valid_range(wp, ip, &which[4])){
			error = 1;
			continue;
		}

		cur += snprintf(cur, buf + sizeof(buf) - cur, "%s%s:%s:%s:%d:%d>%d",
				cur == buf ? "" : " ", new_name, enable, proto, atoi(from), atoi(to), atoi(ip));
#ifdef DEL_IP_CONNTRACK_ENTRY
                if(!strcmp(enable, "off"))
                {
                        int tp;
                        if(!strcmp(proto, "both"))
                        {
                                tp = 2;
                        }
                        else if(!strcmp(proto, "udp"))
                        {
                                tp = 1;
                        }
                        else if(!strcmp(proto, "tcp"))
                        {
                                tp = 0;
                        }
                        else
                        {
                                tp = -1;
                        }
                        if(tp == 0 || tp == 1 || tp == 2)
                        {
                                cur_ip_conntrack += snprintf(cur_ip_conntrack, buf_ip_conntrack + sizeof(buf_ip_conntrack) - cur_ip_conntrack, "%s%d %s %d-%d",
                                        cur_ip_conntrack == buf_ip_conntrack ? "" : "\n", tp, get_complete_lan_ip(ip), atoi(from), atoi(to));
                        }
 
                }
#endif

	}
	if(!error)
	{
		nvram_set(v->name, buf);
#ifdef DEL_IP_CONNTRACK_ENTRY
#define DEL_LIST_PATH "/tmp/.del_ip_conntrack"
                buf_to_file(DEL_LIST_PATH, buf_ip_conntrack);
#endif	
	}
}

#ifdef SPECIAL_FORWARD_SUPPORT
/* Example:
 * name:[on|off]:[tcp|udp]:8000:80>100
 */
void
validate_forward_spec(webs_t wp, char *value, struct variable *v)
{
	int i, error = 0;
	char buf[1000] = "", *cur = buf;
	struct variable forward_spec_variables[] = {
		{ longname: "UPnP Forward Application name", argv: ARGV("12") },
		{ longname: "UPnP Forward from WAN Port", argv: ARGV("0", "65535") },
		{ longname: "UPnP Forward to LAN Port", argv: ARGV("0", "65535") },
		{ longname: "UPnP Forward Protocol",NULL },
	 	{ longname: "UPnP Forward LAN IP Address", argv: ARGV("0", "254") },
	}, *which;

	for (i = 0; i < SPECIAL_FORWARDING_NUM; i++) {

		char spec_name[] = "nameXXX";
                char spec_from[] = "fromXXX";
                char spec_to[] = "toXXX";
                char spec_ip[] = "ipXXX";
                char spec_tcp[] = "tcpXXX";
                char spec_udp[] = "udpXXX";
                char spec_enable[] = "enableXXX";
                char *name=NULL, new_name[200]="", *from=NULL, *to=NULL, *ip=NULL, *enable=NULL, *tcp=NULL, *udp=NULL;
		char proto[10];

                snprintf(spec_name, sizeof(spec_name), "name%d", i);
                snprintf(spec_from, sizeof(spec_from), "from%d", i);
                snprintf(spec_to, sizeof(spec_to), "to%d", i);
                snprintf(spec_ip, sizeof(spec_ip), "ip%d", i);
                snprintf(spec_tcp, sizeof(spec_tcp), "tcp%d", i);
                snprintf(spec_udp, sizeof(spec_udp), "udp%d", i);
                snprintf(spec_enable, sizeof(spec_enable), "enable%d", i);

		name = websGetVar(wp, spec_name, ""); 
		from = websGetVar(wp, spec_from, "0");
		tcp = websGetVar(wp, spec_tcp, NULL);
		udp = websGetVar(wp, spec_udp, NULL);
		to = websGetVar(wp, spec_to, "0");
		ip = websGetVar(wp, spec_ip, "0");
		enable = websGetVar(wp, spec_enable, "off");

		which = &forward_spec_variables[0];

#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(%d): %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s]",
			__FUNCTION__, i,
			spec_name, name,
			spec_from, from,
			spec_to, to,
			spec_ip, ip,
			spec_tcp, tcp,
			spec_udp, udp,
			spec_enable, enable);
#endif
		if((!strcmp(from,"0") || !strcmp(from,"")) && 
		   (!strcmp(to,"0") || !strcmp(to,"")) && 
		   (!strcmp(ip,"0") || !strcmp(ip,"")))	
			continue;

		/* check name */
		if(name && strcmp(name,"")){
			if(!valid_name(wp, name, &which[0])){
				error = 1;
				continue;
			}
			else{
				filter_name(name, new_name, sizeof(new_name), SET);
			}
		}
		
		/* check PORT number */
		if(!*from && !*to){
			error = 1;
			websWrite(wp, "Invalid <b>%s</b> %s: must specify a port<br>",which[1].longname,from);
			websWrite(wp, "Invalid <b>%s</b> %s: must specify a port<br>",which[2].longname,to);
                        continue;
		}

		if(!valid_range(wp, from, &which[1]) || !valid_range(wp, to, &which[2])){
			error = 1;
			continue;
		}

		/* proto */
		if(tcp && udp)
			strcpy(proto,"both");
		else if(tcp && !udp)
			strcpy(proto,"tcp");
		else if(!tcp && udp)
			strcpy(proto,"udp");


		/* check ip address */
		if(!*ip){
			error = 1;
			websWrite(wp, "Invalid <b>%s</b> : must specify a ip<br>",which[4].longname);
                        continue;
		}
		
		if (!valid_range(wp, ip, &which[4])){
			error = 1;
			continue;
		}

		cur += snprintf(cur, buf + sizeof(buf) - cur, "%s%s:%s:%s:%s>%s:%s",
				cur == buf ? "" : " ", new_name, enable, proto, from, to, ip);

	}
	if(!error)
		nvram_set(v->name, buf);
}
#endif

#ifdef UPNP_FORWARD_SUPPORT
/* Example:
 * name:[on|off]:[tcp|udp]:8000:80>100
 */

void
validate_forward_upnp(webs_t wp, char *value, struct variable *v)
{
	int i, error = 0;
	char buf[1000] = "";
	struct variable forward_upnp_variables[] = {
		{ longname: "UPnP Forward Application name", argv: ARGV("12") },
		{ longname: "UPnP Forward from WAN Port", argv: ARGV("0", "65535") },
		{ longname: "UPnP Forward to LAN Port", argv: ARGV("0", "65535") },
		{ longname: "UPnP Forward Protocol",NULL },
	 	{ longname: "UPnP Forward LAN IP Address", argv: ARGV("0", "254") },
	}, *which;

	for (i = 0; i < UPNP_FORWARDING_NUM; i++) {

		char upnp_name[] = "nameXXX";
                char upnp_from[] = "fromXXX";
                char upnp_to[] = "toXXX";
                char upnp_ip[] = "ipXXX";
                char upnp_proto[] = "protoXXX";
                char upnp_enable[] = "enableXXX";
                char *name="", new_name[200]="", *from="", *to="", *ip="", *enable="", *proto="";
		char forward_name[] = "forward_portXXXXXXXXXX";

                snprintf(upnp_name, sizeof(upnp_name), "name%d", i);
                snprintf(upnp_from, sizeof(upnp_from), "from%d", i);
                snprintf(upnp_to, sizeof(upnp_to), "to%d", i);
                snprintf(upnp_ip, sizeof(upnp_ip), "ip%d", i);
                snprintf(upnp_proto, sizeof(upnp_proto), "proto%d", i);
                snprintf(upnp_enable, sizeof(upnp_enable), "enable%d", i);

		if(i >= 10){	
			name = websGetVar(wp, upnp_name, ""); 
			from = websGetVar(wp, upnp_from, "0");
			proto = websGetVar(wp, upnp_proto, "");
		}
		else {
			name = upnp_default[i].name;
			from = upnp_default[i].from;
			proto = upnp_default[i].proto;
		}
		to = websGetVar(wp, upnp_to, "0");
		ip = websGetVar(wp, upnp_ip, "0");
		enable = websGetVar(wp, upnp_enable, "off");

		which = &forward_upnp_variables[0];

#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(%d): %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s]",__FUNCTION__,i,upnp_name,name,upnp_from,from,upnp_to,to,upnp_ip,ip,upnp_proto,proto,upnp_enable,enable);
#endif
		
		if((!strcmp(from,"0") || !strcmp(from,"")) && 
		   (!strcmp(to,"0") || !strcmp(to,"")) && 
		   (!strcmp(ip,"0") || !strcmp(ip,"")))	
			continue;

		/* check name */
		if(strcmp(name,"")){
			if(!valid_name(wp, name, &which[0])){
				error = 1;
				continue;
			}
			else{
				filter_name(name, new_name, sizeof(new_name), SET);
			}
		}
		
		/* check PORT number */

		if(!valid_range(wp, from, &which[1]) || !valid_range(wp, to, &which[2])){
			error = 1;
			continue;
		}

		/* check ip address */
		
		if (!valid_range(wp, ip, &which[4])){
			error = 1;
			continue;
		}


		snprintf(buf, sizeof(buf), "%s-%s>%s:%s-%s,%s,%s,%s", from, from, get_complete_lan_ip(ip), to, to, proto, enable, name);
		//cprintf("buf=[%s]\n", buf);
		
		snprintf(forward_name, sizeof(forward_name), "forward_port%d", i);
		nvram_set(forward_name, buf);
	}
}
#endif

#ifdef ALG_FORWARD_SUPPORT
/* Example:
 * name:[on|off]:[tcp|udp]:8000:80>100
 */

void
validate_forward_alg(webs_t wp, char *value, struct variable *v)
{
	int i, error = 0;
	char buf[1000] = "";
	struct variable forward_alg_variables[] = {
		{ longname: "ALG Forward Application name", argv: ARGV("12") },
		{ longname: "ALG Forward from WAN Port", argv: ARGV("0", "65535") },
		{ longname: "ALG Forward to LAN Port", argv: ARGV("0", "65535") },
		{ longname: "ALG Forward Protocol",NULL },
	 	{ longname: "ALG Forward LAN IP Address", argv: ARGV("0", "254") },
	}, *which;

	for (i = 0; i < ALG_FORWARDING_NUM; i++) {

		char alg_name[] = "nameXXX";
                char alg_from[] = "fromXXX";
                char alg_to[] = "toXXX";
                char alg_ip[] = "ipXXX";
                char alg_proto[] = "protoXXX";
                char alg_enable[] = "enableXXX";
                char alg_policy[] = "policyXXX";
                char *name="", new_name[200]="", *from="", *to="", *ip="", *enable="", *proto="", *policy="";
		char forward_name[] = "forward_algXXXXXXXXXX";

                snprintf(alg_name, sizeof(alg_name), "name%d", i);
                snprintf(alg_from, sizeof(alg_from), "from%d", i);
                snprintf(alg_to, sizeof(alg_to), "to%d", i);
                snprintf(alg_ip, sizeof(alg_ip), "ip%d", i);
                snprintf(alg_proto, sizeof(alg_proto), "proto%d", i);
                snprintf(alg_enable, sizeof(alg_enable), "enable%d", i);
                snprintf(alg_policy, sizeof(alg_policy), "policy%d", i);

		if(i >= 10){	
			name = websGetVar(wp, alg_name, ""); 
			from = websGetVar(wp, alg_from, "0");
			proto = websGetVar(wp, alg_proto, "");
		}
		else {
			name = alg_default[i].name;
			from = alg_default[i].from;
			proto = alg_default[i].proto;
		}
		to = websGetVar(wp, alg_to, "0");
		ip = websGetVar(wp, alg_ip, "0");
		enable = websGetVar(wp, alg_enable, "off");
		policy = websGetVar(wp, alg_policy, "accept");

		which = &forward_alg_variables[0];

#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(%d): %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s]",__FUNCTION__,i,alg_name,name,alg_from,from,alg_to,to,alg_ip,ip,alg_proto,proto,alg_enable,enable);
#endif
		
		if((!strcmp(from,"0") || !strcmp(from,"")) && 
		   (!strcmp(to,"0") || !strcmp(to,"")) && 
		   (!strcmp(ip,"0") || !strcmp(ip,"")))	
			continue;

		/* check name */
		if(strcmp(name,"")){
			if(!valid_name(wp, name, &which[0])){
				error = 1;
				continue;
			}
			else{
				filter_name(name, new_name, sizeof(new_name), SET);
			}
		}
		
		/* check PORT number */

		if(!valid_range(wp, from, &which[1]) || !valid_range(wp, to, &which[2])){
			error = 1;
			continue;
		}

		/* check ip address */
		
		if (!valid_range(wp, ip, &which[4])){
			error = 1;
			continue;
		}


		snprintf(buf, sizeof(buf), "%s-%s>%s:%s-%s,%s,%s,%s,%s", from, from, get_complete_lan_ip(ip), to, to, proto, policy, enable, name);
		//cprintf("buf=[%s]\n", buf);
		
		snprintf(forward_name, sizeof(forward_name), "forward_alg%d", i);
		nvram_set(forward_name, buf);
	}
}
#endif

#ifdef PORT_TRIGGER_SUPPORT
/* Example:
 * name:on:both:1000-2000>3000-4000
 */

void
validate_port_trigger(webs_t wp, char *value, struct variable *v)
{	
	int i, error = 0;
	char buf[1000] = "", *cur = buf;
	struct variable trigger_variables[] = {
		{ longname: "Port Trigger Application name",argv: ARGV("12") },
		{ longname: "Port Trigger from WAN Port", argv: ARGV("0", "65535") },
		{ longname: "Port Trigger from WAN Port", argv: ARGV("0", "65535") },
		{ longname: "Port Trigger to LAN Port", argv: ARGV("0", "65535") },
		{ longname: "Port Trigger to LAN Port", argv: ARGV("0", "65535") },
	}, *which;

	for (i = 0; i < PORT_TRIGGER_NUM; i++) {

		char trigger_name[] = "nameXXX";
		char trigger_enable[] = "enableXXX";
                char trigger_i_from[] = "i_fromXXX";
                char trigger_i_to[] = "i_toXXX";
                char trigger_o_from[] = "o_fromXXX";
                char trigger_o_to[] = "o_toXXX";
                char *name="", *enable, new_name[200]="", *i_from="", *i_to="", *o_from="", *o_to="";

                snprintf(trigger_name, sizeof(trigger_name), "name%d", i);
                snprintf(trigger_enable, sizeof(trigger_enable), "enable%d", i);
                snprintf(trigger_i_from, sizeof(trigger_i_from), "i_from%d", i);
                snprintf(trigger_i_to, sizeof(trigger_i_to), "i_to%d", i);
                snprintf(trigger_o_from, sizeof(trigger_o_from), "o_from%d", i);
                snprintf(trigger_o_to, sizeof(trigger_o_to), "o_to%d", i);
	
		name = websGetVar(wp, trigger_name, "");
		enable = websGetVar(wp, trigger_enable, "off");
		i_from = websGetVar(wp, trigger_i_from, NULL);
		i_to = websGetVar(wp, trigger_i_to, NULL);
		o_from = websGetVar(wp, trigger_o_from, NULL);
		o_to = websGetVar(wp, trigger_o_to, NULL);

		which = &trigger_variables[0];

#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(%d): %s=[%s] %s=[%s] %s=[%s] %s=[%s] %s=[%s]",__FUNCTION__,i,trigger_name,name,trigger_i_from,i_from,trigger_i_to,i_to,trigger_o_from,o_from,trigger_o_to,o_to);
#endif
		if(!i_from || !i_to || !o_from || !o_to)
			continue;

	
		if( (!strcmp(i_from,"0") || !strcmp(i_from,"")) && 
		    (!strcmp(i_to,"0") || !strcmp(i_to,"")) &&
		    (!strcmp(o_from,"0") || !strcmp(o_from,"")) && 
		    (!strcmp(o_to,"0") || !strcmp(o_to,"")) )	
			continue;

		// We don't want such spec 2005-05-17 by honor
		//if (!strcmp(i_from,"0") || !strcmp(i_from,"")) i_from = i_to;
		//if (!strcmp(i_to,"0") || !strcmp(i_to,"")) i_to = i_from;
		//if (!strcmp(o_from,"0") || !strcmp(o_from,"")) o_from = o_to;
		//if (!strcmp(o_to,"0") || !strcmp(o_to,"")) o_to = o_from;
		
		if(atoi(i_from) > atoi(i_to))
			SWAP(i_from, i_to);
		
		if(atoi(o_from) > atoi(o_to))
			SWAP(o_from, o_to);

		if(strcmp(name,"")){
			if(!valid_name(wp, name, &which[0])){
				error = 1;
				continue;
			}
			else{
				filter_name(name, new_name, sizeof(new_name), SET);
			}
		}

		if(!valid_range(wp, i_from, &which[1]) || !valid_range(wp, i_to, &which[2])||
		   !valid_range(wp, o_from, &which[3]) || !valid_range(wp, o_to, &which[4])){
			error = 1;
			continue;
		}

		cur += snprintf(cur, buf + sizeof(buf) - cur, "%s%s:%s:both:%s-%s>%s-%s",
				cur == buf ? "" : " ", new_name, enable, i_from, i_to, o_from, o_to);

	}

	if(!error)
		nvram_set(v->name, buf);
	
}
#endif

/* Example:
 * name:[on|off]:[tcp|udp|both]:8000:80>100
 */

int
ej_port_forward_table(int eid, webs_t wp, int argc, char_t **argv)
{
	char *type;
	int  which;

	static char word[256];
	char *next, *wordlist;
	char *name, *from, *to, *proto, *ip ,*enable;
	static char new_name[200];
	int temp;
	
        if (ejArgs(argc, argv, "%s %d", &type, &which) < 2) {
             websError(wp, 400, "Insufficient args\n");
             return -1;
        }

	wordlist = nvram_safe_get("forward_port");
	temp = which;
	
	foreach(word, wordlist, next) {
		if (which-- == 0) {
			enable = word;
			name = strsep(&enable, ":");
		 	if (!name || !enable)
                                continue;
			proto = enable;
			enable = strsep(&proto, ":");
			if (!enable || !proto)
				continue;
			from = proto;
			proto = strsep(&from, ":");
			if (!proto || !from)
				continue;
			to = from;
			from = strsep(&to, ":");
			if (!to || !from)
				continue;
			ip = to;
			to = strsep(&ip, ">");
			if (!ip || !to)
				continue;
#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(%s,%d): name=[%s] from=[%s] to=[%s] proto=[%s] ip=[%s] enable=[%s]",__FUNCTION__,part,temp,name,from,to,proto,ip,enable);
#endif

			if (!strcmp(type, "name")){
				filter_name(name, new_name, sizeof(new_name), GET);
				return websWrite(wp,"%s",new_name);
			}
			else if (!strcmp(type, "from"))
				return websWrite(wp,"%s",from);
			else if (!strcmp(type, "to"))
				return websWrite(wp,"%s",to);
			else if (!strcmp(type, "tcp")){		// use checkbox
				if(!strcmp(proto, "udp"))
					return websWrite(wp,"");
				else
					return websWrite(wp,"checked");
			}
			else if (!strcmp(type, "udp")){		//use checkbox
				if(!strcmp(proto, "tcp"))
					return websWrite(wp,"");
				else
					return websWrite(wp,"checked");
			}
			else if (!strcmp(type, "sel_tcp")){		// use select
				if(!strcmp(proto, "udp"))
					return websWrite(wp,"");
				else
					return websWrite(wp,"selected");
			}
			else if (!strcmp(type, "sel_udp")){		//use select
				if(!strcmp(proto, "tcp"))
					return websWrite(wp,"");
				else
					return websWrite(wp,"selected");
			}
			else if (!strcmp(type, "sel_both")){		//use select
				if(!strcmp(proto, "both"))
					return websWrite(wp,"selected");
				else
					return websWrite(wp,"");
			}
			else if (!strcmp(type, "ip"))
				return websWrite(wp,"%s",ip);
			else if (!strcmp(type, "enable")){
				if(!strcmp(enable, "on"))
					return websWrite(wp,"checked");
				else
					return websWrite(wp,"");
			}
		}
	}
 	if( !strcmp(type, "from") || !strcmp(type, "to") || !strcmp(type, "ip"))
		return websWrite(wp,"0");
	else if (!strcmp(type, "sel_both"))
		return websWrite(wp,"selected");
	else
		return websWrite(wp,"");
}

#ifdef PORT_TRIGGER_SUPPORT
/* Example:
 * name:on:both:1000-2000>3000-4000
 */

int
ej_port_trigger_table(int eid, webs_t wp, int argc, char_t **argv)
{
	char *type;
	int  which;
	
	static char word[256];
	char *next, *wordlist;
	char *name=NULL, *enable=NULL, *proto=NULL, *i_from=NULL, *i_to=NULL, *o_from=NULL, *o_to=NULL;
	static char new_name[200];
	int temp;

        if (ejArgs(argc, argv, "%s %d", &type, &which) < 2) {
             websError(wp, 400, "Insufficient args\n");
             return -1;
        }

	wordlist = nvram_safe_get("port_trigger");
	temp = which;
	
	foreach(word, wordlist, next) {
		if (which-- == 0) {
			enable = word;
			name = strsep(&enable, ":");
		 	if (!name || !enable)
                                continue;
			proto = enable;
			enable = strsep(&proto, ":");
		 	if (!enable || !proto)
                                continue;
			i_from = proto;
			proto = strsep(&i_from, ":");
		 	if (!proto || !i_from)
                                continue;
			i_to = i_from;
			i_from = strsep(&i_to, "-");
		 	if (!i_from || !i_to)
                                continue;
			o_from = i_to;
			i_to = strsep(&o_from, ">");
			if (!i_to || !o_from)
				continue;
			o_to = o_from;
			o_from = strsep(&o_to, "-");
			if (!o_from || !o_to)
				continue;
#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(%s,%d): i_from=[%s] i_to=[%s] o_from=[%s] o_to=[%s] enable=[%s] proto=[%s]",__FUNCTION__,type,temp,i_from,i_to,o_from,o_to,enable,proto);
#endif

			if (!strcmp(type, "name")){
				if(strcmp(name, "")){
					filter_name(name, new_name, sizeof(new_name), GET);
					return websWrite(wp,"%s",new_name);
				}
			}
			else if (!strcmp(type, "enable")){
				if(!strcmp(enable, "on"))
					return websWrite(wp,"checked");
				else
					return websWrite(wp,"");
			}
			else if (!strcmp(type, "sel_tcp")){		// use select
				if(!strcmp(proto, "udp"))
					return websWrite(wp,"");
				else
					return websWrite(wp,"selected");
			}
			else if (!strcmp(type, "sel_udp")){		//use select
				if(!strcmp(proto, "tcp"))
					return websWrite(wp,"");
				else
					return websWrite(wp,"selected");
			}
			else if (!strcmp(type, "sel_both")){		//use select
				if(!strcmp(proto, "both"))
					return websWrite(wp,"selected");
				else
					return websWrite(wp,"");;
			}
			else if (!strcmp(type, "i_from"))
				return websWrite(wp,"%s",i_from);
			else if (!strcmp(type, "i_to"))
				return websWrite(wp,"%s",i_to);
			else if (!strcmp(type, "o_from"))
				return websWrite(wp,"%s",o_from);
			else if (!strcmp(type, "o_to"))
				return websWrite(wp,"%s",o_to);
		}
	}
 	if(!strcmp(type, "name"))
		return websWrite(wp,"");
	else
		return websWrite(wp,"0");
}
#endif


#ifdef UPNP_FORWARD_SUPPORT
/* Example:
 * name:[on|off]:[tcp|udp]:8000:80>100
 */
int
ej_forward_upnp(int eid, webs_t wp, int argc, char_t **argv)
{
	char name[] = "forward_portXXXXXXXXXX", value[1000];
        char *wan_port0=NULL, *wan_port1=NULL, *lan_ipaddr=NULL, *lan_port0=NULL, *lan_port1=NULL, *proto=NULL;
        char *enable=NULL, *desc=NULL;
			
	char *type;
	int which;
	
        if (ejArgs(argc, argv, "%s %d", &type, &which) < 2) {
             websError(wp, 400, "Insufficient args\n");
             return -1;
        }

	snprintf(name, sizeof(name), "forward_port%d", which);
	if (!nvram_invmatch(name, ""))
		goto def;
	strncpy(value, nvram_get(name), sizeof(value));

	/* Check for LAN IP address specification */
	lan_ipaddr = value;
	wan_port0 = strsep(&lan_ipaddr, ">");
	if (!lan_ipaddr)
		return FALSE;

	/* Check for LAN destination port specification */
	lan_port0 = lan_ipaddr;
	lan_ipaddr = strsep(&lan_port0, ":");
	if (!lan_port0)
		return FALSE;

	/* Check for protocol specification */
	proto = lan_port0;
	lan_port0 = strsep(&proto, ":,");
	if (!proto)
		return FALSE;

	/* Check for enable specification */
	enable = proto;
	proto = strsep(&enable, ":,");
	if (!enable)
		return FALSE;

	/* Check for description specification (optional) */
	desc = enable;
	enable = strsep(&desc, ":,");

	/* Check for WAN destination port range (optional) */
	wan_port1 = wan_port0;
	wan_port0 = strsep(&wan_port1, "-");
	if (!wan_port1)
		wan_port1 = wan_port0;

	/* Check for LAN destination port range (optional) */
        lan_port1 = lan_port0;
        lan_port0 = strsep(&lan_port1, "-");
        if (!lan_port1)
                lan_port1 = lan_port0;
				

#ifdef MY_DEBUG
	LOG(LOG_DEBUG, "%s(%d,%s): name=[%s] from=[%s][%s] to=[%s][%s] proto=[%s] ip=[%s] enable=[%s]\n",
		 __FUNCTION__, which, type,
		 desc, lan_port0, lan_port1, wan_port0, wan_port1, 
		 proto, lan_ipaddr, enable);
#endif
	
	if (!strcmp(type, "name"))
		return  websWrite(wp,"%s", desc);
	else if (!strcmp(type, "from"))
		return  websWrite(wp,"%s", lan_port0);
	else if (!strcmp(type, "to"))
		return  websWrite(wp,"%s", wan_port0);
	else if (!strcmp(type, "tcp")){
		if(!strcmp(proto, "tcp"))
			return  websWrite(wp, "checked");				
		return  websWrite(wp, " ");

	}
	else if (!strcmp(type, "udp")){
		if(!strcmp(proto, "udp"))
			return  websWrite(wp, "checked");				
		return  websWrite(wp, " ");

	}
	else if (!strcmp(type, "ip"))
		return  websWrite(wp, "%d", get_single_ip(lan_ipaddr, 3));
	else if (!strcmp(type, "enable"))
		if(!strcmp(enable, "on"))
			return  websWrite(wp, "checked");
		return  websWrite(wp, " ");

def:
 	if( !strcmp(type, "from") || !strcmp(type, "to") || !strcmp(type, "ip") )
		return  websWrite(wp,"0");
	else  if(!strcmp(type, "udp"))
		return websWrite(wp,"checked");
	else
		return 1;

}
#endif

#ifdef ALG_FORWARD_SUPPORT
/* Example:
 * name:[on|off]:[tcp|udp]:8000:80>100
 */
int
ej_forward_alg(int eid, webs_t wp, int argc, char_t **argv)
{
	char name[] = "forward_algXXXXXXXXXX", value[1000];
        char *wan_port0=NULL, *wan_port1=NULL, *lan_ipaddr=NULL, *lan_port0=NULL, *lan_port1=NULL, *proto=NULL;
        char *enable=NULL, *desc=NULL, *policy=NULL;
			
	char *type;
	int which;
	
        if (ejArgs(argc, argv, "%s %d", &type, &which) < 2) {
             websError(wp, 400, "Insufficient args\n");
             return -1;
        }

	snprintf(name, sizeof(name), "forward_alg%d", which);
	if (!nvram_invmatch(name, ""))
		goto def;
	strncpy(value, nvram_get(name), sizeof(value));

	/* Check for LAN IP address specification */
	lan_ipaddr = value;
	wan_port0 = strsep(&lan_ipaddr, ">");
	if (!lan_ipaddr)
		return FALSE;

	/* Check for LAN destination port specification */
	lan_port0 = lan_ipaddr;
	lan_ipaddr = strsep(&lan_port0, ":");
	if (!lan_port0)
		return FALSE;

	/* Check for protocol specification */
	proto = lan_port0;
	lan_port0 = strsep(&proto, ":,");
	if (!proto)
		return FALSE;

	/* Check for policy specification */
	policy = proto;
	proto = strsep(&policy, ":,");
	if (!policy)
		return FALSE;

	/* Check for enable specification */
	enable = policy;
	policy = strsep(&enable, ":,");
	if (!enable)
		return FALSE;

	/* Check for description specification (optional) */
	desc = enable;
	enable = strsep(&desc, ":,");

	/* Check for WAN destination port range (optional) */
	wan_port1 = wan_port0;
	wan_port0 = strsep(&wan_port1, "-");
	if (!wan_port1)
		wan_port1 = wan_port0;

	/* Check for LAN destination port range (optional) */
        lan_port1 = lan_port0;
        lan_port0 = strsep(&lan_port1, "-");
        if (!lan_port1)
                lan_port1 = lan_port0;
				

#ifdef MY_DEBUG
	LOG(LOG_DEBUG, "%s(%d,%s): name=[%s] from=[%s][%s] to=[%s][%s] proto=[%s] ip=[%s] enable=[%s]\n",
		 __FUNCTION__, which, type,
		 desc, lan_port0, lan_port1, wan_port0, wan_port1, 
		 proto, lan_ipaddr, enable);
#endif
	
	if (!strcmp(type, "name"))
		return  websWrite(wp,"%s", desc);
	else if (!strcmp(type, "from"))
		return  websWrite(wp,"%s", lan_port0);
	else if (!strcmp(type, "to"))
		return  websWrite(wp,"%s", wan_port0);
	else if (!strcmp(type, "tcp")){
		if(!strcmp(proto, "tcp"))
			return  websWrite(wp, "checked");				
		return  websWrite(wp, " ");

	}
	else if (!strcmp(type, "udp")){
		if(!strcmp(proto, "udp"))
			return  websWrite(wp, "checked");				
		return  websWrite(wp, " ");

	}
	else if (!strcmp(type, "ip"))
		return  websWrite(wp, "%d", get_single_ip(lan_ipaddr, 3));
	else if (!strcmp(type, "enable")){
		if(!strcmp(enable, "on"))
			return  websWrite(wp, "checked");
		return  websWrite(wp, " ");
	}
	else if (!strcmp(type, "pol_accept")){
		if(!strcmp(policy, "accept"))
			return websWrite(wp,"selected");
		return websWrite(wp,"");
	}
	else if (!strcmp(type, "pol_deny")){
		if(!strcmp(policy, "deny"))
			return websWrite(wp,"selected");
		return websWrite(wp,"");
	}

def:
 	if( !strcmp(type, "from") || !strcmp(type, "to") || !strcmp(type, "ip") )
		return  websWrite(wp,"0");
	else  if(!strcmp(type, "udp"))
		return websWrite(wp,"checked");
	else
		return 1;

}
#endif

#ifdef SPECIAL_FORWARD_SUPPORT

/* Example:
 * name:[on|off]:[tcp|udp|both]:ext_port>ip:int_port
 */
int
ej_spec_forward_table(int eid, webs_t wp, int argc, char_t **argv)
{
	static char word[256];
	char *next, *wordlist, *type;
	char *name=NULL, *from=NULL, *to=NULL, *proto=NULL, *ip=NULL ,*enable=NULL;
	int temp, which;
	
#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s():",__FUNCTION__);
#endif
        if (ejArgs(argc, argv, "%s %d", &type, &which) < 2) {
             websError(wp, 400, "Insufficient args\n");
             return -1;
        }

	wordlist = nvram_safe_get("forward_spec");
	if(!wordlist)	goto def;
	temp = which;
	
	foreach(word, wordlist, next) {
		if (which-- == 0) {
			enable = word;
			name = strsep(&enable, ":");
		 	if (!name || !enable)
                                continue;
			proto = enable;
			enable = strsep(&proto, ":");
			if (!enable || !proto)
				continue;
			from = proto;
			proto = strsep(&from, ":");
			if (!proto || !from)
				continue;
			to = from;
			from = strsep(&to, ">");
			if (!to || !from)
				continue;
			ip = to;
			to = strsep(&ip, ":");
			if (!ip || !to)
				continue;
#ifdef MY_DEBUG
	LOG(LOG_DEBUG,"%s(%s,%d): name=[%s] from=[%s] to=[%s] proto=[%s] ip=[%s] enable=[%s]",__FUNCTION__,type,temp,name,from,to,proto,ip,enable);
#endif

			if (!strcmp(type, "name"))
				return  websWrite(wp,"%s",name);
			else if (!strcmp(type, "from"))
				return  websWrite(wp,"%s",from);
			else if (!strcmp(type, "to"))
				return  websWrite(wp,"%s",to);
			else if (!strcmp(type, "tcp")){
				if(!strcmp(proto,"tcp") || !strcmp(proto,"both"))
					return  websWrite(wp,"checked");				
				return  websWrite(wp," ");

			}
			else if (!strcmp(type, "udp")){
				if(!strcmp(proto,"udp") || !strcmp(proto,"both"))
					return  websWrite(wp,"checked");				
				return  websWrite(wp," ");

			}
			else if (!strcmp(type, "ip"))
				return  websWrite(wp,"%s",ip);
			else if (!strcmp(type, "enable"))
				if(!strcmp(enable,"on"))
					return  websWrite(wp,"checked");
				return  websWrite(wp," ");
		}
	}
def:
 	if( !strcmp(type, "from") || !strcmp(type, "to") || !strcmp(type, "ip") )
		return  websWrite(wp, "0");
	else
		return  1;
}
#endif