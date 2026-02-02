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

#include <string.h> // strcmp
#include <syslog.h>
#include <tcl.h>
#include "config.h"

#ifndef __cplusplus
#ifndef __bool_true_false_are_defined
typedef _Bool bool;
#define true 1
#define false 0
#endif
#endif

#define ERROR -1

/*
 * Function Prototypes
 */

static int SyslogCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int convert_facility (Tcl_Interp *interp, const char *facility);
static int convert_priority (Tcl_Interp *interp, const char *priority);

typedef struct SyslogThreadStatus {
    char *ident;
    int facility;
    int option;
    int priority;
    int has_priority;
    int opened;
    int initialized;
} SyslogThreadStatus;

static Tcl_ThreadDataKey syslogKey;
static Tcl_Mutex syslogMutex;

static SyslogThreadStatus *get_thread_status(void);
static void reset_thread_status(SyslogThreadStatus *status);

/*
 * Function Bodies
 */

static SyslogThreadStatus *get_thread_status(void) {
    SyslogThreadStatus *status = (SyslogThreadStatus *) Tcl_GetThreadData(&syslogKey, sizeof(SyslogThreadStatus));
    if (!status->initialized) {
        status->ident = NULL;
        status->facility = LOG_USER;
        status->option = LOG_NDELAY;
        status->priority = LOG_DEBUG;
        status->has_priority = 0;
        status->opened = 0;
        status->initialized = 1;
    }
    return status;
}

static void reset_thread_status(SyslogThreadStatus *status) {
    if (status->ident != NULL) {
        Tcl_Free(status->ident);
        status->ident = NULL;
    }
    status->facility = LOG_USER;
    status->option = LOG_NDELAY;
    status->priority = LOG_DEBUG;
    status->has_priority = 0;
    status->opened = 0;
}

int Syslog_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6-10", 0) == NULL) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp,PACKAGE_NAME,SyslogCmd,(ClientData) NULL,NULL);
    Tcl_PkgProvide(interp,PACKAGE_NAME,PACKAGE_VERSION);
    return TCL_OK;
}

int syslog_Init(Tcl_Interp *interp) {
    return Syslog_Init(interp);
}

static void wrong_arguments_message (Tcl_Interp* interp,int c,Tcl_Obj *CONST86 objv[])
{
    Tcl_WrongNumArgs(interp, c, objv, "?open|close? ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-priority priority? ?priority message?");
}

static int parse_options(Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[], int *index,
                         const char **ident, int *facility, int *option, int *priority, int *priority_set,
                         int *options_set) {
    const char *argument = NULL;
    while (*index < objc) {
        argument = Tcl_GetString(objv[*index]);
        if (strcmp("-ident", argument) == 0) {
            if (objc == *index + 1) {
                wrong_arguments_message(interp, 1, objv);
                return TCL_ERROR;
            }
            *ident = Tcl_GetString(objv[++(*index)]);
            *options_set = 1;
        } else if (strcmp("-facility", argument) == 0) {
            if (objc == *index + 1) {
                wrong_arguments_message(interp, 1, objv);
                return TCL_ERROR;
            }
            const char *facility_s = Tcl_GetString(objv[++(*index)]);
            int f = convert_facility(interp, facility_s);
            if (f == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown facility specified.",-1));
                return TCL_ERROR;
            }
            *facility = f;
            *options_set = 1;
        } else if (strcmp("-pid", argument) == 0) {
            *option = *option | LOG_PID;
            *options_set = 1;
        } else if (strcmp("-perror", argument) == 0) {
            *option = *option | LOG_PERROR;
            *options_set = 1;
        } else if (strcmp("-priority", argument) == 0) {
            if (objc == *index + 1) {
                wrong_arguments_message(interp, 1, objv);
                return TCL_ERROR;
            }
            const char *priority_s = Tcl_GetString(objv[++(*index)]);
            int p = convert_priority(interp, priority_s);
            if (p == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown priority specified.",-1));
                return TCL_ERROR;
            }
            *priority = p;
            *priority_set = 1;
            *options_set = 1;
        } else {
            break;
        }
        (*index)++;
    }
    return TCL_OK;
}

static int SyslogCmd (ClientData clientData,Tcl_Interp *interp,int objc,Tcl_Obj *CONST86 objv[]) {
    const char* ident       = NULL;
    const char* priority_s  = NULL;
    const char* message     = NULL;
    int   facility          = LOG_USER;
    int   priority          = LOG_DEBUG;
    int   option            = LOG_NDELAY;
    int   priority_set      = 0;
    int   options_set       = 0;
    SyslogThreadStatus *status = get_thread_status();

    /*  
     *  having less than 2 arguments to 'syslog' is wrong 
     *  anyway and we print the usual error message about
     *  the command usage
     */

    if (objc < 2) {
        wrong_arguments_message(interp, 1, objv);
        return TCL_ERROR;
    }

    const char *subcmd = Tcl_GetString(objv[1]);
    if (strcmp(subcmd, "open") == 0) {
        int i = 2;
        ident = status->ident;
        facility = status->facility;
        option = status->option;
        priority = status->priority;
        priority_set = status->has_priority;
        if (parse_options(interp, objc, objv, &i, &ident, &facility, &option, &priority, &priority_set, &options_set) != TCL_OK) {
            return TCL_ERROR;
        }
        if (i < objc) {
            wrong_arguments_message(interp, 1, objv);
            return TCL_ERROR;
        }
        if (status->ident != NULL) {
            Tcl_Free(status->ident);
            status->ident = NULL;
        }
        if (ident != NULL) {
            size_t len = strlen(ident);
            char *copy = (char *) Tcl_Alloc(len + 1);
            memcpy(copy, ident, len + 1);
            status->ident = copy;
        }
        status->facility = facility;
        status->option = option;
        status->priority = priority;
        status->has_priority = priority_set;
        status->opened = 1;
        Tcl_MutexLock(&syslogMutex);
        openlog(status->ident, status->option, status->facility);
        Tcl_MutexUnlock(&syslogMutex);
        return TCL_OK;
    }

    if (strcmp(subcmd, "close") == 0) {
        if (objc != 2) {
            wrong_arguments_message(interp, 1, objv);
            return TCL_ERROR;
        }
        Tcl_MutexLock(&syslogMutex);
        closelog();
        Tcl_MutexUnlock(&syslogMutex);
        reset_thread_status(status);
        return TCL_OK;
    }

    int i = 1;
    ident = status->ident;
    facility = status->facility;
    option = status->option;
    priority = status->priority;
    priority_set = status->has_priority;
    if (parse_options(interp, objc, objv, &i, &ident, &facility, &option, &priority, &priority_set, &options_set) != TCL_OK) {
        return TCL_ERROR;
    }

    int remaining = objc - i;
    if (remaining == 2) {
        priority_s = Tcl_GetString(objv[i]);
        priority = convert_priority(interp, priority_s);
        if (priority == ERROR) {
            Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown priority specified.",-1));
            return TCL_ERROR;
        }
        priority_set = 1;
        message = Tcl_GetString(objv[i + 1]);
    } else if (remaining == 1) {
        if (!priority_set) {
            wrong_arguments_message(interp, 1, objv);
            Tcl_SetObjResult(interp,Tcl_NewStringObj("Missing syslog priority argument",-1));
            return TCL_ERROR;
        }
        message = Tcl_GetString(objv[i]);
    } else {
        wrong_arguments_message(interp, 1, objv);
        return TCL_ERROR;
    }

    if (options_set || priority_set) {
        if (status->ident != NULL) {
            Tcl_Free(status->ident);
            status->ident = NULL;
        }
        if (ident != NULL) {
            size_t len = strlen(ident);
            char *copy = (char *) Tcl_Alloc(len + 1);
            memcpy(copy, ident, len + 1);
            status->ident = copy;
        }
        status->facility = facility;
        status->option = option;
        status->priority = priority;
        status->has_priority = priority_set;
        status->opened = 1;
    }

    Tcl_MutexLock(&syslogMutex);
    openlog(ident, option, facility);
    syslog(priority,"%s",message);
    if (!status->opened) {
        closelog();
    }
    Tcl_MutexUnlock(&syslogMutex);

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

static int convert_priority (Tcl_Interp *interp, const char *priority) {
    static char* priorities[]    = {"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", NULL};
    static int   priority_code[] = {LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR , LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG};
    int index;

    Tcl_Obj *priority_o = Tcl_NewStringObj(priority, -1);
    Tcl_IncrRefCount(priority_o); /* see convert_facility */

    if (Tcl_GetIndexFromObj(interp, priority_o, priorities, "priority", 0, &index) != TCL_OK) {
        Tcl_DecrRefCount(priority_o);
        return ERROR;
    }
    Tcl_DecrRefCount(priority_o);

    return priority_code[index];
}
