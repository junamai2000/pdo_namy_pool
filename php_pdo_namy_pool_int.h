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

#ifndef PHP_PDO_NAMY_POOL_INT_H
#define PHP_PDO_NAMY_POOL_INT_H

#include <mysql.h>
#define PDO_NAMY_POOL_PARAM_BIND MYSQL_BIND

#if defined(PDO_USE_MYSQLND)
#	include "ext/mysqlnd/mysqlnd.h"
#	include "ext/mysqlnd/mysqlnd_libmysql_compat.h"
#	define PDO_NAMY_POOL_PARAM_BIND MYSQLND_PARAM_BIND
#else
#	include <mysql.h>
#	define PDO_NAMY_POOL_PARAM_BIND MYSQL_BIND
#endif

#if (MYSQL_VERSION_ID >= 40113 && MYSQL_VERSION_ID < 50000) || MYSQL_VERSION_ID >= 50007 || defined(MYSQL_USE_MYSQLND)
# define PDO_MYSQL_HAS_CHARSET
#endif

#if defined(PDO_USE_MYSQLND) && PHP_DEBUG && !defined(PHP_WIN32)
#define PDO_DBG_ENABLED 1

#define PDO_DBG_INF(msg) do { if (dbg_skip_trace == FALSE) PDO_MYSQL_G(dbg)->m->log(PDO_MYSQL_G(dbg), __LINE__, __FILE__, -1, "info : ", (msg)); } while (0)
#define PDO_DBG_ERR(msg) do { if (dbg_skip_trace == FALSE) PDO_MYSQL_G(dbg)->m->log(PDO_MYSQL_G(dbg), __LINE__, __FILE__, -1, "error: ", (msg)); } while (0)
#define PDO_DBG_INF_FMT(...) do { if (dbg_skip_trace == FALSE) PDO_MYSQL_G(dbg)->m->log_va(PDO_MYSQL_G(dbg), __LINE__, __FILE__, -1, "info : ", __VA_ARGS__); } while (0)
#define PDO_DBG_ERR_FMT(...) do { if (dbg_skip_trace == FALSE) PDO_MYSQL_G(dbg)->m->log_va(PDO_MYSQL_G(dbg), __LINE__, __FILE__, -1, "error: ", __VA_ARGS__); } while (0)
#define PDO_DBG_ENTER(func_name) zend_bool dbg_skip_trace = TRUE; if (PDO_MYSQL_G(dbg)) dbg_skip_trace = !PDO_MYSQL_G(dbg)->m->func_enter(PDO_MYSQL_G(dbg), __LINE__, __FILE__, func_name, strlen(func_name));
#define PDO_DBG_RETURN(value)	do { if (PDO_MYSQL_G(dbg)) PDO_MYSQL_G(dbg)->m->func_leave(PDO_MYSQL_G(dbg), __LINE__, __FILE__, 0); return (value); } while (0)
#define PDO_DBG_VOID_RETURN		do { if (PDO_MYSQL_G(dbg)) PDO_MYSQL_G(dbg)->m->func_leave(PDO_MYSQL_G(dbg), __LINE__, __FILE__, 0); return; } while (0)

#else
#define PDO_DBG_ENABLED 0

static inline void PDO_DBG_INF(char *msg) {}
static inline void PDO_DBG_ERR(char *msg) {}
static inline void PDO_DBG_INF_FMT(char *format, ...) {}
static inline void PDO_DBG_ERR_FMT(char *format, ...) {}
static inline void PDO_DBG_ENTER(char *func_name) {}
#define PDO_DBG_RETURN(value)	return (value)
#define PDO_DBG_VOID_RETURN		return;

#endif

#if defined(PDO_USE_MYSQLND)
#include "ext/mysqlnd/mysqlnd_debug.h"
#endif

ZEND_BEGIN_MODULE_GLOBALS(pdo_mysql)
	DL_HANDLE so_handle; // The path of mod_namy_pool.so, so that we can resolve symbols
#ifndef PHP_WIN32
	char          *default_socket;
#endif
#if PDO_DBG_ENABLED
	char          *debug; /* The actual string */
	MYSQLND_DEBUG *dbg;	/* The DBG object */
#endif
#if defined(PHP_WIN32) && !PDO_DBG_ENABLED
	/* dummy member so we get at least one member in the struct
	 * and avoids build errors.
	 */
	void *dummymemmber;
#endif
ZEND_END_MODULE_GLOBALS(pdo_namy_pool)

ZEND_EXTERN_MODULE_GLOBALS(pdo_namy_pool)

#ifdef ZTS
#define PDO_NAMY_POOL_G(v) TSRMG(pdo_namy_pool_globals_id, zend_pdo_namy_pool_globals *, v)
#else
#define PDO_NAMY_POOL_G(v) (pdo_namy_pool_globals.v)
#endif


typedef struct {
	const char *file;
	int line;
	unsigned int errcode;
	char *errmsg;
} pdo_namy_pool_error_info;

/* stuff we use in a mySQL database handle */
typedef struct {
	MYSQL 		*server;

	unsigned attached:1;
	unsigned buffered:1;
	unsigned emulate_prepare:1;
	unsigned fetch_table_names:1;
	unsigned _reserved:31;	
#if !PDO_USE_MYSQLND
	unsigned long max_buffer_size;
#endif

	pdo_namy_pool_error_info einfo;
} pdo_namy_pool_db_handle;

typedef struct {
	MYSQL_FIELD		*def;
} pdo_namy_pool_column;

typedef struct {
	pdo_namy_pool_db_handle 	*H;
	MYSQL_RES		*result;
	const MYSQL_FIELD	*fields;
	MYSQL_ROW		current_data;
#if PDO_USE_MYSQLND
	unsigned long		*current_lengths;
#else
	long			*current_lengths;
#endif
	pdo_namy_pool_error_info einfo;
#if HAVE_MYSQL_STMT_PREPARE || PDO_USE_MYSQLND
#if PDO_USE_MYSQLND
	MYSQLND_STMT 		*stmt;
#else
	MYSQL_STMT		*stmt;
#endif	
	int num_params;
	PDO_NAMY_POOL_PARAM_BIND	*params;
#ifndef PDO_USE_MYSQLND
	my_bool			*in_null;
	unsigned long		*in_length;
#endif
	PDO_NAMY_POOL_PARAM_BIND	*bound_result;
	my_bool			*out_null;
	unsigned long		*out_length;
	unsigned int		params_given;
	unsigned		max_length:1;
#endif
} pdo_namy_pool_stmt;

extern pdo_driver_t pdo_namy_pool_driver;

extern int _pdo_namy_pool_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, const char *file, int line TSRMLS_DC);
#define pdo_namy_pool_error(s) _pdo_namy_pool_error(s, NULL, __FILE__, __LINE__ TSRMLS_CC)
#define pdo_namy_pool_error_stmt(s) _pdo_namy_pool_error(stmt->dbh, stmt, __FILE__, __LINE__ TSRMLS_CC)

extern struct pdo_stmt_methods namy_pool_stmt_methods;

enum {
	PDO_NAMY_POOL_ATTR_USE_BUFFERED_QUERY = PDO_ATTR_DRIVER_SPECIFIC,
	PDO_NAMY_POOL_ATTR_LOCAL_INFILE,
	PDO_NAMY_POOL_ATTR_INIT_COMMAND,
#ifndef PDO_USE_MYSQLND
	PDO_NAMY_POOL_ATTR_READ_DEFAULT_FILE,
	PDO_NAMY_POOL_ATTR_READ_DEFAULT_GROUP,
	PDO_NAMY_POOL_ATTR_MAX_BUFFER_SIZE,
	PDO_NAMY_POOL_ATTR_COMPRESS,
#endif
	PDO_NAMY_POOL_ATTR_DIRECT_QUERY,
	PDO_NAMY_POOL_ATTR_FOUND_ROWS,
	PDO_NAMY_POOL_ATTR_IGNORE_SPACE,
	PDO_NAMY_POOL_ATTR_SSL_KEY,
	PDO_NAMY_POOL_ATTR_SSL_CERT,
	PDO_NAMY_POOL_ATTR_SSL_CA,
	PDO_NAMY_POOL_ATTR_SSL_CAPATH,
	PDO_NAMY_POOL_ATTR_SSL_CIPHER
};

#endif
