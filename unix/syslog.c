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

#define ERROR -1

/*
 * Function Prototypes
 */

static int SyslogCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int convert_facility (Tcl_Interp *interp, const char *facility_s);
static int convert_level    (Tcl_Interp *interp, const char *level_s);

typedef struct SyslogThreadStatus {
    char*   ident;
    char*   message;
    char*   format;
    int     facility;
    int     level;
    int     options;
    bool    opened;
    bool    initialized;
#ifdef TCL_SYSLOG_DEBUG
    uint32_t magic;
    int     count;
#endif
} SyslogThreadStatus;

static Tcl_ThreadDataKey syslogKey;
#ifdef TCL_THREADS
static Tcl_Mutex syslogMutex;
#endif

static SyslogThreadStatus *get_thread_status(void);
static const char* g_default_format         = "%s";
static int         g_opensyslog_options     = LOG_ODELAY;

/*
 * Function Bodies
 */

static void SyslogInitStatus (SyslogThreadStatus *status)
{
    status->ident        = NULL;
    status->message      = NULL;
    status->format       = (char *) g_default_format;
    status->facility     = LOG_USER;
    status->level        = LOG_DEBUG;
    status->options      = LOG_ODELAY;

    status->opened       = false;
    status->initialized  = true;
#ifdef TCL_SYSLOG_DEBUG
    status->magic        = SYSLOG_MAGIC;
    status->count        = 0;
#endif
}

static SyslogThreadStatus* get_thread_status(void) {
    SyslogThreadStatus *status = (SyslogThreadStatus *) Tcl_GetThreadData(&syslogKey, sizeof(SyslogThreadStatus));

#ifdef TCL_SYSLOG_DEBUG
    if ((status->magic != SYSLOG_MAGIC) && status->initialized) {
        fprintf(stderr, "SyslogThreadStatus corrupted!\n");
    }

    Tcl_ThreadId tid = Tcl_GetCurrentThread();
    fprintf(stderr, "tid=%" PRIuPTR " key=%p status=%p initialized=%d\n",
            (uintptr_t)tid, (void*)&syslogKey, (void*)status, (int)status->initialized);
#endif

    if (!status->initialized) {
        SyslogInitStatus(status);
    }
    return status;
}

/*
static void reset_thread_status(SyslogThreadStatus *status) {
    if (status->ident != NULL) {
        Tcl_Free(status->ident);
    }
    if (status->message != NULL) {
        Tcl_Free(status->message);
    }
    SyslogInitStatus(status);
}
*/

int Syslog_Init(Tcl_Interp *interp) {
    SyslogThreadStatus* status;

    if (Tcl_InitStubs(interp, "8.6-10", 0) == NULL) {
        return TCL_ERROR;
    }
    status  = get_thread_status();
    SyslogInitStatus(status);
    Tcl_CreateObjCommand(interp,PACKAGE_NAME,SyslogCmd,(ClientData) NULL,NULL);
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

static int parse_open_options(Tcl_Interp *interp, int* obj_count, Tcl_Obj *CONST86 objv[], SyslogThreadStatus* status) {
    const char *argument    = NULL;
    int         index       = 1;
    int         objc        = *obj_count;

    while (index < objc) {
        if (strcmp("-ident", argument) == 0) {
            if (objc == index) {
                wrong_arguments_message(interp, 1, objv);
                return TCL_ERROR;
            }
            const char* ident = Tcl_GetString(objv[++index]);
            size_t len = strlen(ident);
            char *copy = (char *) Tcl_Alloc(len + 1);
            memcpy(copy,ident,len + 1);

            if (status->ident != NULL) {
                Tcl_Free(status->ident);
            }
            status->ident = copy;
        } else if (strcmp("-nodelay", argument) == 0) {
            status->options = status->options | LOG_NDELAY;
        } else if (strcmp("-console", argument) == 0) {
            status->options = status->options | LOG_CONS;
        } else if (strcmp("-facility", argument) == 0) {
            if (objc == index) {
                wrong_arguments_message(interp,1,objv);
                return TCL_ERROR;
            }
            const char *facility_s = Tcl_GetString(objv[++index]);
            int f = convert_facility(interp, facility_s);
            if (f == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown facility specified.",-1));
                return TCL_ERROR;
            }
            status->facility = f;
        } else if (strcmp("-pid",argument) == 0) {
            status->options = status->options | LOG_PID;
        } else if (strcmp("-perror",argument) == 0) {
            status->options = status->options | LOG_PERROR;
        } else {
            break;  /* we reached the end of the list of options '-opt1 -opt2 val -opt3.... */
        }
        index++;
    }
    *obj_count = index;
    return TCL_OK;
}

static int parse_options(Tcl_Interp *interp, int* obj_count, Tcl_Obj *CONST86 objv[], SyslogThreadStatus* status) {
    const char *argument = NULL;
    int         index    = 1;
    int         objc     = *obj_count;

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
                return TCL_ERROR;
            }

            const char *level_s = Tcl_GetString(objv[++index]);
            int p = convert_level(interp,level_s);
            if (p == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
                return TCL_ERROR;
            }
            status->level = p;
        } else if (strcmp("-format",argument) == 0) {
            if (objc == index) {
                wrong_arguments_message(interp, 1, objv);
                return TCL_ERROR;
            }

            const char *format_s = Tcl_GetString(objv[++index]);
            size_t len = strlen(format_s);
            char *copy = (char *) Tcl_Alloc(len + 1);
            memcpy(copy,format_s,len + 1);
            if (status->format != NULL) {
                Tcl_Free(status->format);
            }
            status->format = copy;
            break;
        } else if (index == objc - 1) {
            char* message = Tcl_GetString(objv[index]);
            size_t len = strlen(message);
            char *copy = (char *) Tcl_Alloc(len + 1);
            memcpy(copy,message,len + 1);
            if (status->message != NULL) {
                Tcl_Free(status->message);
            }
            status->message = copy;
            break;
        } else {
            break;  /* we reach the end of the list of options */
        }
        index++;
    }

    *obj_count = index;
    return TCL_OK;
}

static void SyslogOpen(SyslogThreadStatus* status)
{
    if (status->opened) {
        return;
    }
#ifdef TCL_SYSLOG_DEBUG
    fprintf(stderr, "Calling openlog\n");
#endif
    openlog(status->ident,status->options,status->facility);
    status->opened = true;
}

static void SyslogClose(SyslogThreadStatus* status)
{
    if (status->opened) {
        closelog();
        status->opened = false;
    }
#ifdef TCL_SYSLOG_DEBUG
    fprintf(stderr, "Calling closelog\n");
#endif
    //reset_thread_status(status);
}

static int SyslogCmd (ClientData clientData,Tcl_Interp *interp,int objc,Tcl_Obj *CONST86 objv[]) {
    SyslogThreadStatus* status  = get_thread_status();
    const char* first_arg = Tcl_GetString(objv[1]);

    /*  
     *  having less than 2 arguments to 'syslog' is wrong 
     *  anyway and we print the usual error message about
     *  the command usage
     */

    if (objc < 2) {
        wrong_arguments_message(interp, 1, objv);
        return TCL_ERROR;
    } else if (strcmp(first_arg,"close") == 0) {
        if (objc != 2) {
            wrong_arguments_message(interp, 1, objv);
            return TCL_ERROR;
        }
        
        SyslogClose(status);
        return TCL_OK;
    } else if (strcmp(first_arg,"open") == 0) {
        if (parse_open_options(interp, &objc, &objv[2], status) != TCL_OK) {
            return TCL_ERROR;
        }
        SyslogOpen(status);
        return TCL_OK;
    } else if (strcmp(first_arg,"isopen") == 0) {
        Tcl_SetObjResult(interp,Tcl_NewIntObj(status->opened));
        return TCL_OK;
    } else {
        int remaining_obj_count = objc;
        Tcl_Obj* level_o    = NULL;
        Tcl_Obj* message_o  = NULL;

        /* This CLI options are checked here for compatibility but it will be removed
           in new releases requiring the socket to the syslog utility to be explicitly
           reopened with new options */

        if (parse_open_options(interp, &objc, &objv[1], status) != TCL_OK) {
            return TCL_ERROR;
        }
        if (parse_options(interp, &objc, &objv[1], status) != TCL_OK) {
            return TCL_ERROR;
        }

        remaining_obj_count -= objc;
        switch (remaining_obj_count) {
            case 0:
                break;
            case 1:
                message_o   = objv[objc];
                break;
            case 2:
                level_o     = objv[objc];
                message_o   = objv[objc+1];
                break;
            default:
                break;
        }

        if (level_o != NULL) {
            int level_code = convert_level(interp,Tcl_GetString(level_o)); 
            if (level_code == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
                return TCL_ERROR;
            }
            status->level = level_code;
        }
        if (message_o != NULL) {
            const char* message_s = Tcl_GetString(message_o);
            size_t len = strlen(message_s);
            char *copy = (char *) Tcl_Alloc(len + 1);
            memcpy(copy,message_s,len + 1);

            if (status->message != NULL) {
                Tcl_Free(status->message);
            }
            status->message = copy;
        }
    }

#ifdef TCL_THREADS
    Tcl_MutexLock(&syslogMutex);
#endif

    if (g_opensyslog_options != status->options) {
        SyslogClose(status);
        SyslogOpen(status);
    }
    syslog(LOG_MAKEPRI(status->facility,status->level),status->format,status->message);

#ifdef TCL_THREADS
    Tcl_MutexUnlock(&syslogMutex);
#endif
#ifdef TCL_SYSLOG_DEBUG
    (status->count)++;
#endif
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
