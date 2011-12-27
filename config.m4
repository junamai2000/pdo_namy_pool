dnl $Id$
dnl config.m4 for extension pdo_namy_pool

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(pdo_namy_pool, for pdo_namy_pool support,
dnl Make sure that the comment is aligned:
dnl [  --with-pdo_namy_pool             Include pdo_namy_pool support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(pdo_namy_pool, whether to enable pdo_namy_pool support,
Make sure that the comment is aligned:
[  --enable-pdo_namy_pool           Enable pdo_namy_pool support])

if test "$PHP_PDO_NAMY_POOL" != "no"; then
  dnl Write more examples of tests here...

  # --with-pdo_namy_pool -> check with-path
  # mysql
  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR="/include/mysql/mysql.h"
  AC_MSG_CHECKING([for mysql files in default path])
   for i in $SEARCH_PATH ; do
     if test -r $i/$SEARCH_FOR; then
       MYSQL_DIR=$i
       AC_MSG_RESULT(found in $i)
       PHP_ADD_INCLUDE($MYSQL_DIR/include/mysql)
       PHP_ADD_LIBRARY_WITH_PATH(mysqlclient, $MYSQL_DIR/lib/mysql, PDO_NAMY_POOL_SHARED_LIBADD)
     fi
   done
  
  if test -z "$MYSQL_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the mysql distribution])
  fi

 
  # httpd 
  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR="/include/httpd/httpd.h"
  AC_MSG_CHECKING([for httpd files in default path])
   for i in $SEARCH_PATH ; do
     if test -r $i/$SEARCH_FOR; then
       HTTPD_DIR=$i
       AC_MSG_RESULT(found in $i)
       PHP_ADD_INCLUDE($HTTPD_DIR/include/httpd)
     fi
   done
  
  if test -z "$HTTPD_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the mysql distribution])
  fi
  
  # apr
  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR="/include/apr-1/apr.h"
  AC_MSG_CHECKING([for apr files in default path])
   for i in $SEARCH_PATH ; do
     if test -r $i/$SEARCH_FOR; then
       APR_DIR=$i
       AC_MSG_RESULT(found in $i)
       PHP_ADD_INCLUDE($APR_DIR/include/apr-1)
       PHP_ADD_LIBRARY_WITH_PATH(apr-1, $APR_DIR/lib, PDO_NAMY_POOL_SHARED_LIBADD)
     fi
   done
  
  if test -z "$APR_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the apr distribution])
  fi

  # このファイルってインストールされないよね？？？
  # sapi/apache2handler/php_apache.h
  SEARCH_PATH="/usr/local /usr /usr/src/php-5.3.8"
  SEARCH_FOR="/sapi/apache2handler/php_apache.h"
  AC_MSG_CHECKING([for sapi/apache2handler/php_apache.h files in default path])
   for i in $SEARCH_PATH ; do
     if test -r $i/$SEARCH_FOR; then
       APR_DIR=$i
       AC_MSG_RESULT(found in $i)
       PHP_ADD_INCLUDE($APR_DIR/)
     fi
   done
  
  if test -z "$APR_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the apr distribution])
  fi
  
  # mod_namy_pool.h
  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR="include/httpd/mod_namy_pool.h"
  AC_MSG_CHECKING([for mod_namy_pool.h files in default path])
   for i in $SEARCH_PATH ; do
     if test -r $i/$SEARCH_FOR; then
       MOD_DIR=$i
       AC_MSG_RESULT(found in $i)
       PHP_ADD_INCLUDE($MOD_DIR/include/httpd)
       PHP_ADD_LIBRARY_WITH_PATH(namy_pool, $MOD_DIR/lib/httpd, PDO_NAMY_POOL_SHARED_LIBADD)
     fi
   done
  
  if test -z "$APR_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the mod_namy_pool distribution])
  fi

  # --with-pdo_namy_pool -> add include path
  PHP_ADD_INCLUDE($PDO_NAMY_POOL_DIR/include)

  dnl # --with-pdo_namy_pool -> check for lib and symbol presence
  dnl LIBNAME=pdo_namy_pool # you may want to change this
  dnl LIBSYMBOL=pdo_namy_pool # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $PDO_NAMY_POOL_DIR/lib, PDO_NAMY_POOL_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_PDO_NAMY_POOLLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong pdo_namy_pool lib version or lib not found])
  dnl ],[
  dnl   -L$PDO_NAMY_POOL_DIR/lib -lm
  dnl ])
  dnl
  dnl PHP_SUBST(PDO_NAMY_POOL_SHARED_LIBADD)
  PHP_SUBST(PDO_NAMY_POOL_SHARED_LIBADD)
  PHP_NEW_EXTENSION(pdo_namy_pool, pdo_namy_pool.c namy_pool_driver.c namy_pool_statement.c, $ext_shared,,-I$pdo_inc_path -I)
fi
