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

  dnl # --with-pdo_namy_pool -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/pdo_namy_pool.h"  # you most likely want to change this
  dnl if test -r $PHP_PDO_NAMY_POOL/$SEARCH_FOR; then # path given as parameter
  dnl   PDO_NAMY_POOL_DIR=$PHP_PDO_NAMY_POOL
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for pdo_namy_pool files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       PDO_NAMY_POOL_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$PDO_NAMY_POOL_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the pdo_namy_pool distribution])
  dnl fi

  # --with-pdo_namy_pool -> add include path
  PHP_ADD_INCLUDE($PDO_NAMY_POOL_DIR/include)
  dnl PHP_ADD_INCLUDE($PDO_NAMY_POOL_DIR/../../include)
  dnl PHP_ADD_INCLUDE($PDO_NAMY_POOL_DIR/../../main)
  dnl PHP_ADD_INCLUDE($PDO_NAMY_POOL_DIR/../../ext)
  dnl PHP_ADD_INCLUDE(/usr/local/include/mysql)

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

  PHP_ADD_LIBRARY_WITH_PATH(namy_pool, /usr/local/lib, PDO_NAMY_POOL_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(mysqlclient, /usr/local/lib/mysql, PDO_NAMY_POOL_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(apr-1, /usr/lib, PDO_NAMY_POOL_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(aprutil-1, /usr/lib, PDO_NAMY_POOL_SHARED_LIBADD)
  PHP_SUBST(PDO_NAMY_POOL_SHARED_LIBADD)
  PHP_NEW_EXTENSION(pdo_namy_pool, pdo_namy_pool.c namy_pool_driver.c namy_pool_statement.c, $ext_shared,,-I$pdo_inc_path -I)
fi
