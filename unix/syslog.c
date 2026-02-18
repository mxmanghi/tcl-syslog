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

#include "syslog.h"
#include "params.h"

static Tcl_ThreadDataKey syslogKey;
static Tcl_Mutex syslogMutex;

/*
 * Function Prototypes
 */

static int SyslogCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int SyslogOpenCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int SyslogCloseCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int SyslogLogmaskCmd (ClientData clientData, Tcl_Interp *interp, int objc,Tcl_Obj *CONST86 objv[]);
static int SyslogConfigureCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int SyslogCGetCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);
static int SyslogLogCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[]);

static void SyslogInitGlobal (void);

extern SyslogGlobalStatus *g_status;
extern char* g_default_format;

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
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::logmask",SyslogLogmaskCmd,(ClientData) NULL,NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::close",SyslogCloseCmd,(ClientData) NULL,NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::configure",SyslogConfigureCmd,(ClientData) "cget",NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::cget",SyslogCGetCmd,(ClientData) "cget",NULL);
    Tcl_CreateObjCommand(interp,SYSLOG_NS"::log",SyslogLogCmd,(ClientData) NULL,NULL);
    Tcl_PkgProvide(interp,PACKAGE_NAME,PACKAGE_VERSION);
    return TCL_OK;
}

int syslog_Init(Tcl_Interp *interp) {
    return Syslog_Init(interp);
}

/*
 * Functions handing errors 
 */

static void wrong_arguments_message (Tcl_Interp* interp,int c,Tcl_Obj *CONST86 objv[],char* command)
{
    char* fixture = "?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-level level? message";
    char  cmd_template[512];

    strcpy(cmd_template,command);
    strcat(cmd_template," ");
    strcat(cmd_template,fixture);

    Tcl_WrongNumArgs(interp,c,objv,cmd_template);
}

static void wrong_command_option(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[],char* command)
{
    Tcl_Obj* error_code_list = Tcl_NewObj();
    Tcl_Obj* command_obj = Tcl_NewStringObj(command,-1);

    Tcl_IncrRefCount(command_obj);
    Tcl_IncrRefCount(error_code_list);

    Tcl_ListObjAppendElement(interp, error_code_list, Tcl_NewStringObj("wrong_arguments", -1));
    Tcl_ListObjAppendElement(interp, error_code_list, Tcl_NewListObj(objc, objv)); 
    Tcl_SetObjErrorCode(interp, error_code_list);

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid ::syslog::open option", -1));

    Tcl_Obj* info_o = Tcl_NewObj();
    Tcl_IncrRefCount(info_o);

    Tcl_AppendStringsToObj(info_o,"\n    (unrecognized option detected while parsing '",command,"' arguments",NULL);
    Tcl_AppendObjToErrorInfo(interp,info_o);

    Tcl_DecrRefCount(info_o);
    Tcl_DecrRefCount(error_code_list);
    Tcl_DecrRefCount(command_obj);
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

static inline void log_message (SyslogThreadStatus* status) {

    if (status->open_changed > 0) {
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
    int last_opt_index;
    bool unhandled_open_opt = false;
    int  tcl_exit_status = TCL_OK;

    SYSLOG_MUTEX_LOCK
    if (parse_open_options(interp,objc,objv,true,&last_opt_index,&unhandled_open_opt,"::syslog::open") == ERROR) {
        tcl_exit_status = TCL_ERROR;
        if (unhandled_open_opt) {
            wrong_command_option(interp,objc,objv,"::syslog::open");
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

static int SyslogConfigureCmd (ClientData clientData,
                                    Tcl_Interp *interp,
                                    int objc,Tcl_Obj *CONST86 objv[]) {
    SyslogThreadStatus *status = get_thread_status();
    int last_arg_opts = 0;
    bool unhandled_log_opt = false;
    if (parse_options (interp,objc, objv, status, &last_arg_opts,
                      &unhandled_log_opt,"::syslog::configure") == ERROR) {
        return TCL_ERROR;
    }
    
    SYSLOG_MUTEX_LOCK

    int last_open_opt = 0;
    bool unhandled_global_opt = false;
    status->open_changed = parse_open_options (interp,objc,objv,false,
                                              &last_open_opt,&unhandled_global_opt,"::syslog::configure");

    if (unhandled_log_opt && unhandled_global_opt) {
        wrong_command_option(interp,objc,objv,"::syslog::configure");
        return TCL_ERROR;
    }

    if (status->open_changed > 0) {
        SyslogClose();
        SyslogOpen();
    }

    SYSLOG_MUTEX_UNLOCK
    return TCL_OK;
}

static int SyslogCGetCmd (ClientData clientData,
                                    Tcl_Interp *interp,
                                    int objc,Tcl_Obj *CONST86 objv[]) {
    if (objc == 2) {
        char* argument = Tcl_GetString(objv[1]);
        if (strcmp(argument,"-global") == 0) {
            Tcl_Obj* global_conf = Tcl_NewObj();
            Tcl_IncrRefCount(global_conf);

            SYSLOG_MUTEX_LOCK
            if (g_status->ident != NULL) {
                Tcl_ListObjAppendElement(interp,global_conf,Tcl_NewStringObj("-ident",-1));
                Tcl_ListObjAppendElement(interp,global_conf,Tcl_NewStringObj(g_status->ident,-1));
            }

            char* facility = facility_code_to_cli(g_status->facility);
            if (facility != NULL) {
                Tcl_ListObjAppendElement(interp,global_conf,Tcl_NewStringObj("-facility",-1));
                Tcl_ListObjAppendElement(interp,global_conf,Tcl_NewStringObj(facility,-1));
            }

            int opt;
            for (opt = 0; opt < num_syslog_options; opt++)
            {
                extern int* opt_code;
                extern const char** options;
                if ((g_status->options & opt_code[opt]) != 0)
                {
                    Tcl_ListObjAppendElement(interp,global_conf,Tcl_NewStringObj(options[opt],-1));
                }
            }

            Tcl_SetObjResult(interp,global_conf);
            SYSLOG_MUTEX_UNLOCK
            Tcl_DecrRefCount(global_conf);
            return TCL_OK;
        } 
    }

    SyslogThreadStatus *status = (SyslogThreadStatus *) Tcl_GetThreadData(&syslogKey, sizeof(SyslogThreadStatus));
    Tcl_Obj* configuration = Tcl_NewObj();
    Tcl_IncrRefCount(configuration);

    Tcl_ListObjAppendElement(interp,configuration,Tcl_NewStringObj("-format",-1));
    Tcl_ListObjAppendElement(interp,configuration,Tcl_NewStringObj(status->format,-1));
    Tcl_ListObjAppendElement(interp,configuration,Tcl_NewStringObj("-level",-1));
    Tcl_ListObjAppendElement(interp,configuration,Tcl_NewStringObj(level_code_to_cli(status->level),-1));
    if (status->facility >= 0) {
        Tcl_ListObjAppendElement(interp,configuration,Tcl_NewStringObj("-facility",-1));
        Tcl_ListObjAppendElement(interp,configuration,Tcl_NewStringObj(facility_code_to_cli(status->facility),-1));
    }

    Tcl_SetObjResult(interp,configuration);
    Tcl_DecrRefCount(configuration);
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
        wrong_arguments_message(interp, 1, objv,"syslog");
        return TCL_ERROR;
    }

    /*
     * syslog is called to actually log a message. This command provides compatibility
     * with previous versions of syslog. We fail with unrecognized option error when both
     * parse_options (log command specific option
     */

    int last_arg_opts = 0;
    bool unhandled_log_opt = false;
    if (parse_options(interp,objc, objv, status, &last_arg_opts, &unhandled_log_opt,"syslog") == ERROR) {
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

    int first_non_opt_arg = MAX(last_open_opt,last_arg_opts) + 1;
    if (first_non_opt_arg == objc-2) {
        Tcl_Obj* level_o = objv[objc-2];

        int level_code = level_cli_to_code(interp,Tcl_GetString(level_o));
        if (level_code == ERROR) {
            Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
            tcl_exit_code = TCL_ERROR;
        }
        status->level = level_code;
        status->message = Tcl_GetString(objv[objc-1]);
        log_message(status);
    } else if (first_non_opt_arg == objc-1) {
        status->message = Tcl_GetString(objv[objc-1]);
        log_message(status);
    }

    SYSLOG_MUTEX_UNLOCK
    return tcl_exit_code;
}

static int SyslogLogCmd (ClientData clientData,
                          Tcl_Interp *interp,
                          int objc,Tcl_Obj *CONST86 objv[]) {
    SyslogThreadStatus* status  = get_thread_status();

    /* we don't change open options in ::syslog::log */

    status->open_changed = false;

    /* We repeat what we do in SyslogCmd but 
     * skip the processing of the open specific
     * options. This command is only for emitting
     * log messages
     */

    if (objc < 2) {
        wrong_arguments_message(interp, 1, objv,"::syslog::log");
        return TCL_ERROR;
    }

    int last_arg_opts = 0;
    bool unhandled_log_opt = false;
    if (parse_options(interp,objc, objv, status,&last_arg_opts, &unhandled_log_opt,"::syslog::log") == ERROR) {
        return TCL_ERROR;
    } else if (unhandled_log_opt) {
        wrong_command_option(interp,objc,objv,"::syslog::log");
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
        log_message(status);
    } else if (first_non_opt_arg == objc-1) {
        status->message = Tcl_GetString(objv[objc-1]);
        log_message(status);
    }
    SYSLOG_MUTEX_UNLOCK
    return TCL_OK;
}

