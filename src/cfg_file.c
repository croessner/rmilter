/*
 * Copyright (c) 2007-2012, Vsevolod Stakhov
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. Redistributions in binary form
 * must reproduce the above copyright notice, this list of conditions and the
 * following disclaimer in the documentation and/or other materials provided with
 * the distribution. Neither the name of the author nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include "config.h"

#include "cfg_file.h"
#include "rmilter.h"

extern int yylineno;
extern char *yytext;
extern char* fnames_stack[];
extern int include_stack_ptr;

void parse_err(const char *fmt, ...)
{
	va_list aq;
	char logbuf[BUFSIZ], readbuf[32];
	int r;

	va_start (aq, fmt);
	rmilter_strlcpy (readbuf, yytext, sizeof(readbuf));

	if (include_stack_ptr > 0) {
		r = snprintf(logbuf, sizeof(logbuf), "config file <%s> parse error! line: %d, "
				"text: %s, reason: ", fnames_stack[include_stack_ptr - 1],
				yylineno, readbuf);
	}
	else {
		r = snprintf(logbuf, sizeof(logbuf), "config file parse error! line: %d, "
						"text: %s, reason: ",
						yylineno, readbuf);
	}
	r += vsnprintf(logbuf + r, sizeof(logbuf) - r, fmt, aq);

	va_end (aq);
	fprintf (stderr, "%s\n", logbuf);
	syslog (LOG_ERR, "%s", logbuf);
}

void parse_warn(const char *fmt, ...)
{
	va_list aq;
	char logbuf[BUFSIZ], readbuf[32];
	int r;

	va_start (aq, fmt);
	rmilter_strlcpy (readbuf, yytext, sizeof(readbuf));

	r = snprintf(logbuf, sizeof(logbuf),
			"config file parse warning! line: %d, text: %s, reason: ", yylineno,
			readbuf);
	r += vsnprintf(logbuf + r, sizeof(logbuf) - r, fmt, aq);

	va_end (aq);
	syslog (LOG_ERR, "%s", logbuf);
}

static size_t copy_regexp(char **dst, const char *src)
{
	size_t len;
	if (!src || *src == '\0')
		return 0;

	len = strlen (src);

	/* Skip slashes */
	if (*src == '/') {
		src++;
		len--;
	}
	if (src[len - 1] == '/') {
		len--;
	}

	*dst = malloc (len + 1);
	if (!*dst)
		return 0;

	return rmilter_strlcpy (*dst, src, len + 1);
}

int add_cache_server(struct config_file *cf, char *str, char *str2,
		int type)
{
	char *cur_tok, *err_str;
	struct cache_server *mc = NULL;
	uint16_t port;
	unsigned *pnum;

	if (str == NULL) {
		return 0;
	}

	switch (type) {
	case CACHE_SERVER_GREY:
		pnum = &cf->cache_servers_grey_num;
		mc = cf->cache_servers_grey;
		break;
	case CACHE_SERVER_WHITE:
		pnum = &cf->cache_servers_white_num;
		mc = cf->cache_servers_white;
		break;
	case CACHE_SERVER_LIMITS:
		pnum = &cf->cache_servers_limits_num;
		mc = cf->cache_servers_limits;
		break;
	case CACHE_SERVER_ID:
		pnum = &cf->cache_servers_id_num;
		mc = cf->cache_servers_id;
		break;
	case CACHE_SERVER_COPY:
		pnum = &cf->cache_servers_copy_num;
		mc = cf->cache_servers_copy;
		break;
	case CACHE_SERVER_SPAM:
		pnum = &cf->cache_servers_spam_num;
		mc = cf->cache_servers_spam;
		break;
	}

	if (*pnum >= MAX_CACHE_SERVERS) {
		yyerror ("yyparse: maximum number of cache servers is reached %d",
				MAX_CACHE_SERVERS);
		return 0;
	}

	mc += *pnum;
	cur_tok = strsep (&str, ":");

	if (cur_tok == NULL || *cur_tok == '\0') {
		return 0;
	}

	/* cur_tok - server name, str - server port */
	if (str == NULL) {
		port = DEFAULT_MEMCACHED_PORT;
	}
	else {
		port = strtoul (str, &err_str, 10);
		if (*err_str != '\0') {
			yyerror ("yyparse: bad memcached port: %s", str);
			return 0;
		}
	}

	mc->addr = strdup (cur_tok);
	mc->port = port;

	if (str2 != NULL) {
		msg_warn("mirrored servers are no longer supported; "
				"server %s will be ignored", str2);
	}

	(*pnum)++;

	return 1;
}

int add_clamav_server(struct config_file *cf, char *str)
{
	char *cur_tok, *err_str;
	struct clamav_server *srv;
	struct hostent *he;

	if (str == NULL)
		return 0;

	if (cf->clamav_servers_num == MAX_CLAMAV_SERVERS) {
		yyerror ("yyparse: maximum number of clamav servers is reached %d",
				MAX_CLAMAV_SERVERS);
	}

	srv = &cf->clamav_servers[cf->clamav_servers_num];

	if (srv == NULL)
		return 0;

	cur_tok = strsep (&str, ":");

	if (cur_tok == NULL || *cur_tok == '\0') {
		return 0;
	}

	srv->name = strdup (cur_tok);

	if (str != NULL) {
		/* We have also port */
		srv->port = strtoul (str, NULL, 10);
	}

	/* Try to parse priority */
	cur_tok = strsep (&str, ":");
	if (str != NULL && *str != '\0') {
		srv->up.priority = strtoul (str, NULL, 10);
	}

	cf->clamav_servers_num ++;

	return 1;
}

int add_spamd_server(struct config_file *cf, char *str, int is_extra)
{
	char *cur_tok, *err_str;
	struct spamd_server *srv;
	struct hostent *he;

	if (str == NULL)
		return 0;

	if (is_extra) {
		if (cf->extra_spamd_servers_num == MAX_SPAMD_SERVERS) {
			yyerror ("yyparse: maximum number of spamd servers is reached %d",
					MAX_SPAMD_SERVERS);
			return -1;
		}
	}
	else {
		if (cf->spamd_servers_num == MAX_SPAMD_SERVERS) {
			yyerror ("yyparse: maximum number of spamd servers is reached %d",
					MAX_SPAMD_SERVERS);
			return -1;
		}
	}

	if (is_extra) {
		srv = &cf->extra_spamd_servers[cf->extra_spamd_servers_num];
	}
	else {
		srv = &cf->spamd_servers[cf->spamd_servers_num];
	}

	if (*str == 'r' && *(str + 1) == ':') {
		srv->type = SPAMD_RSPAMD;
		str += 2;
	}
	else {
		srv->type = SPAMD_RSPAMD;
	}

	cur_tok = strsep (&str, ":");

	if (cur_tok == NULL || *cur_tok == '\0') {
		return 0;
	}

	srv->name = strdup (cur_tok);

	if (str != NULL) {
		/* We have also port */
		srv->port = strtoul (str, NULL, 10);
	}

	/* Try to parse priority */
	cur_tok = strsep (&str, ":");
	if (str != NULL && *str != '\0') {
		srv->up.priority = strtoul (str, NULL, 10);
	}

	if (is_extra) {
		cf->extra_spamd_servers_num++;
	}
	else {
		cf->spamd_servers_num++;
	}
	return 1;
}

int add_ip_radix (radix_compressed_t **tree, char *ipnet)
{
	if (!radix_add_generic_iplist (ipnet, tree, true)) {
		yyerror ("add_ip_radix: cannot insert ip to tree: %s",
				ipnet);
		return 0;
	}

	return 1;
}

#ifdef WITH_DKIM
static void add_hashed_header(const char *name, struct dkim_hash_entry **hash)
{
	struct dkim_hash_entry *new;

	new = malloc (sizeof(struct dkim_hash_entry));
	new->name = strdup (name);
	rmilter_str_lc (new->name, strlen (new->name));
	HASH_ADD_KEYPTR (hh, *hash, new->name, strlen (new->name), new);
}
#endif

void init_defaults(struct config_file *cfg)
{
	memset (cfg, 0, sizeof (*cfg));

	cfg->wlist_rcpt_global = NULL;
	cfg->wlist_rcpt_limit = NULL;
	cfg->clamav_connect_timeout = DEFAULT_CLAMAV_CONNECT_TIMEOUT;
	cfg->clamav_port_timeout = DEFAULT_CLAMAV_PORT_TIMEOUT;
	cfg->clamav_results_timeout = DEFAULT_CLAMAV_RESULTS_TIMEOUT;
	cfg->cache_connect_timeout = DEFAULT_MEMCACHED_CONNECT_TIMEOUT;
	cfg->spamd_connect_timeout = DEFAULT_SPAMD_CONNECT_TIMEOUT;
	cfg->spamd_results_timeout = DEFAULT_SPAMD_RESULTS_TIMEOUT;

	cfg->clamav_error_time = DEFAULT_UPSTREAM_ERROR_TIME;
	cfg->clamav_dead_time = DEFAULT_UPSTREAM_DEAD_TIME;
	cfg->clamav_maxerrors = DEFAULT_UPSTREAM_MAXERRORS;

	cfg->spamd_error_time = DEFAULT_UPSTREAM_ERROR_TIME;
	cfg->spamd_dead_time = DEFAULT_UPSTREAM_DEAD_TIME;
	cfg->spamd_maxerrors = DEFAULT_UPSTREAM_MAXERRORS;
	cfg->spamd_reject_message = strdup (DEFAUL_SPAMD_REJECT);
	cfg->rspamd_metric = strdup (DEFAULT_RSPAMD_METRIC);
	cfg->spam_header = strdup (DEFAULT_SPAM_HEADER);
	cfg->spam_header_value = strdup (DEFAULT_SPAM_HEADER_VALUE);
	cfg->spamd_retry_count = DEFAULT_SPAMD_RETRY_COUNT;
	cfg->spamd_retry_timeout = DEFAULT_SPAMD_RETRY_TIMEOUT;
	cfg->spamd_temp_fail = 0;
	cfg->spam_bar_char = strdup ("x");

	cfg->cache_error_time = DEFAULT_UPSTREAM_ERROR_TIME;
	cfg->cache_dead_time = DEFAULT_UPSTREAM_DEAD_TIME;
	cfg->cache_maxerrors = DEFAULT_UPSTREAM_MAXERRORS;

	cfg->grey_whitelist_tree = radix_create_compressed ();
	cfg->limit_whitelist_tree = radix_create_compressed ();
	cfg->spamd_whitelist = radix_create_compressed ();
	cfg->clamav_whitelist = radix_create_compressed ();
	cfg->dkim_ip_tree = radix_create_compressed ();
	cfg->our_networks = radix_create_compressed ();
	cfg->greylisted_message = strdup (DEFAULT_GREYLISTED_MESSAGE);
	/* Defaults for greylisting */
	/* 1d for greylisting data */
	cfg->greylisting_expire = 86400;
	/* 3d for whitelisting */
	cfg->whitelisting_expire = cfg->greylisting_expire * 3;
	cfg->greylisting_timeout = 300;
	cfg->white_prefix = strdup ("white");
	cfg->grey_prefix = strdup ("grey");
	cfg->id_prefix = strdup ("id");
	cfg->spamd_spam_add_header = 1;

	cfg->cache_copy_prob = 100.0;

	cfg->spamd_soft_fail = 1;
	cfg->spamd_greylist = 1;
	cfg->greylisting_enable = 1;
	cfg->ratelimit_enable = 1;

	cfg->dkim_auth_only = 1;
	cfg->dkim_enable = 1;
	cfg->pid_file = NULL;
	cfg->tempfiles_mode = 00600;

#if 0
	/* Init static defaults */
	white_from_abuse.addr = "abuse";
	white_from_abuse.len = sizeof ("abuse") - 1;
	white_from_postmaster.addr = "postmaster";
	white_from_postmaster.len = sizeof ("postmaster") - 1;
	LIST_INSERT_HEAD (&cfg->whitelist_static, &white_from_abuse, next);
	LIST_INSERT_HEAD (&cfg->whitelist_static, &white_from_postmaster, next);
#endif

#ifdef WITH_DKIM
	cfg->dkim_lib = dkim_init (NULL, NULL);
	/* Add recommended by rfc headers */
	add_hashed_header ("from", &cfg->headers);
	add_hashed_header ("sender", &cfg->headers);
	add_hashed_header ("reply-to", &cfg->headers);
	add_hashed_header ("subject", &cfg->headers);
	add_hashed_header ("date", &cfg->headers);
	add_hashed_header ("message-id", &cfg->headers);
	add_hashed_header ("to", &cfg->headers);
	add_hashed_header ("cc", &cfg->headers);
	add_hashed_header ("date", &cfg->headers);
	add_hashed_header ("mime-version", &cfg->headers);
	add_hashed_header ("content-type", &cfg->headers);
	add_hashed_header ("content-transfer-encoding", &cfg->headers);
	add_hashed_header ("resent-to", &cfg->headers);
	add_hashed_header ("resent-cc", &cfg->headers);
	add_hashed_header ("resent-from", &cfg->headers);
	add_hashed_header ("resent-sender", &cfg->headers);
	add_hashed_header ("resent-message-id", &cfg->headers);
	add_hashed_header ("in-reply-to", &cfg->headers);
	add_hashed_header ("references", &cfg->headers);
	add_hashed_header ("list-id", &cfg->headers);
	add_hashed_header ("list-owner", &cfg->headers);
	add_hashed_header ("list-unsubscribe", &cfg->headers);
	add_hashed_header ("list-subscribe", &cfg->headers);
	add_hashed_header ("list-post", &cfg->headers);
	/* TODO: make it configurable */
#endif
}

void free_config(struct config_file *cfg)
{
	unsigned int i;
	struct rule *cur, *tmp_rule;
	struct condition *cond, *tmp_cond;
	struct addr_list_entry *addr_cur, *addr_tmp;
	struct whitelisted_rcpt_entry *rcpt_cur, *rcpt_tmp;

	if (cfg->pid_file) {
		free (cfg->pid_file);
	}
	if (cfg->temp_dir) {
		free (cfg->temp_dir);
	}
	if (cfg->sock_cred) {
		free (cfg->sock_cred);
	}

	if (cfg->special_mid_re) {
		pcre_free (cfg->special_mid_re);
	}

	for (i = 0; i < cfg->clamav_servers_num; i++) {
		free (cfg->clamav_servers[i].name);
	}
	for (i = 0; i < cfg->spamd_servers_num; i++) {
		free (cfg->spamd_servers[i].name);
	}

	/* Free whitelists and bounce list*/
	HASH_ITER (hh, cfg->wlist_rcpt_global, rcpt_cur, rcpt_tmp) {
		HASH_DEL (cfg->wlist_rcpt_global, rcpt_cur);
		free (rcpt_cur->rcpt);
		free (rcpt_cur);
	}
	HASH_ITER (hh, cfg->wlist_rcpt_limit, rcpt_cur, rcpt_tmp) {
		HASH_DEL (cfg->wlist_rcpt_limit, rcpt_cur);
		free (rcpt_cur->rcpt);
		free (rcpt_cur);
	}
	HASH_ITER (hh, cfg->bounce_addrs, addr_cur, addr_tmp) {
		HASH_DEL (cfg->bounce_addrs, addr_cur);
		free (addr_cur->addr);
		free (addr_cur);
	}

	radix_destroy_compressed (cfg->grey_whitelist_tree);
	radix_destroy_compressed (cfg->spamd_whitelist);
	radix_destroy_compressed (cfg->clamav_whitelist);
	radix_destroy_compressed (cfg->limit_whitelist_tree);
	radix_destroy_compressed (cfg->dkim_ip_tree);
	radix_destroy_compressed (cfg->our_networks);

	if (cfg->spamd_reject_message) {
		free (cfg->spamd_reject_message);
	}
	if (cfg->rspamd_metric) {
		free (cfg->rspamd_metric);
	}
	if (cfg->spam_header) {
		free (cfg->spam_header);
	}
	if (cfg->spam_header_value) {
		free (cfg->spam_header_value);
	}
	if (cfg->id_prefix) {
		free (cfg->id_prefix);
	}
	if (cfg->grey_prefix) {
		free (cfg->grey_prefix);
	}
	if (cfg->white_prefix) {
		free (cfg->white_prefix);
	}
	if (cfg->cache_password) {
		free (cfg->cache_password);
	}
	if (cfg->cache_dbname) {
		free (cfg->cache_dbname);
	}
	if (cfg->cache_copy_channel) {
		free (cfg->cache_copy_channel);
	}
	if (cfg->cache_spam_channel) {
		free (cfg->cache_spam_channel);
	}
	if (cfg->greylisted_message) {
		free (cfg->greylisted_message);
	}
	if (cfg->spam_bar_char) {
		free (cfg->spam_bar_char);
	}

#ifdef WITH_DKIM
	struct dkim_hash_entry *curh, *tmph;
	struct dkim_domain_entry *curd, *tmpd;

	if (cfg->dkim_lib) {
		dkim_close (cfg->dkim_lib);
	}
	HASH_ITER (hh, cfg->headers, curh, tmph) {
		HASH_DEL (cfg->headers, curh); /* delete; users advances to next */
		free (curh->name);
		free (curh);
	}
	HASH_ITER (hh, cfg->dkim_domains, curd, tmpd) {
		HASH_DEL (cfg->dkim_domains, curd); /* delete; users advances to next */
		if (curd->key != MAP_FAILED && curd->key != NULL) {
			munmap (curd->key, curd->keylen);
		}
		if (curd->domain) {
			free (curd->domain);
		}
		if (curd->selector) {
			free (curd->selector);
		}
		if (curd->keyfile) {
			free (curd->keyfile);
		}
		free (curd);
	}
#endif
}
void
add_rcpt_whitelist (struct config_file *cfg, const char *rcpt,
		int is_global)
{
	struct whitelisted_rcpt_entry *t;
	t = (struct whitelisted_rcpt_entry *) malloc (
			sizeof(struct whitelisted_rcpt_entry));
	if (*rcpt == '@') {
		t->type = WLIST_RCPT_DOMAIN;
		rcpt++;
	}
	else if (strchr (rcpt, '@') != NULL) {
		t->type = WLIST_RCPT_USERDOMAIN;
	}
	else {
		t->type = WLIST_RCPT_USER;
	}
	t->rcpt = strdup (rcpt);
	t->len = strlen (t->rcpt);
	if (is_global) {
		HASH_ADD_KEYPTR(hh, cfg->wlist_rcpt_global, t->rcpt, t->len, t);
	}
	else {
		HASH_ADD_KEYPTR(hh, cfg->wlist_rcpt_limit, t->rcpt, t->len, t);
	}
}

void
clear_rcpt_whitelist (struct config_file *cfg, bool is_global)
{
	struct whitelisted_rcpt_entry *t, *tmp;

	if (is_global) {
		HASH_ITER (hh, cfg->wlist_rcpt_global, t, tmp) {
			HASH_DEL (cfg->wlist_rcpt_global, t);
			free (t->rcpt);
			free (t);
		}
	}
	else {
		HASH_ITER (hh, cfg->wlist_rcpt_limit, t, tmp) {
			HASH_DEL (cfg->wlist_rcpt_limit, t);
			free (t->rcpt);
			free (t);
		}
	}
}

int
is_whitelisted_rcpt (struct config_file *cfg, const char *str, int is_global)
{
	int len;
	struct whitelisted_rcpt_entry *entry, *list;
	char rcptbuf[ADDRLEN + 1], *domain;

	if (*str == '<') {
		str++;
	}

	len = strcspn (str, ">");
	rmilter_strlcpy (rcptbuf, str, MIN(len + 1, sizeof(rcptbuf)));
	rmilter_str_lc (rcptbuf, strlen (rcptbuf));

	if (len > 0) {
		if (is_global) {
			list = cfg->wlist_rcpt_global;
		}
		else {
			list = cfg->wlist_rcpt_limit;
		}
		/* Initially search for userdomain */
		HASH_FIND_STR(list, rcptbuf, entry);
		if (entry != NULL && entry->type == WLIST_RCPT_USERDOMAIN) {
			return 1;
		}

		domain = strchr (rcptbuf, '@');
		if (domain == NULL && entry != NULL && entry->type == WLIST_RCPT_USER) {
			return 1;
		}

		/* Search for user */
		if (domain != NULL) {
			*domain = '\0';
		}

		HASH_FIND_STR(list, rcptbuf, entry);
		if (entry != NULL && entry->type == WLIST_RCPT_USER) {
			return 1;
		}
		if (domain != NULL) {
			/* Search for domain */
			domain++;
			HASH_FIND_STR(list, domain, entry);
			if (entry != NULL && entry->type == WLIST_RCPT_DOMAIN) {
				return 1;
			}
		}
	}

	return 0;
}

char *
trim_quotes(char *in)
{
	char *res = in;
	size_t len;

	assert(in != NULL);

	len = strlen (in);

	if (*in == '"') {
		res = strdup (in + 1);
		len = strlen (res);
		free (in);
	}

	if (len > 1 && res[len - 1] == '"') {
		res[len - 1] = '\0';
	}

	return res;
}

/*
 * vi:ts=4
 */
