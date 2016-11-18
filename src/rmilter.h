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

#ifndef RMILTER_H
#define RMILTER_H

#include "config.h"
#include "util.h"
#include "cfg_file.h"

#ifdef WITH_DKIM
#include <dkim.h>
#endif

#ifndef ADDRLEN
#define ADDRLEN 324
#endif

#ifndef NS_MAXDNAME
#define NS_MAXDNAME 1025
#endif

#define STAGE_MAX 7

#define RCODE_REJECT    "554"
#define RCODE_TEMPFAIL  "451"
#define RCODE_LATER  	"452"
#define XCODE_REJECT    "5.7.1"
#define XCODE_TEMPFAIL  "4.7.1"


struct rmilter_rng_state {
	pthread_mutex_t mtx;
	uint64_t s[16];
	int p;
};

/* Structures and macros used */
struct rcpt {
	char r_addr[ADDRLEN + 1];
	int is_whitelisted;
	struct rcpt *prev, *next;
};

struct mlfi_priv {
	struct rmilter_inet_address priv_addr;
	char priv_ip[INET6_ADDRSTRLEN + 1];
	char priv_hostname[ADDRLEN + 1];
	char priv_helo[ADDRLEN + 1];
	char priv_from[ADDRLEN + 1];
	char priv_user[ADDRLEN + 1];
	char message_id[ADDRLEN + 1];
	struct rcpt *rcpts;
	char *priv_subject;
	int priv_rcptcount;
	struct {
		char *header_name;
		char *header_value;
	} priv_cur_header;
	struct {
		char *value;
		size_t len;
	} priv_cur_body;
	char mlfi_id[14];
	char queue_id[32];
	char mta_tag[ADDRLEN + 1];
	char reply_id[ADDRLEN + 33];

#ifdef HAVE_PATH_MAX
	char file[PATH_MAX];
#elif defined(HAVE_MAXPATHLEN)
	char file[MAXPATHLEN];
#else
#error "neither PATH_MAX nor MAXPATHLEN defined"
#endif
    FILE *fileh;
	int filed;
	struct timeval conn_tm;
	struct rule* matched_rules[STAGE_MAX];
	long eoh_pos;
	short int strict;
	/* Config serial */
	short int serial;
	short int has_return_path;
	short int complete_to_beanstalk;
	short int has_whitelisted;
	short int authenticated;
#ifdef WITH_DKIM
	DKIM *dkim;
	struct dkim_domain_entry *dkim_domain;
#endif
};

#define MLFIPRIV	((struct mlfi_priv *) smfi_getpriv(ctx))

#endif /* RMILTER_H */
/*
 * vi:ts=4
 */
