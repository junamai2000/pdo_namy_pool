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
  +----------------------------------------------------------------------+
*/

#ifndef PHP_PDO_NAMY_POOL_H
#define PHP_PDO_NAMY_POOL_H

extern zend_module_entry pdo_namy_pool_module_entry;
#define phpext_pdo_namy_pool_ptr &pdo_namy_pool_module_entry

#ifdef PHP_WIN32
#define PHP_PDO_NAMY_POOL_API __declspec(dllexport)
#else
#define PHP_PDO_NAMY_POOL_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif


#endif	/* PHP_PDO_MYSQL_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
