/*
 *    syslog - Syslog loggin service for Tcl
 *    A Tcl interface to the POSIX syslog service.
 *
 *    Copyright (C) 2026 Massimo Manghi <mxmanghi@apache.org>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __syslog_h__
#define __syslog_h__

#include <tcl.h>

/* Definition suggested in
 *
 * https://www.gnu.org/software/autoconf/manual/autoconf-2.67/html_node/Particular-Headers.html
 *
 * in order to have a portable definition of the 'bool' data type
 */

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true  1
# define __bool_true_false_are_defined 1
#endif

typedef struct SyslogThreadStatus {
    char*   format;
    int     level;
    int     facility;
    bool    initialized;
    int     open_changed;
    char*   message;        /* volatile string pointer */
#ifdef TCL_SYSLOG_DEBUG
    uint32_t magic;
    int     count;
#endif
} SyslogThreadStatus;

typedef struct SyslogGlobalStatus {
    char*   ident;
    int     facility;
    int     options;
    bool    opened;
} SyslogGlobalStatus;


#ifdef TCL_THREADS

#define SYSLOG_MUTEX_LOCK   Tcl_MutexLock(&syslogMutex);
#define SYSLOG_MUTEX_UNLOCK Tcl_MutexUnlock(&syslogMutex);
#define SYSLOG_ATOMIC_ASSIGN(varname,sourcename) \
        Tcl_MutexLock(&syslogMutex); \
        varname = sourcename; \
        Tcl_MutexUnlock(&syslogMutex);
#else

#define SYSLOG_MUTEX_LOCK   
#define SYSLOG_MUTEX_UNLOCK
#define SYSLOG_ATOMIC_ASSIGN(varname,sourcename) varname = sourcename;

#endif

#define ERROR -1
#define SYSLOG_NS "::syslog"

int parse_open_options(Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[],
                       bool open_cmd,int* last_option_p,bool *unhandled_opt,char *tcl_command);
int parse_options(Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[],
                  SyslogThreadStatus* status,int* last_option_p,bool *unhandled_opt,
                  char* tcl_command);

/* facilities */

int facility_cli_to_code (Tcl_Interp *interp, const char *facility);
char* facility_code_to_cli (int code);
int level_cli_to_code (Tcl_Interp *interp, const char *level);
char* level_code_to_cli (int code);

#endif /* __syslog_h__ */
