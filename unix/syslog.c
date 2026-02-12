/*
 *    syslog - Syslog loggin service for Tcl
 *    A Tcl interface to the POSIX syslog service.
 *
 *    Copyright (C) 2008 Alexandros Stergiakis
 *    Copyright (C) 2024-26 Massimo Manghi <mxmanghi@apache.org>
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

/*
 * syslog.c
 *
 */

/* Rivet config */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef TCL_SYSLOG_DEBUG
#include <stdio.h>
#include <inttypes.h>
#define SYSLOG_MAGIC 0x5359534c 
#endif

#include <string.h>
#include <syslog.h>
#include <tcl.h>
#include <sys/param.h>

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

#ifdef TCL_SYSLOG_DEBUG
#define SYSLOG_DEBUG_MSG(s)  fprintf(stderr,"%s\n",s);
#else
#define SYSLOG_DEBUG_MSG(s)
#endif

#define ERROR -1
#define SYSLOG_NS "::syslog"

/*
 * Function Prototypes
 */

static int SyslogCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int SyslogOpenCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int SyslogCloseCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int SyslogLogmaskCmd (ClientData clientData, Tcl_Interp *interp, int objc,Tcl_Obj *CONST86 objv[]);
static int SyslogLogCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);

static void SyslogInitGlobal (void);
static int  convert_facility (Tcl_Interp *interp, const char *facility_s);
static int  convert_level    (Tcl_Interp *interp, const char *level_s);

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


static Tcl_ThreadDataKey syslogKey;
#ifdef TCL_THREADS
static Tcl_Mutex syslogMutex;
#define SYSLOG_MUTEX_LOCK   Tcl_MutexLock(&syslogMutex);
#define SYSLOG_MUTEX_UNLOCK Tcl_MutexUnlock(&syslogMutex);
#else
#define SYSLOG_MUTEX_LOCK   
#define SYSLOG_MUTEX_UNLOCK 
#endif

static SyslogGlobalStatus *g_status = NULL;
static SyslogThreadStatus *get_thread_status(void);
static const char* g_default_format         = "%s";
//static int       g_opensyslog_options     = LOG_ODELAY;

/*
 * Function Bodies
 */

static void SyslogInitGlobal (void) {
    g_status->ident      = NULL;
    g_status->facility   = LOG_USER;
    g_status->options    = LOG_ODELAY;
    g_status->opened     = false;
}

static void SyslogInitStatus (SyslogThreadStatus *status)
{
    status->format       = (char *) g_default_format;
    status->level        = LOG_DEBUG;
    status->facility     = 0;
    status->initialized  = true;
    status->message      = NULL;
    status->open_changed = 0;
#ifdef TCL_SYSLOG_DEBUG
    status->magic        = SYSLOG_MAGIC;
    status->count        = 0;
#endif
}

static SyslogThreadStatus* get_thread_status(void) {
    SyslogThreadStatus *status = (SyslogThreadStatus *) Tcl_GetThreadData(&syslogKey, sizeof(SyslogThreadStatus));

#ifdef TCL_SYSLOG_DEBUG
    if ((status->magic != SYSLOG_MAGIC) && status->initialized) {
        SYSLOG_DEBUG_MSG("SyslogThreadStatus corrupted!");
    }

    Tcl_ThreadId tid = Tcl_GetCurrentThread();
    char thread_debug[1024];
    sprintf(thread_debug, "tid=%" PRIuPTR " key=%p status=%p initialized=%d",
            (uintptr_t)tid, (void*)&syslogKey, (void*)status, (int)status->initialized);
    SYSLOG_DEBUG_MSG(thread_debug)
#endif

    if (!status->initialized) {
        SyslogInitStatus(status);
    }
    return status;
}

int Syslog_Init(Tcl_Interp *interp) {
    SyslogThreadStatus* status;

    if (Tcl_InitStubs(interp, "8.6-10", 0) == NULL) {
        return TCL_ERROR;
    }

    SYSLOG_MUTEX_LOCK
    if (g_status == NULL) {
        g_status = (SyslogGlobalStatus*) Tcl_Alloc(sizeof(SyslogGlobalStatus));
        SyslogInitGlobal();
    }
    SYSLOG_MUTEX_UNLOCK

    status = get_thread_status();
    SyslogInitStatus(status);

    /* Let's create the syslog namespace */

    Tcl_CreateObjCommand(interp,PACKAGE_NAME,SyslogCmd,(ClientData) NULL,NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::open",SyslogOpenCmd,(ClientData) NULL,NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::isopen",SyslogOpenCmd,(ClientData) "isopen",NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::logmask",SyslogLogmaskCmd,(ClientData) NULL,NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::close",SyslogCloseCmd,(ClientData) NULL,NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::log",SyslogLogCmd,(ClientData) NULL,NULL);
    Tcl_PkgProvide(interp,PACKAGE_NAME,PACKAGE_VERSION);
    return TCL_OK;
}

int syslog_Init(Tcl_Interp *interp) {
    return Syslog_Init(interp);
}

static void wrong_arguments_message (Tcl_Interp* interp,int c,Tcl_Obj *CONST86 objv[])
{
    Tcl_WrongNumArgs(interp,c,objv,
            "?open|close? ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-level level? message");
}

/*
 * parse_open_options
 *
 */

static int parse_open_options(Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[],bool open_cmd,int* last_option_p) {
    const char *argument    = NULL;
    int         index       = 1;
    int         fchanged    = 0;
    int         last_option_index = 0;

    while (index < objc) {
        argument = Tcl_GetString(objv[index]);
        if (strcmp("-ident", argument) == 0) {
            if (objc == index) {
                wrong_arguments_message(interp, 1, objv);
                return -1;
            }
            const char* ident = Tcl_GetString(objv[++index]);
            size_t len = strlen(ident);
            char *copy = (char *) Tcl_Alloc(len + 1);
            memcpy(copy,ident,len + 1);

            if (g_status->ident != NULL) {
                Tcl_Free(g_status->ident);
            }
            g_status->ident = copy;
            fchanged++;
            last_option_index = index;
        } else if (strcmp("-nodelay",argument) == 0) {
            g_status->options = g_status->options | LOG_NDELAY;
            fchanged++;
            last_option_index = index;
        } else if (strcmp("-console",argument) == 0) {
            g_status->options = g_status->options | LOG_CONS;
            fchanged++;
            last_option_index = index;
        } else if ((strcmp("-facility",argument) == 0) && open_cmd) {

            /*
             * the '-facility' switch must be handled in this context
             * only when the command 'syslog open...' is processed
             */

            if (objc == index) {
                wrong_arguments_message(interp,1,objv);
                return -1;
            }
            const char *facility_s = Tcl_GetString(objv[++index]);
            int f = convert_facility(interp, facility_s);
            if (f == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown facility specified.",-1));
                return -1;
            }
            g_status->facility = f;
            fchanged++;
            last_option_index = index;
        } else if (strcmp("-pid",argument) == 0) {
            g_status->options = g_status->options | LOG_PID;
            fchanged++;
            last_option_index = index;
        } else if (strcmp("-perror",argument) == 0) {
            g_status->options = g_status->options | LOG_PERROR;
            fchanged++;
            last_option_index = index;
        }
        index++;
    }
    *last_option_p = last_option_index;
    return fchanged;
}

static int parse_options(Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[], SyslogThreadStatus* status,int* last_option_p) {
    const char *argument = NULL;
    int         index    = 1;
    int         fchanged = 0;
    int         last_option_index = 0;

    status->facility = -1;
    if (status->format != g_default_format) {
        Tcl_Free(status->format);
        status->format = (char *) g_default_format;
    }
    while (index < objc) {
        argument = Tcl_GetString(objv[index]);
        if ((strcmp("-priority",argument) == 0) || 
            (strcmp("-level",argument)    == 0)) {

            if (objc == index) {
                wrong_arguments_message(interp, 1, objv);
                return -1;
            }

            const char *level_s = Tcl_GetString(objv[++index]);
            int p = convert_level(interp,level_s);
            if (p == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
                return -1;
            }
            status->level = p;
            fchanged++;
            last_option_index = index;
        } else if (strcmp("-facility",argument) == 0) {

            if (objc == index) {
                wrong_arguments_message(interp,1,objv);
                return -1;
            }
            const char *facility_s = Tcl_GetString(objv[++index]);
            int f = convert_facility(interp, facility_s);
            if (f == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown facility specified.",-1));
                return -1;
            }
            status->facility = f;
            fchanged++;
            last_option_index = index;
        } else if (strcmp("-format",argument) == 0) {

            if (objc == index) {
                wrong_arguments_message(interp, 1, objv);
                return -1;
            }

            const char *format_s = Tcl_GetString(objv[++index]);
            size_t len = strlen(format_s);
            char *copy = (char *) Tcl_Alloc(len + 1);
            memcpy(copy,format_s,len + 1);
            if ((status->format != NULL) && (status->format != g_default_format)) {
                Tcl_Free(status->format);
            }
            status->format = copy;
            fchanged++;
            last_option_index = index;
        }
        index++;
    }
    *last_option_p = last_option_index;
    return fchanged;
}

static void SyslogOpen(void)
{
    if (g_status->opened) {
        return;
    }
    SYSLOG_DEBUG_MSG("Calling openlog")

    openlog(g_status->ident,g_status->options,g_status->facility);
    g_status->opened = true;
}

static void SyslogClose(void)
{
    if (g_status->opened) {
        closelog();
        g_status->opened = false;
    }
    SYSLOG_DEBUG_MSG("Calling closelog")
}

static inline void log_message (SyslogThreadStatus* status,int open_changed) {

    if (open_changed > 0) {
        SyslogClose();
        SyslogOpen();
    }
    int facility = status->facility;
    if (facility < 0) {
        facility = g_status->facility;
    }
    syslog(LOG_MAKEPRI(facility,status->level),status->format,status->message);
#ifdef TCL_SYSLOG_DEBUG
    (status->count)++;
#endif

}

static int SyslogLogmaskCmd (ClientData clientData,
                             Tcl_Interp *interp,
                             int objc,Tcl_Obj *CONST86 objv[]) {
    return TCL_OK;
}

static int SyslogOpenCmd (ClientData clientData,
                          Tcl_Interp *interp,
                          int objc,Tcl_Obj *CONST86 objv[]) {

    char* cdata = (char *) clientData;
    if (cdata != NULL) {
        if (strcmp(cdata,"isopen") == 0) {
            SYSLOG_MUTEX_LOCK
            Tcl_SetObjResult(interp,Tcl_NewIntObj(g_status->opened));
            SYSLOG_MUTEX_UNLOCK
            return TCL_OK;
        } else {
            Tcl_SetErrorCode(interp,"internal_error",cdata,(char *)NULL);
            Tcl_AddErrorInfo(interp,"Internal error: invalid command argument");
            return TCL_ERROR;
        }
    }

    SYSLOG_MUTEX_LOCK
    int last_opt_index;
    if (parse_open_options(interp,objc,objv,true,&last_opt_index) < -1) {
        return TCL_ERROR;
    }

    if (last_opt_index < objc) {
        Tcl_WrongNumArgs(interp,objc,objv,
            "open ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-nodelay? ?-console?");
        return TCL_ERROR;
    }

    SyslogClose();
    SyslogOpen();

    SYSLOG_MUTEX_UNLOCK
    return TCL_OK;
}

static int SyslogCloseCmd (ClientData clientData,
                          Tcl_Interp *interp,
                          int objc,Tcl_Obj *CONST86 objv[]) {
    SYSLOG_MUTEX_LOCK

    SyslogClose();

    SYSLOG_MUTEX_UNLOCK
    return TCL_OK;
}

static int SyslogCmd (ClientData clientData,Tcl_Interp *interp,int objc,Tcl_Obj *CONST86 objv[]) {
    SyslogThreadStatus* status  = get_thread_status();

    /*  
     *  having less than 2 arguments to 'syslog' is wrong 
     *  anyway and we print the usual error message about
     *  the command usage. 
     */

    if (objc < 2) {
        wrong_arguments_message(interp, 1, objv);
        return TCL_ERROR;
    }

    /*
     *  syslog is called to actually log a message. 
     */

    int last_arg_opts = 0;
    if (parse_options(interp,objc, objv, status,&last_arg_opts) == -1) {
        return TCL_ERROR;
    }

    /* This CLI options are checked here for compatibility but it will be removed
       in new releases requiring the socket to the syslog utility to be explicitly
       reopened with new options */

    SYSLOG_MUTEX_LOCK

    int tcl_exit_code = TCL_OK;
    int last_open_opt = 0;
    status->open_changed = parse_open_options(interp,objc,objv,false,&last_open_opt);
    if (status->open_changed == -1) {
        tcl_exit_code = TCL_ERROR;
    } else {
        int first_non_opt_arg = MAX(last_open_opt,last_arg_opts) + 1;
        if (first_non_opt_arg == objc-2) {
            Tcl_Obj* level_o = objv[objc-2];

            int level_code = convert_level(interp,Tcl_GetString(level_o));
            if (level_code == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
                return TCL_ERROR;
            }
            status->level = level_code;
            status->message = Tcl_GetString(objv[objc-1]);
            log_message(status,status->open_changed);
        } else if (first_non_opt_arg == objc-1) {
            status->message = Tcl_GetString(objv[objc-1]);
            log_message(status,status->open_changed);
        }
    }

    SYSLOG_MUTEX_UNLOCK
    return tcl_exit_code;
}

static int SyslogLogCmd (ClientData clientData,
                          Tcl_Interp *interp,
                          int objc,Tcl_Obj *CONST86 objv[]) {
    SyslogThreadStatus* status  = get_thread_status();

    /* We repeat what we do in SyslogCmd but 
     * skip the processing of the open specific
     * options. This command is only for emitting
     * log messages
     */

    if (objc < 2) {
        wrong_arguments_message(interp, 1, objv);
        return TCL_ERROR;
    }

    int last_arg_opts = 0;
    if (parse_options(interp,objc, objv, status,&last_arg_opts) == -1) {
        return TCL_ERROR;
    }

    int first_non_opt_arg = last_arg_opts + 1;
    SYSLOG_MUTEX_LOCK
    if (first_non_opt_arg == objc-2) {
        Tcl_Obj* level_o = objv[objc-2];

        int level_code = convert_level(interp,Tcl_GetString(level_o));
        if (level_code == ERROR) {
            Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
            return TCL_ERROR;
        }
        status->level = level_code;
        status->message = Tcl_GetString(objv[objc-1]);
        log_message(status,status->open_changed);
    } else if (first_non_opt_arg == objc-1) {
        status->message = Tcl_GetString(objv[objc-1]);
        log_message(status,status->open_changed);
    }
    SYSLOG_MUTEX_UNLOCK
    return TCL_OK;
}

static int convert_facility (Tcl_Interp *interp, const char *facility) {
    static char* facilities[] = { "kern", "user", "mail", "daemon", "auth", "syslog",
                                  "lpr", "news", "uucp", "cron", "authpriv", "ftp",
                                  "local0", "local1", "local2", "local3", "local4",
                                  "local5", "local6", "local7", NULL };
    static int facility_code[] = { LOG_KERN, LOG_USER, LOG_MAIL, LOG_DAEMON, LOG_AUTH, LOG_SYSLOG,
                                   LOG_LPR, LOG_NEWS, LOG_UUCP, LOG_CRON, LOG_AUTHPRIV, LOG_FTP,
                                   LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4,
                                   LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7 };
    int index;
    Tcl_Obj *facility_o = Tcl_NewStringObj(facility, -1);
    Tcl_IncrRefCount(facility_o); /* Let's protect its memory */

    if (Tcl_GetIndexFromObj(interp, facility_o, facilities, "facility", 0, &index) != TCL_OK) {
        Tcl_DecrRefCount(facility_o);
        return ERROR;
    }
    Tcl_DecrRefCount(facility_o);

    return facility_code[index];
}

static int convert_level (Tcl_Interp *interp, const char *level) {
    static char* levels[]     = {"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", NULL};
    static int   level_code[] = {LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR , LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG};
    int index;

    Tcl_Obj *level_o = Tcl_NewStringObj(level, -1);
    Tcl_IncrRefCount(level_o); /* see convert_facility */

    if (Tcl_GetIndexFromObj(interp, level_o, levels, "level", 0, &index) != TCL_OK) {
        Tcl_DecrRefCount(level_o);
        return ERROR;
    }
    Tcl_DecrRefCount(level_o);

    return level_code[index];
}
