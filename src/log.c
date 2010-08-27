/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2010 Jonas Gehring
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *      file: log.c
 *      desc: Convenient functions for gathering revision logs
 */


#include <svn_path.h>
#include <svn_pools.h>
#include <svn_ra.h>

#include <apr_tables.h>

#include "main.h"
#include "list.h"
#include "session.h"

#include "log.h"


/*---------------------------------------------------------------------------*/
/* Local data structures                                                     */
/*---------------------------------------------------------------------------*/


/* A baton for log_receiver() */
typedef struct {
	log_revision_t	*log;
	session_t	*session;
	apr_pool_t	*pool;
} log_receiver_baton_t;


/* A baton for log_receiver_list() */
typedef struct {
	list_t		*list;
	session_t	*session;
	apr_pool_t	*pool;
} log_receiver_list_baton_t;


/*---------------------------------------------------------------------------*/
/* Static functions                                                          */
/*---------------------------------------------------------------------------*/


/* Callback for svn_ra_get_log() */
static svn_error_t *log_receiver(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	apr_hash_index_t *hi;
	log_receiver_baton_t *data = (log_receiver_baton_t *)baton;

	data->log->revision = revision;
	data->log->author = apr_pstrdup(data->pool, author);
	data->log->date = apr_pstrdup(data->pool, date);
	data->log->message = apr_pstrdup(data->pool, message);
	data->log->changed_paths = apr_hash_make(data->pool);

	/* Deep-copy the changed_paths hash */
	if (changed_paths == NULL) {
		DEBUG_MSG("changed_paths is NULL\n");
		return SVN_NO_ERROR;
	}
	for (hi = apr_hash_first(pool, changed_paths); hi; hi = apr_hash_next(hi)) {
		const char *key;
		svn_log_changed_path_t *svalue, *dvalue;
		apr_hash_this(hi, (const void **)(void *)&key, NULL, (void **)(void *)&svalue);

		/* Skip this entry? */
		if ((strlen(key) < 1) || strncmp(data->session->prefix, key+1, strlen(data->session->prefix))) {
			DEBUG_MSG("%c %s [skipped]\n", svalue->action, key);
			continue;
		}

		dvalue = apr_palloc(data->pool, sizeof(svn_log_changed_path_t));
		dvalue->action = svalue->action;
		if (svalue->copyfrom_path != NULL) {
			if (*svalue->copyfrom_path == '/') {
				dvalue->copyfrom_path = apr_pstrdup(data->pool, svalue->copyfrom_path + 1);
			} else {
				dvalue->copyfrom_path = apr_pstrdup(data->pool, svalue->copyfrom_path);
			}
		} else {
			dvalue->copyfrom_path = NULL;
		}
		dvalue->copyfrom_rev = svalue->copyfrom_rev;

		/* Strip the prefix (or the leading slash) from the path */
		if (*data->session->prefix != '\0') {
			key += strlen(data->session->prefix) + 2;
		} else {
			key += 1;
		}
		apr_hash_set(data->log->changed_paths, apr_pstrdup(data->pool, key), APR_HASH_KEY_STRING, dvalue);

		/* A little debugging */
		DEBUG_MSG("%c %s", dvalue->action, key);
		if (dvalue->copyfrom_path != NULL) {
			DEBUG_MSG(" (from %s@%ld)", dvalue->copyfrom_path, dvalue->copyfrom_rev);
		}
		DEBUG_MSG("\n");
	}

	return SVN_NO_ERROR;
}


/* Callback for svn_ra_get_log() */
static svn_error_t *log_receiver_list(void *baton, apr_hash_t *changed_paths, svn_revnum_t revision, const char *author, const char *date, const char *message, apr_pool_t *pool)
{
	log_receiver_list_baton_t *data = (log_receiver_list_baton_t *)baton;
	log_revision_t log;
	log_receiver_baton_t receiver_baton;

	receiver_baton.log = &log;
	receiver_baton.session = data->session;
	receiver_baton.pool = data->pool;
	log_receiver(&receiver_baton, changed_paths, revision, author, date, message, pool);
	list_append(data->list, &log);

	return SVN_NO_ERROR;
}


/*---------------------------------------------------------------------------*/
/* Global functions                                                          */
/*---------------------------------------------------------------------------*/


/* Determines the first and last revision of the session root */
char log_get_range(session_t *session, svn_revnum_t *start, svn_revnum_t *end, int verbosity)
{
	svn_error_t *err;
	apr_array_header_t *paths;
	apr_pool_t *subpool;
	list_t list;
	log_receiver_list_baton_t baton;

	/* We just need the root */
	subpool = svn_pool_create(session->pool);
	paths = apr_array_make(subpool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", subpool);

	list = list_create(sizeof(log_revision_t));
	baton.list = &list;
	baton.session = session;
	baton.pool = subpool;

	if (verbosity > 0) {
		fprintf(stderr, _("Determining start end end revision... "));
	}
	if ((err = svn_ra_get_log(session->ra, paths, *start, *end, 0, FALSE, TRUE, log_receiver_list, &baton, subpool))) {
		if (verbosity > 0) {
			fprintf(stderr, "\n");
		}
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		list_free(&list);
		svn_pool_destroy(subpool);
		return 1;
	}
	if (verbosity > 0) {
		fprintf(stderr, _("done\n"));
	}

	*start = ((log_revision_t *)list.elements)[0].revision;
	*end = ((log_revision_t *)list.elements)[list.size-1].revision;

	list_free(&list);
	svn_pool_destroy(subpool);
	return 0;
}


/* Fetches a single revision log */
char log_fetch_single(session_t *session, svn_revnum_t rev, svn_revnum_t end, log_revision_t *log, apr_pool_t *pool)
{
	svn_error_t *err;
	apr_array_header_t *paths;
	apr_pool_t *subpool;
	log_receiver_baton_t baton;

	/* We just need the root */
	subpool = svn_pool_create(pool);
	paths = apr_array_make(subpool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", subpool);

	baton.log = log;
	baton.session = session;
	baton.pool = pool;

	if ((err = svn_ra_get_log(session->ra, paths, rev, end, 1, TRUE, FALSE, log_receiver, &baton, subpool))) {
		utils_handle_error(err, stderr, FALSE, "ERROR: ");
		svn_error_clear(err);
		svn_pool_destroy(subpool);
		return 1;
	}

	svn_pool_destroy(subpool);
	return 0;
}


/* Fetches all revision logs for a given revision range */
char log_fetch_all(session_t *session, svn_revnum_t start, svn_revnum_t end, list_t *list, int verbosity, int window_size)
{
	svn_error_t *err;
	apr_array_header_t *paths;
	apr_pool_t *pool;
	log_receiver_list_baton_t baton;
        svn_revnum_t i;

	/* We just need the root */
	pool = svn_pool_create(session->pool);
	paths = apr_array_make(pool, 1, sizeof (const char *));
	APR_ARRAY_PUSH(paths, const char *) = svn_path_canonicalize(".", pool);

	baton.list = list;
	baton.session = session;
	baton.pool = session->pool;

	if (window_size <= 0) {
		window_size = end - start + 1;
	}
	for (i = start; i <= end; i += window_size) {
		svn_revnum_t last = i + window_size - 1;
		if (last > end) {
			last = end;
		}

		if (verbosity > 0) {
			fprintf(stderr, _("Fetching logs %ld to %ld of %ld... "), i, last, end);
		}

		if ((err = svn_ra_get_log(session->ra, paths, i, last, 0, TRUE, FALSE, log_receiver_list, &baton, pool))) {
			if (verbosity > 0) {
				fprintf(stderr, "\n");
			}
			utils_handle_error(err, stderr, FALSE, "ERROR: ");
			list_free(list);
			svn_error_clear(err);
			svn_pool_destroy(pool);
			return 1;
		}

		if (verbosity > 0) {
			fprintf(stderr, _("done\n"));
		}
	}
	svn_pool_destroy(pool);
	return 0;
}
