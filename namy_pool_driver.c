/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: George Schlossnagle <george@omniti.com>                      |
  |         Wez Furlong <wez@php.net>                                    |
  |         Johannes Schlueter <johannes@mysql.com>                      |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_main.h"
#include "php_ini.h"
#include "php_variables.h"
#include "pdo/php_pdo.h"
#include "pdo/php_pdo_driver.h"
#include "php_pdo_namy_pool.h"
#include "php_pdo_namy_pool_int.h"
#include "SAPI.h"
#include "sapi/apache2handler/php_apache.h"
#include "ext/standard/info.h"


#ifndef PDO_USE_MYSQLND
#include <mysqld_error.h>
#endif
#include "zend_exceptions.h"

/*----- imported from mod_namy_pool.h -----*/
#define NAMY_UNKNOWN_CONNECTION 0
#define NAMY_OK 1
MYSQL* namy_attach_pool_connection(request_rec *r, const char* connection_pool_name);
int    namy_detach_pool_connection(request_rec *r, MYSQL *mysql);
void   namy_close_pool_connection(server_rec *s);
int    namy_is_pooled_connection(request_rec *r, MYSQL *mysql);
/*----- imported from mod_namy_pool.h -----*/


#if PDO_USE_MYSQLND
#	define pdo_namy_pool_init(persistent) mysqlnd_init(persistent)
#else
#	define pdo_namy_pool_init(persistent) mysql_init(NULL)
#endif

#if !HAVE_MYSQL_SQLSTATE && !PDO_USE_MYSQLND
static const char *pdo_namy_pool_get_sqlstate(unsigned int my_errno) { /* {{{ */
	switch (my_errno) {
		/* import auto-generated case: code */
#include "php_pdo_namy_pool_sqlstate.h"
	default: return "HY000";
	}
}
/* }}} */
#endif

/* {{{ _pdo_namy_pool_error */
int _pdo_namy_pool_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, const char *file, int line TSRMLS_DC) /* {{{ */
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;
	pdo_error_type *pdo_err; 
	pdo_namy_pool_error_info *einfo;
	pdo_namy_pool_stmt *S = NULL;

	PDO_DBG_ENTER("_pdo_namy_pool_error");
	PDO_DBG_INF_FMT("file=%s line=%d", file, line);
	if (stmt) {
		S = (pdo_namy_pool_stmt*)stmt->driver_data;
		pdo_err = &stmt->error_code;
		einfo   = &S->einfo;
	} else {
		pdo_err = &dbh->error_code;
		einfo   = &H->einfo;
	}

#if HAVE_MYSQL_STMT_PREPARE || PDO_USE_MYSQLND
	if (S && S->stmt) {
		einfo->errcode = mysql_stmt_errno(S->stmt);
	}
	else
#endif
	{
		einfo->errcode = mysql_errno(H->server);
	}

	einfo->file = file;
	einfo->line = line;

	if (einfo->errmsg) {
		pefree(einfo->errmsg, dbh->is_persistent);
		einfo->errmsg = NULL;
	}

	if (einfo->errcode) {
		if (einfo->errcode == 2014) {
			einfo->errmsg = pestrdup(
				"Cannot execute queries while other unbuffered queries are active.  "
				"Consider using PDOStatement::fetchAll().  Alternatively, if your code "
				"is only ever going to run against mysql, you may enable query "
				"buffering by setting the PDO::MYSQL_ATTR_USE_BUFFERED_QUERY attribute.",
				dbh->is_persistent);
		} else if (einfo->errcode == 2057) {
			einfo->errmsg = pestrdup(
				"A stored procedure returning result sets of different size was called. "
				"This is not supported by libmysql",
				dbh->is_persistent);

		} else {
			einfo->errmsg = pestrdup(mysql_error(H->server), dbh->is_persistent);
		}
	} else { /* no error */
		strcpy(*pdo_err, PDO_ERR_NONE);
		PDO_DBG_RETURN(0);
	}

#if HAVE_MYSQL_SQLSTATE || PDO_USE_MYSQLND
# if HAVE_MYSQL_STMT_PREPARE || PDO_USE_MYSQLND
	if (S && S->stmt) {
		strcpy(*pdo_err, mysql_stmt_sqlstate(S->stmt));
	} else
# endif
	{
		strcpy(*pdo_err, mysql_sqlstate(H->server));
	}
#else
	strcpy(*pdo_err, pdo_namy_pool_get_sqlstate(einfo->errcode));
#endif

	if (!dbh->methods) {
		PDO_DBG_INF("Throwing exception");
		zend_throw_exception_ex(php_pdo_get_exception(), einfo->errcode TSRMLS_CC, "SQLSTATE[%s] [%d] %s",
				*pdo_err, einfo->errcode, einfo->errmsg);
	}

	PDO_DBG_RETURN(einfo->errcode);
}
/* }}} */

/* {{{ pdo_namy_pool_fetch_error_func */
static int pdo_namy_pool_fetch_error_func(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info TSRMLS_DC)
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;
	pdo_namy_pool_error_info *einfo = &H->einfo;

	PDO_DBG_ENTER("pdo_namy_pool_fetch_error_func");
	PDO_DBG_INF_FMT("dbh=%p stmt=%p", dbh, stmt);
	if (stmt) {
		pdo_namy_pool_stmt *S = (pdo_namy_pool_stmt*)stmt->driver_data;
		einfo = &S->einfo;
	} else {
		einfo = &H->einfo;
	}

	if (einfo->errcode) {
		add_next_index_long(info, einfo->errcode);
		add_next_index_string(info, einfo->errmsg, 1);
	}

	PDO_DBG_RETURN(1);
}
/* }}} */

/* {{{ namy_pool_handle_closer */
static int namy_pool_handle_closer(pdo_dbh_t *dbh TSRMLS_DC) /* {{{ */
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;
	
	PDO_DBG_ENTER("namy_pool_handle_closer");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
	if (H) {
		if (H->server) {
			php_struct *ctx = SG(server_context);
			if (ctx && namy_is_pooled_connection(ctx->r, H->server)==NAMY_OK)
			{
				namy_detach_pool_connection(ctx->r, H->server);
			}
			else
			{
				mysql_close(H->server);
			}
			H->server = NULL;
		}
		if (H->einfo.errmsg) {
			pefree(H->einfo.errmsg, dbh->is_persistent);
			H->einfo.errmsg = NULL;
		}
		pefree(H, dbh->is_persistent);
		dbh->driver_data = NULL;
	}
	PDO_DBG_RETURN(0);
}
/* }}} */

/* {{{ namy_pool_handle_preparer */
static int namy_pool_handle_preparer(pdo_dbh_t *dbh, const char *sql, long sql_len, pdo_stmt_t *stmt, zval *driver_options TSRMLS_DC)
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;
	pdo_namy_pool_stmt *S = ecalloc(1, sizeof(pdo_namy_pool_stmt));
#if HAVE_MYSQL_STMT_PREPARE || PDO_USE_MYSQLND
	char *nsql = NULL;
	int nsql_len = 0;
	int ret;
	int server_version;
#endif
	
	PDO_DBG_ENTER("namy_pool_handle_preparer");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
	PDO_DBG_INF_FMT("sql=%.*s", sql_len, sql);

	S->H = H;
	stmt->driver_data = S;
	stmt->methods = &namy_pool_stmt_methods;

	if (H->emulate_prepare) {
		goto end;
	}

#if HAVE_MYSQL_STMT_PREPARE || PDO_USE_MYSQLND
	server_version = mysql_get_server_version(H->server);
	if (server_version < 40100) {
		goto fallback;
	}
	stmt->supports_placeholders = PDO_PLACEHOLDER_POSITIONAL;
	ret = pdo_parse_params(stmt, (char*)sql, sql_len, &nsql, &nsql_len TSRMLS_CC);

	if (ret == 1) {
		/* query was rewritten */
		sql = nsql;
		sql_len = nsql_len;
	} else if (ret == -1) {
		/* failed to parse */
		strcpy(dbh->error_code, stmt->error_code);
		PDO_DBG_RETURN(0);
	}

	if (!(S->stmt = mysql_stmt_init(H->server))) {
		pdo_namy_pool_error(dbh);
		if (nsql) {
			efree(nsql);
		}
		PDO_DBG_RETURN(0);
	}
	
	if (mysql_stmt_prepare(S->stmt, sql, sql_len)) {
		/* TODO: might need to pull statement specific info here? */
		/* if the query isn't supported by the protocol, fallback to emulation */
		if (mysql_errno(H->server) == 1295) {
			if (nsql) {
				efree(nsql);
			}
			goto fallback;
		}
		pdo_namy_pool_error(dbh);
		if (nsql) {
			efree(nsql);
		}
		PDO_DBG_RETURN(0);
	}
	if (nsql) {
		efree(nsql);
	}

	S->num_params = mysql_stmt_param_count(S->stmt);

	if (S->num_params) {
		S->params_given = 0;
#if PDO_USE_MYSQLND
		S->params = NULL;
#else
		S->params = ecalloc(S->num_params, sizeof(MYSQL_BIND));
		S->in_null = ecalloc(S->num_params, sizeof(my_bool));
		S->in_length = ecalloc(S->num_params, sizeof(unsigned long));
#endif
	}
	dbh->alloc_own_columns = 1;

	S->max_length = pdo_attr_lval(driver_options, PDO_ATTR_MAX_COLUMN_LEN, 0 TSRMLS_CC);

	PDO_DBG_RETURN(1);

fallback:
#endif
end:
	stmt->supports_placeholders = PDO_PLACEHOLDER_NONE;
	
	PDO_DBG_RETURN(1);
}
/* }}} */

/* {{{ namy_pool_handle_doer */
static long namy_pool_handle_doer(pdo_dbh_t *dbh, const char *sql, long sql_len TSRMLS_DC)
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;
	PDO_DBG_ENTER("namy_pool_handle_doer");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
	PDO_DBG_INF_FMT("sql=%.*s", sql_len, sql);

	if (mysql_real_query(H->server, sql, sql_len)) {
		pdo_namy_pool_error(dbh);
		PDO_DBG_RETURN(-1);
	} else {
		my_ulonglong c = mysql_affected_rows(H->server);
		if (c == (my_ulonglong) -1) {
			pdo_namy_pool_error(dbh);
			PDO_DBG_RETURN(H->einfo.errcode ? -1 : 0);
		} else {

#if HAVE_MYSQL_NEXT_RESULT || PDO_USE_MYSQLND
			/* MULTI_QUERY support - eat up all unfetched result sets */
			MYSQL_RES* result;
			while (mysql_more_results(H->server)) {
				if (mysql_next_result(H->server)) {
					PDO_DBG_RETURN(1);
				}
				result = mysql_store_result(H->server);
				if (result) {
					mysql_free_result(result);
				}
			}
#endif
			PDO_DBG_RETURN((int)c);
		}
	}
}
/* }}} */

/* {{{ pdo_namy_pool_last_insert_id */
static char *pdo_namy_pool_last_insert_id(pdo_dbh_t *dbh, const char *name, unsigned int *len TSRMLS_DC)
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;
	char *id = php_pdo_int64_to_str(mysql_insert_id(H->server) TSRMLS_CC);
	PDO_DBG_ENTER("pdo_namy_pool_last_insert_id");
	*len = strlen(id);
	PDO_DBG_RETURN(id);
}

/* {{{ namy_pool_handle_quoter */
static int namy_pool_handle_quoter(pdo_dbh_t *dbh, const char *unquoted, int unquotedlen, char **quoted, int *quotedlen, enum pdo_param_type paramtype  TSRMLS_DC)
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;
	PDO_DBG_ENTER("namy_pool_handle_quoter");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
	PDO_DBG_INF_FMT("unquoted=%.*s", unquotedlen, unquoted);
	*quoted = safe_emalloc(2, unquotedlen, 3);
	*quotedlen = mysql_real_escape_string(H->server, *quoted + 1, unquoted, unquotedlen);
	(*quoted)[0] =(*quoted)[++*quotedlen] = '\'';
	(*quoted)[++*quotedlen] = '\0';
	PDO_DBG_INF_FMT("quoted=%.*s", *quotedlen, *quoted);
	PDO_DBG_RETURN(1);
}
/* }}} */

/* {{{ namy_pool_handle_begin */
static int namy_pool_handle_begin(pdo_dbh_t *dbh TSRMLS_DC)
{
	PDO_DBG_ENTER("namy_pool_handle_quoter");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
	PDO_DBG_RETURN(0 <= namy_pool_handle_doer(dbh, ZEND_STRL("START TRANSACTION") TSRMLS_CC));
}
/* }}} */

/* {{{ namy_pool_handle_commit */
static int namy_pool_handle_commit(pdo_dbh_t *dbh TSRMLS_DC)
{
	PDO_DBG_ENTER("namy_pool_handle_commit");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
#if MYSQL_VERSION_ID >= 40100 || defined(PDO_USE_MYSQLND)
	PDO_DBG_RETURN(0 <= mysql_commit(((pdo_namy_pool_db_handle *)dbh->driver_data)->server));
#else
	PDO_DBG_RETURN(0 <= namy_pool(dbh, ZEND_STRL("COMMIT") TSRMLS_CC));
#endif
}

/* {{{ namy_pool_handle_rollback */
static int namy_pool_handle_rollback(pdo_dbh_t *dbh TSRMLS_DC)
{
	PDO_DBG_ENTER("namy_pool_handle_rollback");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
#if MYSQL_VERSION_ID >= 40100 || defined(PDO_USE_MYSQLND)
	PDO_DBG_RETURN(0 <= mysql_rollback(((pdo_namy_pool_db_handle *)dbh->driver_data)->server));
#else
	PDO_DBG_RETURN(0 <= namy_pool(dbh, ZEND_STRL("ROLLBACK") TSRMLS_CC));
#endif
}
/* }}} */

/* {{{ namy_pool_handle_autocommit */
static inline int namy_pool_handle_autocommit(pdo_dbh_t *dbh TSRMLS_DC)
{
	PDO_DBG_ENTER("namy_pool_handle_autocommit");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
	PDO_DBG_INF_FMT("dbh->autocommit=%d", dbh->auto_commit);
#if MYSQL_VERSION_ID >= 40100 || defined(PDO_USE_MYSQLND)
	PDO_DBG_RETURN(0 <= mysql_autocommit(((pdo_namy_pool_db_handle *)dbh->driver_data)->server, dbh->auto_commit));
#else
	if (dbh->auto_commit) {
		PDO_DBG_RETURN(0 <= namy_pool(dbh, ZEND_STRL("SET AUTOCOMMIT=1") TSRMLS_CC));
	} else {
		PDO_DBG_RETURN(0 <= namy_pool(dbh, ZEND_STRL("SET AUTOCOMMIT=0") TSRMLS_CC));
	}
#endif
}
/* }}} */

/* {{{ pdo_namy_pool_set_attribute */
static int pdo_namy_pool_set_attribute(pdo_dbh_t *dbh, long attr, zval *val TSRMLS_DC)
{
	PDO_DBG_ENTER("pdo_namy_pool_set_attribute");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
	PDO_DBG_INF_FMT("attr=%l", attr);
	switch (attr) {
		case PDO_ATTR_AUTOCOMMIT:		
			convert_to_boolean(val);
	
			/* ignore if the new value equals the old one */			
			if (dbh->auto_commit ^ Z_BVAL_P(val)) {
				dbh->auto_commit = Z_BVAL_P(val);
				namy_pool_handle_autocommit(dbh TSRMLS_CC);
			}
			PDO_DBG_RETURN(1);

		case PDO_NAMY_POOL_ATTR_USE_BUFFERED_QUERY:
			((pdo_namy_pool_db_handle *)dbh->driver_data)->buffered = Z_BVAL_P(val);
			PDO_DBG_RETURN(1);
		case PDO_NAMY_POOL_ATTR_DIRECT_QUERY:
		case PDO_ATTR_EMULATE_PREPARES:
			((pdo_namy_pool_db_handle *)dbh->driver_data)->emulate_prepare = Z_BVAL_P(val);
			PDO_DBG_RETURN(1);
		case PDO_ATTR_FETCH_TABLE_NAMES:
			((pdo_namy_pool_db_handle *)dbh->driver_data)->fetch_table_names = Z_BVAL_P(val);
			PDO_DBG_RETURN(1);
#ifndef PDO_USE_MYSQLND
		case PDO_NAMY_POOL_ATTR_MAX_BUFFER_SIZE:
			if (Z_LVAL_P(val) < 0) {
				/* TODO: Johannes, can we throw a warning here? */
 				((pdo_namy_pool_db_handle *)dbh->driver_data)->max_buffer_size = 1024*1024;
				PDO_DBG_INF_FMT("Adjusting invalid buffer size to =%l", ((pdo_namy_pool_db_handle *)dbh->driver_data)->max_buffer_size);
			} else {
				((pdo_namy_pool_db_handle *)dbh->driver_data)->max_buffer_size = Z_LVAL_P(val);
			}
			PDO_DBG_RETURN(1);
			break;
#endif

		default:
			PDO_DBG_RETURN(0);
	}
}
/* }}} */

/* {{{ pdo_namy_pool_get_attribute */
static int pdo_namy_pool_get_attribute(pdo_dbh_t *dbh, long attr, zval *return_value TSRMLS_DC)
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;

	PDO_DBG_ENTER("pdo_namy_pool_get_attribute");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
	PDO_DBG_INF_FMT("attr=%l", attr);
	switch (attr) {
		case PDO_ATTR_CLIENT_VERSION:
			ZVAL_STRING(return_value, (char *)mysql_get_client_info(), 1);
			break;

		case PDO_ATTR_SERVER_VERSION:
			ZVAL_STRING(return_value, (char *)mysql_get_server_info(H->server), 1);
			break;

		case PDO_ATTR_CONNECTION_STATUS:
			ZVAL_STRING(return_value, (char *)mysql_get_host_info(H->server), 1);
			break;
		case PDO_ATTR_SERVER_INFO: {
			char *tmp;
#if PDO_USE_MYSQLND
			unsigned int tmp_len;

			if (mysqlnd_stat(H->server, &tmp, &tmp_len) == PASS) {
				ZVAL_STRINGL(return_value, tmp, tmp_len, 0);
#else
			if ((tmp = (char *)mysql_stat(H->server))) {
				ZVAL_STRING(return_value, tmp, 1);
#endif
			} else {
				pdo_namy_pool_error(dbh);
				PDO_DBG_RETURN(-1);
			}
		}
			break;
		case PDO_ATTR_AUTOCOMMIT:
			ZVAL_LONG(return_value, dbh->auto_commit);
			break;
			
		case PDO_NAMY_POOL_ATTR_USE_BUFFERED_QUERY:
			ZVAL_LONG(return_value, H->buffered);
			break;

		case PDO_NAMY_POOL_ATTR_DIRECT_QUERY:
			ZVAL_LONG(return_value, H->emulate_prepare);
			break;

#ifndef PDO_USE_MYSQLND
		case PDO_NAMY_POOL_ATTR_MAX_BUFFER_SIZE:
			ZVAL_LONG(return_value, H->max_buffer_size);
			break;
#endif

		default:
			PDO_DBG_RETURN(0);	
	}

	PDO_DBG_RETURN(1);
}
/* }}} */

/* {{{ pdo_namy_pool_check_liveness */
static int pdo_namy_pool_check_liveness(pdo_dbh_t *dbh TSRMLS_DC) /* {{{ */
{
	pdo_namy_pool_db_handle *H = (pdo_namy_pool_db_handle *)dbh->driver_data;
#if MYSQL_VERSION_ID <= 32230
	void (*handler) (int);
	unsigned int my_errno;
#endif

	PDO_DBG_ENTER("pdo_namy_pool_check_liveness");
	PDO_DBG_INF_FMT("dbh=%p", dbh);

#if MYSQL_VERSION_ID > 32230
	if (mysql_ping(H->server)) {
		PDO_DBG_RETURN(FAILURE);
	}
#else /* no mysql_ping() */
	handler = signal(SIGPIPE, SIG_IGN);
	mysql_stat(H->server);
	switch (mysql_errno(H->server)) {
		case CR_SERVER_GONE_ERROR:
		case CR_SERVER_LOST:
			signal(SIGPIPE, handler);
			PDO_DBG_RETURN(FAILURE);
		default:
			break;
	}
	signal(SIGPIPE, handler);
#endif /* end mysql_ping() */
	PDO_DBG_RETURN(SUCCESS);
} 
/* }}} */

/* {{{ namy_pool_methods */
static struct pdo_dbh_methods namy_pool_methods = {
	namy_pool_handle_closer,
	namy_pool_handle_preparer,
	namy_pool_handle_doer,
	namy_pool_handle_quoter,
	namy_pool_handle_begin,
	namy_pool_handle_commit,
	namy_pool_handle_rollback,
	pdo_namy_pool_set_attribute,
	pdo_namy_pool_last_insert_id,
	pdo_namy_pool_fetch_error_func,
	pdo_namy_pool_get_attribute,
	pdo_namy_pool_check_liveness
};
/* }}} */
#ifdef PDO_USE_MYSQLND
# ifdef PHP_WIN32
#  define MYSQL_UNIX_ADDR	"MySQL"
# else
#  define MYSQL_UNIX_ADDR	PDO_MYSQL_G(default_socket)
# endif
#endif

/* {{{ pdo_namy_pool_handle_factory */
static int pdo_namy_pool_handle_factory(pdo_dbh_t *dbh, zval *driver_options TSRMLS_DC) /* {{{ */
{
	pdo_namy_pool_db_handle *H;
	int i, ret = 0;
	char *host = NULL, *unix_socket = NULL;
	unsigned int port = 3306;
	char *dbname;
	struct pdo_data_src_parser vars[] = {
		{ "charset",  NULL,	0 },
		{ "dbname",   "",	0 },
		{ "host",   "localhost",	0 },
		{ "port",   "3306",	0 },
		{ "unix_socket",  MYSQL_UNIX_ADDR,	0 },
	};
	int connect_opts = 0
#ifdef CLIENT_MULTI_RESULTS
		|CLIENT_MULTI_RESULTS
#endif
#ifdef CLIENT_MULTI_STATEMENTS
		|CLIENT_MULTI_STATEMENTS
#endif
		;

#if PDO_USE_MYSQLND
	int dbname_len = 0;
	int password_len = 0;
#endif
	PDO_DBG_ENTER("pdo_namy_pool_handle_factory");
	PDO_DBG_INF_FMT("dbh=%p", dbh);
#ifdef CLIENT_MULTI_RESULTS
	PDO_DBG_INF("multi results");
#endif

	php_pdo_parse_data_source(dbh->data_source, dbh->data_source_len, vars, 5);

	H = pecalloc(1, sizeof(pdo_namy_pool_db_handle), dbh->is_persistent);

	H->einfo.errcode = 0;
	H->einfo.errmsg = NULL;

	/* allocate an environment */

	/* handle for the server */
	if (!(H->server = pdo_namy_pool_init(dbh->is_persistent))) {
		pdo_namy_pool_error(dbh);
		goto cleanup;
	}
	
	dbh->driver_data = H;

#ifndef PDO_USE_MYSQLND
	H->max_buffer_size = 1024*1024;
#endif

	H->buffered = H->emulate_prepare = 1;

	/* handle MySQL options */
	if (driver_options) {
		long connect_timeout = pdo_attr_lval(driver_options, PDO_ATTR_TIMEOUT, 30 TSRMLS_CC);
		long local_infile = pdo_attr_lval(driver_options, PDO_NAMY_POOL_ATTR_LOCAL_INFILE, 0 TSRMLS_CC);
		char *init_cmd = NULL;
#ifndef PDO_USE_MYSQLND
		char *default_file = NULL, *default_group = NULL;
		long compress = 0;
#endif
#if HAVE_MYSQL_STMT_PREPARE || PDO_USE_MYSLQND
		char *ssl_key = NULL, *ssl_cert = NULL, *ssl_ca = NULL, *ssl_capath = NULL, *ssl_cipher = NULL;
#endif
		H->buffered = pdo_attr_lval(driver_options, PDO_NAMY_POOL_ATTR_USE_BUFFERED_QUERY, 1 TSRMLS_CC);

		H->emulate_prepare = pdo_attr_lval(driver_options,
			PDO_NAMY_POOL_ATTR_DIRECT_QUERY, H->emulate_prepare TSRMLS_CC);
		H->emulate_prepare = pdo_attr_lval(driver_options, 
			PDO_ATTR_EMULATE_PREPARES, H->emulate_prepare TSRMLS_CC);

#ifndef PDO_USE_MYSQLND
		H->max_buffer_size = pdo_attr_lval(driver_options, PDO_NAMY_POOL_ATTR_MAX_BUFFER_SIZE, H->max_buffer_size TSRMLS_CC);
#endif

		if (pdo_attr_lval(driver_options, PDO_NAMY_POOL_ATTR_FOUND_ROWS, 0 TSRMLS_CC)) {
			connect_opts |= CLIENT_FOUND_ROWS;
		}

		if (pdo_attr_lval(driver_options, PDO_NAMY_POOL_ATTR_IGNORE_SPACE, 0 TSRMLS_CC)) {
			connect_opts |= CLIENT_IGNORE_SPACE;
		}

		if (mysql_options(H->server, MYSQL_OPT_CONNECT_TIMEOUT, (const char *)&connect_timeout)) {
			pdo_namy_pool_error(dbh);
			goto cleanup;
		}

#if PHP_API_VERSION < 20100412
		if ((PG(open_basedir) && PG(open_basedir)[0] != '\0') || PG(safe_mode))
#else
		if (PG(open_basedir) && PG(open_basedir)[0] != '\0') 
#endif
		{
			local_infile = 0;
		}
#ifdef MYSQL_OPT_LOCAL_INFILE
		if (mysql_options(H->server, MYSQL_OPT_LOCAL_INFILE, (const char *)&local_infile)) {
			pdo_namy_pool_error(dbh);
			goto cleanup;
		}
#endif
#ifdef MYSQL_OPT_RECONNECT
		/* since 5.0.3, the default for this option is 0 if not specified.
		 * we want the old behaviour */
		{
			long reconnect = 1;
			mysql_options(H->server, MYSQL_OPT_RECONNECT, (const char*)&reconnect);
		}
#endif
		init_cmd = pdo_attr_strval(driver_options, PDO_NAMY_POOL_ATTR_INIT_COMMAND, NULL TSRMLS_CC);
		if (init_cmd) {
			if (mysql_options(H->server, MYSQL_INIT_COMMAND, (const char *)init_cmd)) {
				efree(init_cmd);
				pdo_namy_poo_error(dbh);
				goto cleanup;
			}
			efree(init_cmd);
		}
#ifndef PDO_USE_MYSQLND		
		default_file = pdo_attr_strval(driver_options, PDO_NAMY_POOL_ATTR_READ_DEFAULT_FILE, NULL TSRMLS_CC);
		if (default_file) {
			if (mysql_options(H->server, MYSQL_READ_DEFAULT_FILE, (const char *)default_file)) {
				efree(default_file);
				pdo_namy_poo_error(dbh);
				goto cleanup;
			}
			efree(default_file);
		}
		
		default_group= pdo_attr_strval(driver_options, PDO_NAMY_POOL_ATTR_READ_DEFAULT_GROUP, NULL TSRMLS_CC);
		if (default_group) {
			if (mysql_options(H->server, MYSQL_READ_DEFAULT_GROUP, (const char *)default_group)) {
				efree(default_group);
				pdo_namy_poo_error(dbh);
				goto cleanup;
			}
			efree(default_group);
		}

		compress = pdo_attr_lval(driver_options, PDO_NAMY_POOL_ATTR_COMPRESS, 0 TSRMLS_CC);
		if (compress) {
			if (mysql_options(H->server, MYSQL_OPT_COMPRESS, 0)) {
				pdo_namy_poo_error(dbh);
				goto cleanup;
			}
		}
#endif
#if HAVE_MYSQL_STMT_PREPARE || PDO_USE_MYSLQND
		ssl_key = pdo_attr_strval(driver_options, PDO_NAMY_POOL_ATTR_SSL_KEY, NULL TSRMLS_CC);
		ssl_cert = pdo_attr_strval(driver_options, PDO_NAMY_POOL_ATTR_SSL_CERT, NULL TSRMLS_CC);
		ssl_ca = pdo_attr_strval(driver_options, PDO_NAMY_POOL_ATTR_SSL_CA, NULL TSRMLS_CC);
		ssl_capath = pdo_attr_strval(driver_options, PDO_NAMY_POOL_ATTR_SSL_CAPATH, NULL TSRMLS_CC);
		ssl_cipher = pdo_attr_strval(driver_options, PDO_NAMY_POOL_ATTR_SSL_CIPHER, NULL TSRMLS_CC);
		
		if (ssl_key || ssl_cert || ssl_ca || ssl_capath || ssl_cipher) {
			mysql_ssl_set(H->server, ssl_key, ssl_cert, ssl_ca, ssl_capath, ssl_cipher);
			if (ssl_key) {
				efree(ssl_key);
			}
			if (ssl_cert) {
				efree(ssl_cert);
			}
			if (ssl_ca) {
				efree(ssl_ca);
			}
			if (ssl_capath) {
				efree(ssl_capath);
			}
			if (ssl_cipher) {
				efree(ssl_cipher);
			}
		}
#endif
	}

#ifdef PDO_MYSQL_HAS_CHARSET
	if (vars[0].optval && mysql_options(H->server, MYSQL_SET_CHARSET_NAME, vars[0].optval)) {
		pdo_namy_pool_error(dbh);
		goto cleanup;
	}
#endif

	dbname = vars[1].optval;
	host = vars[2].optval;	
	if(vars[3].optval) {
		port = atoi(vars[3].optval);
	}
	if (vars[2].optval && !strcmp("localhost", vars[2].optval)) {
		unix_socket = vars[4].optval;  
	}

	/* TODO: - Check zval cache + ZTS */
#ifdef PDO_USE_MYSQLND
	if (dbname) {
		dbname_len = strlen(dbname);
	}

	if (dbh->password) {
		password_len = strlen(dbh->password);
	}

	if (mysqlnd_connect(H->server, host, dbh->username, dbh->password, password_len, dbname, dbname_len,
						port, unix_socket, connect_opts TSRMLS_CC) == NULL) {
#else
	php_struct *ctx = SG(server_context);
	MYSQL *con = NULL;
	if (!ctx || (con = namy_attach_pool_connection(ctx->r, dbh->username))==NULL)
	{
		mysql_real_connect(H->server, host, dbh->username, dbh->password, dbname, port, unix_socket, connect_opts); 
	}
	else
	{
		// initに対するclose やらないとリークしてる気がする
		mysql_close(H->server);
		// コネクションを差し替えてるのでoption系を設定できてない
		// この辺どうするのか検討が必要
		H->server = con;
	}
	// original
	//if (mysql_real_connect(H->server, host, dbh->username, dbh->password, dbname, port, unix_socket, connect_opts) == NULL) {
	if (H->server == NULL) {
#endif
		pdo_namy_pool_error(dbh);
		goto cleanup;
	}

	if (!dbh->auto_commit) {
		namy_pool_handle_autocommit(dbh TSRMLS_CC);
	}

	H->attached = 1;

	dbh->alloc_own_columns = 1;
	dbh->max_escaped_char_length = 2;
	dbh->methods = &namy_pool_methods;

	ret = 1;
	
cleanup:
	for (i = 0; i < sizeof(vars)/sizeof(vars[0]); i++) {
		if (vars[i].freeme) {
			efree(vars[i].optval);
		}
	}
	
	dbh->methods = &namy_pool_methods;

	PDO_DBG_RETURN(ret);
}
/* }}} */

pdo_driver_t pdo_namy_pool_driver = {
	PDO_DRIVER_HEADER(namy_pool),
	pdo_namy_pool_handle_factory
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
