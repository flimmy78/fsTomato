
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
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>

#include <broadcom.h>

struct ddns_message {
	char *name;
	char *desc;
};


#if LANGUAGE == JAPANESE
struct ddns_message ddns_messages[] = {
	// Below is DynDNS error code
        {"dyn_good",	"DDNS �̍X�V�͊������܂���"},
        {"dyn_noupdate","DDNS �̍X�V�͊������܂���"},
	{"dyn_nohost",	"�z�X�g�������݂��܂���"},
	{"dyn_notfqdn",	"�h���C����������������܂���"},
	{"dyn_!yours",	"���[�U�[��������������܂���"},
	{"dyn_abuse",	"�z�X�g���� DDNS �T�[�o�ɂ��u���b�N����Ă��܂�"},
	{"dyn_nochg",	"DDNS �̍X�V�͊������܂���"},
	{"dyn_badauth",	"�F�؂Ɏ��s���܂��� (���[�U�[���܂��̓p�X���[�h)"},
	{"dyn_badsys",	"�V�X�e�� �p�����[�^���s���ł�"},
	{"dyn_badagent","���̃��[�U�[ �G�[�W�F���g�̓u���b�N����Ă��܂�"},
	{"dyn_numhost",	"�z�X�g���������邩���Ȃ����܂�"},
	{"dyn_dnserr",	"DNS �G���[����"},
	{"dyn_911",	"�\�����ʃG���[�ł��B (1)"},
	{"dyn_999",	"�\�����ʃG���[�ł��B (2)"},
	{"dyn_!donator","���N�G�X�g���ꂽ�@�\�͊�t�����ꍇ�ɂ̂ݗL���ł��B��t�����Ă��������B"},
	{"dyn_strange",	"�������s���ł��B�ڑ���T�[�o�����������ǂ����A�m�F���������B"},
	{"dyn_uncode",	"DynDns ����̕s���ȃ��^�[�� �R�[�h"},

	// Below is TZO error code
        {"tzo_good",	"Operation Complete"},
        {"tzo_noupdate","Operation Complete"},
	{"tzo_notfqdn",	"Invalid Domain Name"},
        {"tzo_notmail",	"Invalis Email"},
        {"tzo_notact",	"Invalid Action"},
        {"tzo_notkey",	"Invalid Key"},
        {"tzo_notip",	"Invalid IP address"},
        {"tzo_dupfqdn",	"Duplicate Domain Name"},
        {"tzo_fqdncre",	"Domain Name has already been created for this domain name"},
        {"tzo_expired",	"The account has expired"},
        {"tzo_error",	"An unexpected server error"},

	// Below is for all
	{"all_closed",	"DDNS �T�[�o�͌��݃N���[�Y���Ă��܂�"},
	{"all_resolving",	"�h���C������������"},
	{"all_errresolv",	"�h���C�����̉����Ɏ��s���܂����B"},
	{"all_connecting",	"�T�[�o�֐ڑ���"},
	{"all_connectfail",	"�T�[�o�ւ̐ڑ��Ɏ��s���܂����B"},
	{"all_disabled",	"DDNS �͖����ł�"},
	{"all_noip",		"�C���^�[�l�b�g�ڑ�������܂���"},
};
#else
struct ddns_message ddns_messages[] = {
	// Below is DynDNS error code
        {"dyn_good",	"DDNS is updated successfully"},
        {"dyn_noupdate","DDNS is updated successfully"},
	{"dyn_nohost",	"The hostname does not exist"},
	{"dyn_notfqdn",	"Domain Name is not correct"},
	{"dyn_!yours",	"Username is not correct"},
	{"dyn_abuse",	"The hostname is blocked by the DDNS server"},
	{"dyn_nochg",	"DDNS is updated successfully"},
	{"dyn_badauth",	"Authorization fails (username or passwords)"},
	{"dyn_badsys",	"The system parameters are invalid"},
	{"dyn_badagent","This useragent has been blocked"},
	{"dyn_numhost",	"Too many or too few hosts found"},
	{"dyn_dnserr",	"DNS error encountered"},
	{"dyn_911",	"An unexpected error (1)"},
	{"dyn_999",	"An unexpected error (2)"},
	{"dyn_!donator","A feature requested is only available to donators, please donate"},
	{"dyn_strange",	"Strange server response, are you connecting to the right server?"},
	{"dyn_uncode",	"Unknown return code"},

	// Below is TZO error code
        {"tzo_good",	"Operation Complete"},
	{"tzo_noupdate","Operation Complete"},
	{"tzo_notfqdn",	"Invalid Domain Name"},
        {"tzo_notmail",	"Invalis Email"},
        {"tzo_notact",	"Invalid Action"},
        {"tzo_notkey",	"Invalid Key"},
        {"tzo_notip",	"Invalid IP address"},
        {"tzo_dupfqdn",	"Duplicate Domain Name"},
        {"tzo_fqdncre",	"Domain Name has already been created for this domain name"},
        {"tzo_expired",	"The account has expired"},
        {"tzo_error",	"An unexpected server error"},

	// Below is for all
	{"all_closed",	"DDNS server is currently closed"},
	{"all_resolving",	"Resolving domain name"},
	{"all_errresolv",	"Domain name resolv fail"},
	{"all_connecting",	"Connecting to server"},
	{"all_connectfail",	"Connect to server fail"},
	{"all_disabled",	"DDNS function is disabled"},
	{"all_noip",	"No Internet connection"},
};
#endif

char *
convert(char *msg)
{
	static char buf[200];
//dyn_!yours       dyn_!donator
	cprintf("931021 Amin ddns.c for multilang support\r\n");
	if( !strcmp(msg, "dyn_!yours"))
		snprintf(buf, sizeof(buf), "ddnsm.dyn_yours");
	else if( !strcmp(msg, "dyn_!donator"))
		snprintf(buf, sizeof(buf), "ddnsm.dyn_donator");
	else
		snprintf(buf, sizeof(buf), "ddnsm.%s", msg);
	return buf;
}

int
ej_show_ddns_status(int eid, webs_t wp, int argc, char_t **argv)
{
	char string[80]="";
	int ret = 0;
	char *enable = websGetVar(wp, "ddns_enable", NULL);
	
	if(!enable)
		enable = nvram_safe_get("ddns_enable");		// for first time

	if(strcmp(nvram_safe_get("ddns_enable"),enable))	// change service
		return websWrite(wp," ");

	if(nvram_match("ddns_enable","0")) // only for no hidden page
		return websWrite(wp, "%s", convert("all_disabled"));

	if(!check_wan_link(0))
		return websWrite(wp, "%s", convert("all_noip"));

	if(file_to_buf("/tmp/ddns_msg", string, sizeof(string))){
		if(!strcmp(string, "")){
			if(nvram_match("ddns_status","1")){
				if(nvram_match("ddns_enable","1"))
					ret = websWrite(wp, "%s", convert("dyn_good"));	// dyndns
				else
					ret = websWrite(wp, "%s", convert("tzo_good"));	// TZO
			}
			else
				ret = websWrite(wp, "%s", convert("all_closed"));
		}
		else
			ret = websWrite(wp, "%s", convert(string));
	}

	return ret;
}

int
ddns_save_value(webs_t wp)
{
	char *enable, *username, *passwd, *hostname;
	char *mx=NULL, *backmx=NULL, *wildcard=NULL, *service=NULL;     // Only for DynDNS
	struct variable ddns_variables[] = {
		{ longname: "DDNS enable", argv: ARGV("0","1","2") },
		{ longname: "DDNS password", argv: ARGV("30") },
		{ longname: "DDNS hostname", argv: ARGV("128") },
		{ longname: "DDNS MX", argv: ARGV("63") },	// Only for DynDNS
		{ longname: "DDNS BACKMX", argv: ARGV("YES","NO") },	// Only for DynDNS
		{ longname: "DDNS wildcard", argv: ARGV("ON","OFF") },	// Only for DynDNS
		{ longname: "DDNS service", argv: ARGV("dyndns","dyndns-static","dyndns-custom") },	// Only for DynDNS
	}, *which;

	int ret=-1;
	char _username[] = "ddns_username_X";
	char _passwd[] = "ddns_passwd_X";
	char _hostname[] = "ddns_hostname_X";
			
	which = &ddns_variables[0];

	enable = websGetVar(wp, "ddns_enable", NULL);
	if(!enable && !valid_choice(wp, enable, &which[0])){
		error_value = 1;
		return 1;
	}

	if(atoi(enable) == 0)	{	// Disable
		nvram_set("ddns_enable_buf", nvram_safe_get("ddns_enable"));
		nvram_set("ddns_enable", enable);
		return 1;
	}
	else if(atoi(enable) == 1){	// dyndns
		snprintf(_username, sizeof(_username),"%s","ddns_username");
		snprintf(_passwd, sizeof(_passwd),"%s","ddns_passwd");
		snprintf(_hostname, sizeof(_hostname),"%s","ddns_hostname");
		mx = websGetVar(wp, "ddns_mx", NULL);
		backmx = websGetVar(wp, "ddns_backmx", NULL);
		wildcard = websGetVar(wp, "ddns_wildcard", NULL);
		service = websGetVar(wp, "ddns_service", NULL);
	}
	else {	// tzo
		snprintf(_username, sizeof(_username),"ddns_username_%s",enable);
		snprintf(_passwd, sizeof(_passwd),"ddns_passwd_%s",enable);
		snprintf(_hostname, sizeof(_hostname),"ddns_hostname_%s",enable);
	}

	username = websGetVar(wp, _username, NULL);
	passwd = websGetVar(wp, _passwd, NULL);
	hostname = websGetVar(wp, _hostname, NULL);

	if(!username || !passwd || !hostname){
		error_value = 1;
		return 1;
	}

	if(hostname && *hostname && !valid_name(wp, hostname, &which[2])) {
		error_value = 1;
		return 1;
	}

	if(mx && *mx && !valid_name(wp, mx, &which[3])) {
		error_value = 1;
		return 1;
	}

	if(backmx && !valid_choice(wp, backmx, &which[4])) {
		error_value = 1;
		return 1;
	}

	if(wildcard && !valid_choice(wp, wildcard, &which[5])) {
		error_value = 1;
		return 1;
	}

	if(service && !valid_choice(wp, service, &which[6])) {
		error_value = 1;
		return 1;
	}
	
	

	nvram_set("ddns_enable_buf", nvram_safe_get("ddns_enable"));
	nvram_set("ddns_enable", enable);
	nvram_set("ddns_username_buf", nvram_safe_get(_username));
	nvram_set("ddns_passwd_buf", nvram_safe_get(_passwd));
	nvram_set("ddns_hostname_buf", nvram_safe_get(_hostname));
	if (strcmp(passwd, TMP_PASSWD))
		nvram_set(_passwd, passwd);
	nvram_set(_username, username);

	if(hostname)	nvram_set(_hostname, hostname);

	if(mx)		nvram_set("ddns_mx", mx);
	if(backmx)	nvram_set("ddns_backmx", backmx);
	if(wildcard)	nvram_set("ddns_wildcard", wildcard);
	if(service)	nvram_set("ddns_service", service);

	return ret;
}

int
ddns_update_value(webs_t wp)
{
	return 1;
}

int
ej_show_ddns_ip(int eid, webs_t wp, int argc, char_t **argv)
{
	
	if(check_wan_link(0)) {
		char *ddns_enable = GOZILA_GET("ddns_enable");
		
		if(!strcmp(ddns_enable, "2")) {	// TZO
			if(!nvram_get("public_ip") || nvram_match("public_ip", ""))
				system("ddns_checkip -t tzo-echo -n public_ip");
			if(nvram_match("public_ip", ""))
				return websWrite(wp,"0.0.0.0");
			else
				return websWrite(wp,"%s",nvram_safe_get("public_ip"));
		}
		else {
			if(nvram_match("wan_proto","l2tp"))
				return websWrite(wp,"%s",nvram_safe_get("l2tp_get_ip"));
			else if(nvram_match("wan_proto","pptp"))
				return websWrite(wp,"%s",nvram_safe_get("pptp_get_ip"));
			else
				return websWrite(wp,"%s",nvram_safe_get("wan_ipaddr"));
		}
	}
	else
		return websWrite(wp,"0.0.0.0");

}

int
ej_show_ddns_setting(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;

        char *ddns_enable = GOZILA_GET("ddns_enable"); 

	if(!strcmp(ddns_enable, "1"))
		do_ej("dyndns.asp", wp);

	else if(!strcmp(ddns_enable, "2"))
		do_ej("tzo.asp", wp);

	return ret;
}
