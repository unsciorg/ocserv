/*
 * Copyright (C) 2013 Nikos Mavrogiannopoulos
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of ocserv.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#ifndef VPN_H
#define VPN_H

#include <config.h>
#include <gnutls/gnutls.h>
#include <http_parser.h>
#include <ccan/htable/htable.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <minmax.h>

#ifdef __GNUC__
# define _OCSERV_GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
# if _OCSERV_GCC_VERSION >= 30000
#  define _ATTR_PACKED __attribute__ ((__packed__))
# endif
#endif /* __GNUC__ */

#ifndef _ATTR_PACKED
# define _ATTR_PACKED
#endif

typedef enum {
	SOCK_TYPE_TCP,
	SOCK_TYPE_UDP,
	SOCK_TYPE_UNIX
} sock_type_t;

typedef enum {
	OC_COMP_NULL = 0,
	OC_COMP_LZ4,
	OC_COMP_LZS,
} comp_type_t;

#define MIN_NO_COMPRESS_LIMIT 64
#define DEFAULT_NO_COMPRESS_LIMIT 256

#define DEBUG_BASIC 1
#define DEBUG_HTTP  2
#define DEBUG_TRANSFERRED 5
#define DEBUG_TLS   9

#define DEFAULT_DPD_TIME 600

#define AC_PKT_DATA             0	/* Uncompressed data */
#define AC_PKT_DPD_OUT          3	/* Dead Peer Detection */
#define AC_PKT_DPD_RESP         4	/* DPD response */
#define AC_PKT_DISCONN          5	/* Client disconnection notice */
#define AC_PKT_KEEPALIVE        7	/* Keepalive */
#define AC_PKT_COMPRESSED       8	/* Compressed data */
#define AC_PKT_TERM_SERVER      9	/* Server kick */

#define REKEY_METHOD_SSL 1
#define REKEY_METHOD_NEW_TUNNEL 2

extern int syslog_open;

/* the first is generic, for the methods that require a username password */
#define AUTH_TYPE_USERNAME_PASS (1<<0)
#define AUTH_TYPE_PAM (1<<1 | AUTH_TYPE_USERNAME_PASS)
#define AUTH_TYPE_PLAIN (1<<2 | AUTH_TYPE_USERNAME_PASS)
#define AUTH_TYPE_CERTIFICATE (1<<3)
#define AUTH_TYPE_RADIUS (1<<5 | AUTH_TYPE_USERNAME_PASS)
#define AUTH_TYPE_GSSAPI (1<<6)

#define ERR_SUCCESS 0
#define ERR_BAD_COMMAND -2
#define ERR_AUTH_FAIL -3
#define ERR_AUTH_CONTINUE -4
#define ERR_WAIT_FOR_SCRIPT -5
#define ERR_MEM -6
#define ERR_READ_CONFIG -7
#define ERR_NO_IP -8
#define ERR_PARSING -9
#define ERR_EXEC -10
#define ERR_PEER_TERMINATED -11
#define ERR_CTL -12
#define ERR_NO_CMD_FD -13

#define ERR_WORKER_TERMINATED ERR_PEER_TERMINATED

#define LOG_HTTP_DEBUG 2048
#define LOG_TRANSFER_DEBUG 2049

/* Allow few seconds prior to cleaning up entries, to avoid any race
 * conditions when session control is enabled.
 */
#define AUTH_SLACK_TIME 10


#define MAX_CIPHERSUITE_NAME 64
#define SID_SIZE 16

typedef enum {
	AUTH_COOKIE_REP = 2,
	AUTH_COOKIE_REQ = 4,
	RESUME_STORE_REQ = 6,
	RESUME_DELETE_REQ = 7,
	RESUME_FETCH_REQ = 8,
	RESUME_FETCH_REP = 9,
	CMD_UDP_FD = 10,
	CMD_TUN_MTU = 11,
	CMD_TERMINATE = 12,
	CMD_SESSION_INFO = 13,
	CMD_CLI_STATS = 15,

	SM_CMD_AUTH_INIT = 120,
	SM_CMD_AUTH_CONT,
	SM_CMD_AUTH_REP,
	SM_CMD_DECRYPT,
	SM_CMD_SIGN,
	SM_CMD_AUTH_SESSION_OPEN,
	SM_CMD_AUTH_SESSION_CLOSE,
	SM_CMD_AUTH_SESSION_REPLY,
	SM_CMD_CLI_STATS,
} cmd_request_t;

#define MAX_IP_STR 46

struct group_cfg_st {
	/* routes to be forwarded to the client */
	char **routes;
	unsigned int routes_size;

	/* routes that are excluded */
	char **no_routes;
	unsigned int no_routes_size;

	/* routes to be applied to the server */
	char **iroutes;
	unsigned int iroutes_size;

	char **dns;
	unsigned int dns_size;

	char **nbns;
	unsigned int nbns_size;

	char *ipv4_network;
	char *ipv6_network;
	unsigned ipv6_prefix;
	char *ipv4_netmask;

	char *explicit_ipv4;
	char *explicit_ipv6;

	char *cgroup;

	char *xml_config_file;

	size_t rx_per_sec;
	size_t tx_per_sec;

	unsigned deny_roaming; /* whether the user is allowed to re-use cookies from another IP */
	unsigned net_priority;
	unsigned no_udp; /* whether to disable UDP for this user */
};

struct vpn_st {
	char name[IFNAMSIZ];
	char *ipv4_netmask;
	char *ipv4_network;
	char *ipv4;
	char *ipv4_local; /* local IPv4 address */
	char *ipv6_network;
	unsigned ipv6_prefix;

	char *ipv6;
	char *ipv6_local; /* local IPv6 address */
	unsigned int mtu;

	char **routes;
	unsigned int routes_size;

	/* excluded routes */
	char **no_routes;
	unsigned int no_routes_size;

	char **dns;
	unsigned int dns_size;

	char **nbns;
	unsigned int nbns_size;
};

#define MAX_AUTH_METHODS 4

typedef struct auth_struct_st {
	const char *name;
	char *additional;
	unsigned type;
	const struct auth_mod_st *amod;
	bool enabled;
} auth_struct_st;

struct cfg_st {
	char *name; /* server name */
	unsigned int port;
	unsigned int udp_port;
	unsigned int is_dyndns;
	char* unix_conn_file;
	unsigned int sup_config_type; /* one of SUP_CONFIG_ */
	unsigned int stats_report_time;

	char *pin_file;
	char *srk_pin_file;
	char **cert;
	unsigned cert_size;
	char **key;
	unsigned key_size;

	char *ca;
	char *crl;
	char *dh_params_file;
	char *cert_user_oid;	/* The OID that will be used to extract the username */
	char *cert_group_oid;	/* The OID that will be used to extract the groupname */

	auth_struct_st auth[MAX_AUTH_METHODS];
	unsigned auth_methods;

	gnutls_certificate_request_t cert_req;
	char *priorities;
	unsigned enable_compression;
	unsigned no_compress_limit;	/* under this size (in bytes) of data there will be no compression */
	char *chroot_dir;	/* where the xml files are served from */
	char *banner;
	char *ocsp_response; /* file with the OCSP response */
	char *default_domain; /* domain to be advertised */

	char **group_list; /* select_group */
	unsigned int group_list_size;

	char **friendly_group_list; /* the same size as group_list_size */

	char *default_select_group;

	char **custom_header;
	unsigned custom_header_size;;

	char **split_dns;
	unsigned split_dns_size;;

	char* socket_file_prefix;

	unsigned deny_roaming; /* whether a cookie is restricted to a single IP */
	time_t cookie_timeout;	/* in seconds */

	time_t rekey_time;	/* in seconds */
	unsigned rekey_method; /* REKEY_METHOD_ */

	time_t min_reauth_time;	/* after a failed auth, how soon one can reauthenticate -> in seconds */

	unsigned isolate; /* whether seccomp should be enabled or not */

	unsigned auth_timeout; /* timeout of HTTP auth */
	unsigned idle_timeout; /* timeout when idle */
	unsigned mobile_idle_timeout; /* timeout when a mobile is idle */
	unsigned keepalive;
	unsigned dpd;
	unsigned mobile_dpd;
	unsigned foreground;
	unsigned debug;
	unsigned max_clients;
	unsigned max_same_clients;
	unsigned use_utmp;
	unsigned use_dbus; /* whether the D-BUS service is registered */
	unsigned use_occtl; /* whether support for the occtl tool will be enabled */
	char* occtl_socket_file;

	unsigned try_mtu; /* MTU discovery enabled */
	unsigned cisco_client_compat; /* do not require client certificate, 
	                               * and allow auth to complete in different
	                               * TCP sessions. */
	unsigned rate_limit_ms; /* if non zero force a connection every rate_limit milliseconds */
	unsigned ping_leases; /* non zero if we need to ping prior to leasing */

	size_t rx_per_sec;
	size_t tx_per_sec;
	unsigned net_priority;

	unsigned output_buffer;
	unsigned default_mtu;
	unsigned predictable_ips; /* boolean */

	char *route_add_cmd;
	char *route_del_cmd;

	char *connect_script;
	char *disconnect_script;
	
	char *cgroup;
	char *proxy_url;

#ifdef ANYCONNECT_CLIENT_COMPAT
	char *xml_config_file;
	char *xml_config_hash;
	char *cert_hash;
#endif

	uid_t uid;
	gid_t gid;

	/* additional configuration files */
	char *per_group_dir;
	char *per_user_dir;
	char *default_group_conf;
	char *default_user_conf;
	
	/* the tun network */
	struct vpn_st network;
};

/* generic thing to stop complaints */
struct worker_st;
struct main_server_st;

#define MAX_BANNER_SIZE 256
#define MAX_USERNAME_SIZE 64
#define MAX_AGENT_NAME 48
#define MAX_PASSWORD_SIZE 64
#define TLS_MASTER_SIZE 48
#define MAX_HOSTNAME_SIZE MAX_USERNAME_SIZE
#define MAX_GROUPNAME_SIZE MAX_USERNAME_SIZE
#define MAX_SESSION_DATA_SIZE (4*1024)

#define MAX_CONFIG_ENTRIES 64

#include <tun.h>

unsigned extract_prefix(char *network);
char *human_addr2(const struct sockaddr *sa, socklen_t salen,
		       void *buf, size_t buflen, unsigned full);

#define human_addr(x, y, z, w) human_addr2(x, y, z, w, 1)

/* Helper casts */
#define SA_IN_P(p) (&((struct sockaddr_in *)(p))->sin_addr)
#define SA_IN_U8_P(p) ((uint8_t*)(&((struct sockaddr_in *)(p))->sin_addr))
#define SA_IN6_P(p) (&((struct sockaddr_in6 *)(p))->sin6_addr)
#define SA_IN6_U8_P(p) ((uint8_t*)(&((struct sockaddr_in6 *)(p))->sin6_addr))

#define SA_IN_PORT(p) (((struct sockaddr_in *)(p))->sin_port)
#define SA_IN6_PORT(p) (((struct sockaddr_in6 *)(p))->sin6_port)

#define SA_IN_P_GENERIC(addr, size) ((size==sizeof(struct sockaddr_in))?SA_IN_U8_P(addr):SA_IN6_U8_P(addr))
#define SA_IN_P_TYPE(addr, type) ((type==AF_INET)?SA_IN_U8_P(addr):SA_IN6_U8_P(addr))
#define SA_IN_SIZE(size) ((size==sizeof(struct sockaddr_in))?sizeof(struct in_addr):sizeof(struct in6_addr))

/* macros */
#define TOS_PACK(x) (x<<4)
#define TOS_UNPACK(x) (x>>4)
#define IS_TOS(x) ((x&0x0f)==0)

/* Helper structures */
enum option_types { OPTION_NUMERIC, OPTION_STRING, OPTION_BOOLEAN, OPTION_MULTI_LINE };

#endif
