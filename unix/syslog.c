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
static int SyslogConfigurationCmd (ClientData clientData, Tcl_Interp *interp, int objc,Tcl_Obj *CONST86 objv[]);
static int SyslogLogCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);

static void SyslogInitGlobal (void);

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
#define SYSLOG_ATOMIC(varname,sourcename) \
Tcl_MutexLock(&syslogMutex); \
varname = sourcename; \
Tcl_MutexUnlock(&syslogMutex);

#else

#define SYSLOG_MUTEX_LOCK   
#define SYSLOG_MUTEX_UNLOCK
#define SYSLOG_ATOMIC(varname,sourcename) varname = sourcename;

#endif

static SyslogGlobalStatus *g_status = NULL;
static SyslogThreadStatus *get_thread_status(void);
static const char* g_default_format         = "%s";
//static int       g_opensyslog_options     = LOG_ODELAY;

/* X-macro based database of facilities and levels
 *
 * The tables define 2 arrays for facilities and levels
 *
 *  1 'facilities' and 'levels' define the cli definition
 *    of such information
 *
 *  2 'facility_code' and 'level_code' map the corresponding
 *    cli definitions to the codes defined in syslog.h
 */

#define SYSLOG_FACILITIES(X) \
	X("kern",LOG_KERN,log_kern_idx,0) \
	X("user",LOG_USER,log_user_idx,1) \
	X("mail",LOG_MAIL,log_mail_idx,2) \
	X("daemon",LOG_DAEMON,log_daemon_idx,3) \
	X("auth",LOG_AUTH,log_auth_idx,4) \
	X("syslog",LOG_SYSLOG,log_syslog_idx,5) \
	X("lpr",LOG_LPR,log_lpr_idx,6) \
	X("news",LOG_NEWS,log_news_idx,7) \
	X("uucp",LOG_UUCP,log_uucp_idx,8) \
	X("cron",LOG_CRON,log_cron_idx,9) \
	X("authpriv",LOG_AUTHPRIV,log_authpriv_idx,10) \
	X("ftp",LOG_FTP,log_ftp_idx,11) \
	X("local0",LOG_LOCAL0,log_local0_idx,12) \
	X("local1",LOG_LOCAL1,log_local1_idx,13) \
	X("local2",LOG_LOCAL2,log_local2_idx,14) \
	X("local3",LOG_LOCAL3,log_local3_idx,15) \
	X("local4",LOG_LOCAL4,log_local4_idx,16) \
	X("local5",LOG_LOCAL5,log_local5_idx,17) \
	X("local6",LOG_LOCAL6,log_local6_idx,18) \
	X("local7",LOG_LOCAL7,log_local7_idx,19)

enum SyslogFacilitiesIndices {
#define SYSLOG_FAC_IDX(facility,facility_code,facility_idx,n) facility_idx = n,
    SYSLOG_FACILITIES(SYSLOG_FAC_IDX)
    num_syslog_facilities
};

static char* facilities[num_syslog_facilities + 1] = {
#define SYSLOG_FAC_CLI(facility,facility_code,facility_idx,n) [facility_idx] = facility,
    SYSLOG_FACILITIES(SYSLOG_FAC_CLI)
    [num_syslog_facilities] = NULL
};

static int facility_code[num_syslog_facilities + 1] = {
#define SYSLOG_FAC_CODE(facility,facility_code,facility_idx,n) [facility_idx] = facility_code,
    SYSLOG_FACILITIES(SYSLOG_FAC_CODE)
    [num_syslog_facilities] = -1
};

#define SYSLOG_LEVELS(X) \
	X("emergency",LOG_EMERG,log_emerg_idx,0) \
	X("alert",LOG_ALERT,log_alert_idx,1) \
	X("critical",LOG_CRIT,log_crit_idx,2) \
	X("error",LOG_ERR,log_err_idx,3) \
	X("warning",LOG_WARNING,log_warning_idx,4) \
	X("notice",LOG_NOTICE,log_notice_idx,5) \
	X("info",LOG_INFO,log_info_idx,6) \
	X("debug",LOG_DEBUG,log_debug_idx,7)

enum SyslogLevelsIndices {
#define SYSLOG_LEVEL_IDX(level,level_code,level_idx,n) level_idx = n,
    SYSLOG_LEVELS(SYSLOG_LEVEL_IDX)
    num_syslog_levels
};

static char* levels[num_syslog_levels+1] = {
#define SYSLOG_LEVEL_CLI(level,level_code,level_idx,n) [level_idx] = level,
    SYSLOG_LEVELS(SYSLOG_LEVEL_CLI)
    [num_syslog_levels] = NULL
};

static int level_code[num_syslog_levels+1] = {
#define SYSLOG_LEVEL_CODE(level,level_code,level_idx,n) [level_idx] = level_code,
    SYSLOG_LEVELS(SYSLOG_LEVEL_CODE)
    [num_syslog_levels] = -1
};

#define SYSLOG_OPTIONS(X) \
    X("-pid",LOG_PID,log_pid_idx) \
    X("-perror",LOG_PERROR,log_perror_idx) \
    X("-console",LOG_CONS,log_cons_idx) \
    X("-nodelay",LOG_NDELAY,log_ndelay_idx)

enum SyslogOptions {
#define SYSLOG_OPTIONS_IDX(option,optcode,option_idx) option_idx,
    SYSLOG_OPTIONS(SYSLOG_OPTIONS_IDX)

    num_syslog_options
};

static char* options[num_syslog_options+1] = {
#define SYSLOG_OPTION_CLI(option,optcode,option_idx) [option_idx] = option,
    SYSLOG_OPTIONS(SYSLOG_OPTION_CLI) 

    (char *) NULL
};

static int opt_code[num_syslog_options+1] = {
#define SYSLOG_OPTION_CODE(option,optcode,option_idx) [option_idx] = optcode,
    SYSLOG_OPTIONS(SYSLOG_OPTION_CODE)
    /* Sentinel */
    [num_syslog_options] = -1  
};
    

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
    status->level        = LOG_INFO;
    status->facility     = -1;
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
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::cget",SyslogConfigurationCmd,(ClientData) "cget",NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::log",SyslogLogCmd,(ClientData) NULL,NULL);
    Tcl_PkgProvide(interp,PACKAGE_NAME,PACKAGE_VERSION);
    return TCL_OK;
}

int syslog_Init(Tcl_Interp *interp) {
    return Syslog_Init(interp);
}

/* Facility and level mapping */

static int facility_cli_to_code (Tcl_Interp *interp, const char *facility) {
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

static char* facility_code_to_cli (int code) {
    int index;
    for (index = 0; index < num_syslog_facilities; index++) {
        if (facility_code[index] == code) { return facilities[index]; }
    }
    return NULL;
}

static int level_cli_to_code (Tcl_Interp *interp, const char *level) {
    int index;

    Tcl_Obj *level_o = Tcl_NewStringObj(level, -1);
    Tcl_IncrRefCount(level_o); /* see facility_cli_to_code */

    if (Tcl_GetIndexFromObj(interp, level_o, levels, "level", 0, &index) != TCL_OK) {
        Tcl_DecrRefCount(level_o);
        return ERROR;
    }
    Tcl_DecrRefCount(level_o);

    return level_code[index];
}

static char* level_code_to_cli (int code) {
    int index;
    for (index = 0; index < num_syslog_levels; index++) {
        if (level_code[index] == code) { return levels[index]; }
    }
    return NULL;
}

/*
 * Functions handing errors 
 */

static void wrong_arguments_message (Tcl_Interp* interp,int c,Tcl_Obj *CONST86 objv[])
{
    Tcl_WrongNumArgs(interp,c,objv,
            "?open|close? ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-level level? message");
}

static void missing_option_value (Tcl_Interp* interp,char* cmd,Tcl_Obj* option)
{
    Tcl_Obj* error_code_list = Tcl_NewObj();
    Tcl_IncrRefCount(error_code_list);

    Tcl_ListObjAppendElement(interp, error_code_list, Tcl_NewStringObj("wrong_argument_value", -1));
    Tcl_ListObjAppendElement(interp, error_code_list, option);
    Tcl_SetObjErrorCode(interp, error_code_list);

    Tcl_Obj* error_message = Tcl_NewStringObj("Missing option value for option '",-1);
    Tcl_IncrRefCount(error_message);

    Tcl_AppendObjToObj(error_message,option);
    Tcl_AppendStringsToObj(error_message,"' in ",cmd,NULL);

    Tcl_SetObjResult(interp,error_message);
    Tcl_DecrRefCount(error_message);

    Tcl_Obj* info_message = Tcl_NewStringObj("\n    (missing option value condition detected while parsing command ",-1);
    Tcl_IncrRefCount(info_message);
    Tcl_AppendStringsToObj(info_message,cmd,")",NULL);
    Tcl_AppendObjToErrorInfo(interp, info_message);

    Tcl_DecrRefCount(info_message);
    Tcl_DecrRefCount(error_code_list);
}


static void wrong_command_option(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *error_code_list = Tcl_NewObj();
    Tcl_IncrRefCount(error_code_list);

    Tcl_ListObjAppendElement(interp, error_code_list, Tcl_NewStringObj("wrong_arguments", -1));
    Tcl_ListObjAppendElement(interp, error_code_list, Tcl_NewListObj(objc, objv)); 
    Tcl_SetObjErrorCode(interp, error_code_list);

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid ::syslog::open option", -1));
    Tcl_AddErrorInfo(interp, "\n    (unrecognized option detected while parsing ::syslog::open arguments)");

    Tcl_DecrRefCount(error_code_list);
}

/*
 * parse_open_options
 *
 */

static int parse_open_options(Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[],
                              bool open_cmd,int* last_option_p,bool *unhandled_opt,char *tcl_command) {
    const char *argument    = NULL;
    int         index       = 1;
    int         fchanged    = 0;
    int         last_option_index = 0;

    while (index < objc) {
        argument = Tcl_GetString(objv[index]);
        if (strcmp("-ident", argument) == 0) {
            if (index == objc-1) {
                missing_option_value(interp,tcl_command,objv[index]);
                return ERROR;
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
                missing_option_value(interp,tcl_command,objv[index]);
                return ERROR;
            }
            const char *facility_s = Tcl_GetString(objv[++index]);
            int f = facility_cli_to_code(interp, facility_s);
            if (f == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown facility specified.",-1));
                return ERROR;
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
        } else if (argument[0] == '-') {
            *unhandled_opt = true;
        } else {
            wrong_arguments_message(interp, 1, objv);
            return ERROR;
        }
        index++;
    }
    *last_option_p = last_option_index;
    return fchanged;
}

static int parse_options(Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[],
                         SyslogThreadStatus* status,int* last_option_p,bool *unhandled_opt,
                         char* tcl_command) {
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

            if (index == objc-1) {
                missing_option_value(interp,tcl_command,objv[index]);
                return ERROR;
            }

            const char *level_s = Tcl_GetString(objv[++index]);
            int p = level_cli_to_code(interp,level_s);
            if (p == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
                return ERROR;
            }
            status->level = p;
            fchanged++;
            last_option_index = index;

        } else if (strcmp("-facility",argument) == 0) {

            if (index == objc-1) {
                missing_option_value(interp,tcl_command,objv[index]);
                return ERROR;
            }

            const char *facility_s = Tcl_GetString(objv[++index]);
            int f = facility_cli_to_code(interp, facility_s);
            if (f == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown facility specified.",-1));
                return ERROR;
            }
            status->facility = f;
            fchanged++;
            last_option_index = index;

        } else if (strcmp("-format",argument) == 0) {

            if (index == objc-1) {
                missing_option_value(interp,tcl_command,objv[index]);
                return ERROR;
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

        } else if (argument[0] == '-') {
            *unhandled_opt = true;
        } else {
            wrong_arguments_message(interp, 1, objv);
            return ERROR;
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
            bool opened;

            SYSLOG_ATOMIC(opened,g_status->opened)
            Tcl_SetObjResult(interp,Tcl_NewIntObj(opened));
            return TCL_OK;

        } else {
            Tcl_SetErrorCode(interp,"internal_error",cdata,(char *)NULL);
            Tcl_AddErrorInfo(interp,"Internal error: invalid command argument");
            return TCL_ERROR;
        }
    }

    int last_opt_index;
    bool unhandled_open_opt = false;
    int  tcl_exit_status = TCL_OK;

    SYSLOG_MUTEX_LOCK
    if (parse_open_options(interp,objc,objv,true,&last_opt_index,&unhandled_open_opt,"::syslog::open") == ERROR) {
        tcl_exit_status = TCL_ERROR;
        if (unhandled_open_opt) {
            wrong_command_option(interp,objc,objv);
        }
    } else {

        /* 
         * Il comando ::syslog::open ammette solo options e quindi last_opt_index
         * deve essere necessariamente == objc - 1
         */

        if (last_opt_index != objc-1) {
            Tcl_WrongNumArgs(interp,objc,objv,
                "open ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-nodelay? ?-console?");
            return TCL_ERROR;
        }

        SyslogClose();
        SyslogOpen();
    }

    SYSLOG_MUTEX_UNLOCK
    return tcl_exit_status;
}

static int SyslogCloseCmd (ClientData clientData,
                          Tcl_Interp *interp,
                          int objc,Tcl_Obj *CONST86 objv[]) {
    SYSLOG_MUTEX_LOCK

    SyslogClose();

    SYSLOG_MUTEX_UNLOCK
    return TCL_OK;
}

static int SyslogConfigurationCmd (ClientData clientData,
                                    Tcl_Interp *interp,
                                    int objc,Tcl_Obj *CONST86 objv[]) {
    if (objc == 2) {
        char* argument = Tcl_GetString(objv[1]);
        if (strcmp(argument,"-global") == 0) {
            Tcl_Obj* global_info = Tcl_NewObj();
            Tcl_IncrRefCount(global_info);

            SYSLOG_MUTEX_LOCK
            if (g_status->ident != NULL) {
                Tcl_ListObjAppendElement(interp,global_info,Tcl_NewStringObj("-ident",-1));
                Tcl_ListObjAppendElement(interp,global_info,Tcl_NewStringObj(g_status->ident,-1));
            }

            char* facility = facility_code_to_cli(g_status->facility);
            if (facility != NULL) {
                Tcl_ListObjAppendElement(interp,global_info,Tcl_NewStringObj("-facility",-1));
                Tcl_ListObjAppendElement(interp,global_info,Tcl_NewStringObj(facility,-1));
            }

            int opt;
            for (opt = 0; opt < num_syslog_options; opt++)
            {
                if ((g_status->options & opt_code[opt]) != 0)
                {
                    Tcl_ListObjAppendElement(interp,global_info,Tcl_NewStringObj(options[opt],-1));
                }
            }

            Tcl_SetObjResult(interp,global_info);
            SYSLOG_MUTEX_UNLOCK
            Tcl_DecrRefCount(global_info);
            return TCL_OK;
        }
    }

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
     * syslog is called to actually log a message. This command provides compatibility
     * with previous versions of syslog. We fail with unrecognized option error when both
     * parse_options (log command specific option
     */

    int last_arg_opts = 0;
    bool unhandled_log_opt = false;
    if (parse_options(interp,objc, objv, status,&last_arg_opts, &unhandled_log_opt,"syslog") == -1) {
        return TCL_ERROR;
    }

    /* This CLI options are checked here for compatibility but it will be removed
       in new releases requiring the socket to the syslog utility to be explicitly
       reopened with new options */

    SYSLOG_MUTEX_LOCK

    int tcl_exit_code = TCL_OK;
    int last_open_opt = 0;
    bool unhandled_global_opt = false;
    status->open_changed = parse_open_options(interp,objc,objv,false,&last_open_opt,&unhandled_global_opt,"syslog");

    if (unhandled_log_opt && unhandled_global_opt) {
        wrong_command_option(interp,objc,objv);
        return TCL_ERROR;
    }

    if (status->open_changed == -1) {
        tcl_exit_code = TCL_ERROR;
    } else {
        int first_non_opt_arg = MAX(last_open_opt,last_arg_opts) + 1;
        if (first_non_opt_arg == objc-2) {
            Tcl_Obj* level_o = objv[objc-2];

            int level_code = level_cli_to_code(interp,Tcl_GetString(level_o));
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
    bool unhandled_log_opt = false;
    if (parse_options(interp,objc, objv, status,&last_arg_opts, &unhandled_log_opt,"::syslog::log") == -1) {
        return TCL_ERROR;
    } else if (unhandled_log_opt) {
        wrong_command_option(interp,objc,objv);
        return TCL_ERROR;
    }

    int first_non_opt_arg = last_arg_opts + 1;
    SYSLOG_MUTEX_LOCK
    if (first_non_opt_arg == objc-2) {
        Tcl_Obj* level_o = objv[objc-2];

        int level_code = level_cli_to_code(interp,Tcl_GetString(level_o));
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

