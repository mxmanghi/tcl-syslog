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

static void init_parse_options(ParseArgsOptions* pao)
{
    pao->status = get_thread_status();
    pao->last_option_index = 0;
    pao->unhandled_opt_index = 0;
    pao->option_class = ALL_OPTION_CLASSES;
    pao->modified_opt_class = 0;
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

static void wrong_command_option(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[],int cli_option_idx)
{
    Tcl_Obj* error_code_list = Tcl_NewObj();
    const char* command = Tcl_GetString(objv[0]);

    Tcl_IncrRefCount(error_code_list);

    Tcl_ListObjAppendElement(interp, error_code_list, Tcl_NewStringObj("invalid_option", -1));
    Tcl_ListObjAppendElement(interp, error_code_list, Tcl_NewListObj(objc, objv)); 
    Tcl_SetObjErrorCode(interp, error_code_list);

    Tcl_Obj* error_message = Tcl_NewStringObj("Invalid option '", -1);
    Tcl_IncrRefCount(error_message);
    Tcl_AppendStringsToObj(error_message,Tcl_GetString(objv[cli_option_idx]),"'",NULL);
    Tcl_SetObjResult(interp,error_message);
    Tcl_DecrRefCount(error_message);

    Tcl_Obj* info_o = Tcl_NewObj();
    Tcl_IncrRefCount(info_o);

    Tcl_AppendStringsToObj(info_o,"\n    (unrecognized option detected while parsing '",command,"' arguments",NULL);
    Tcl_AppendObjToErrorInfo(interp,info_o);

    Tcl_DecrRefCount(info_o);
    Tcl_DecrRefCount(error_code_list);
    return;
}

static void SyslogOpen(void)
{
    if (!g_status->opened) {
        SYSLOG_DEBUG_MSG("Calling openlog")
        openlog(g_status->ident,g_status->options,g_status->facility);
        g_status->opened = true;
    }
}

static void SyslogClose(void)
{
    if (g_status->opened) {
        SYSLOG_DEBUG_MSG("Calling closelog")
        closelog();
        g_status->opened = false;
    }
}

static inline void log_message (SyslogThreadStatus* status) {
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
    int tcl_exit_status = TCL_OK;
    ParseArgsOptions pao;

    init_parse_options(&pao);

    SYSLOG_MUTEX_LOCK
    int parse_result = parse_options (interp,objc,objv,&pao);
    switch (parse_result) {
        case ERROR:
        {
            tcl_exit_status = TCL_ERROR;
            break;
        }
        case INVALID_OPTION:
        case INVALID_OPTION_CLASS:
        {
            wrong_command_option(interp,objc,objv,pao.unhandled_opt_index);
            break;
        }
        default:
        {
            /* 
             * Il comando ::syslog::open ammette solo options e quindi last_opt_index
             * deve essere necessariamente == objc - 1
             */

            if (pao.last_option_index != objc-1) {
                Tcl_WrongNumArgs(interp,objc,objv,
                    "open ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-nodelay? ?-console?");
                tcl_exit_status = TCL_ERROR;
            } else {
                SyslogClose();
                SyslogOpen();
            }
        }
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
    ParseArgsOptions pao;
    init_parse_options(&pao);

    int tcl_exit_status = TCL_OK;   
    SYSLOG_MUTEX_LOCK

    int opt_changed = parse_options (interp,objc,objv,&pao);
    if (opt_changed == ERROR) {
        tcl_exit_status = TCL_ERROR;
    } else if (pao.unhandled_opt_index > 0) {
        wrong_command_option(interp,objc,objv,pao.unhandled_opt_index);
        tcl_exit_status = TCL_ERROR;
    } else if (pao.modified_opt_class & GLOBAL_OPTION_CLASS) {
        SyslogClose();
        SyslogOpen();
    }

    SYSLOG_MUTEX_UNLOCK
    return tcl_exit_status;
}

static int SyslogCGetCmd (ClientData clientData,
                                    Tcl_Interp *interp,
                                    int objc,Tcl_Obj *CONST86 objv[]) {
    if (objc == 2) {
        extern int opt_code[];
        extern const char* options[];
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
                if (opt_code[opt] == NOOPT) { continue; }
                if ((g_status->options & opt_code[opt]) != 0)
                {
                    Tcl_ListObjAppendElement(interp,global_conf,Tcl_NewStringObj(options[opt],-1));
                }
            }

            Tcl_SetObjResult(interp,global_conf);
            SYSLOG_MUTEX_UNLOCK
            Tcl_DecrRefCount(global_conf);
            return TCL_OK;
        } else {
            wrong_command_option(interp,objc,objv,1);
            return TCL_ERROR;
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
    ParseArgsOptions pao;

    init_parse_options(&pao);

    /*  
     *  having less than 2 arguments to 'syslog' is wrong 
     *  anyway and we print the usual error message about
     *  the command usage. 
     */

    if (objc < 2) {
        wrong_arguments_message(interp,1, objv,"syslog");
        return TCL_ERROR;
    }

    /*
     * syslog is called to actually log a message. This command provides compatibility
     * with previous versions of syslog. We fail with unrecognized option error when both
     * parse_options (log command specific option
     */

    /* This CLI options are checked here for compatibility but it will be removed
       in new releases requiring the socket to the syslog utility to be explicitly
       reopened with new options */

    SYSLOG_MUTEX_LOCK

    int tcl_exit_code = TCL_OK;
    int parse_results = parse_options(interp,objc,objv,&pao);
    if (parse_results == ERROR) {
        tcl_exit_code = TCL_ERROR;
    } else if (pao.unhandled_opt_index) {
        wrong_command_option(interp,objc,objv,pao.unhandled_opt_index);
        tcl_exit_code = TCL_ERROR;
    } else {
        if (pao.modified_opt_class & GLOBAL_OPTION_CLASS) {
            SyslogClose();
            SyslogOpen();
        } 

        int first_non_opt_arg = pao.last_option_index + 1;
        if (first_non_opt_arg == objc-2) {
            Tcl_Obj* level_o = objv[objc-2];

            int level_code = level_cli_to_code(interp,Tcl_GetString(level_o));
            if (level_code == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
                tcl_exit_code = TCL_ERROR;
            }
            pao.status->level = level_code;
            pao.status->message = Tcl_GetString(objv[objc-1]);
            log_message(pao.status);
        } else if (first_non_opt_arg == objc-1) {
            pao.status->message = Tcl_GetString(objv[objc-1]);
            log_message(pao.status);
        }
    }

    SYSLOG_MUTEX_UNLOCK
    return tcl_exit_code;
}

static int SyslogLogCmd (ClientData clientData,
                          Tcl_Interp *interp,
                          int objc,Tcl_Obj *CONST86 objv[]) {
    ParseArgsOptions pao;

    init_parse_options(&pao);

    /* We repeat what we do in SyslogCmd but 
     * skip the processing of the open specific
     * options. This command is only for emitting
     * log messages
     */

    if (objc < 2) {
        wrong_arguments_message(interp, 1, objv,"::syslog::log");
        return TCL_ERROR;
    }

    int parse_result = parse_options(interp,objc, objv, &pao);
    if (parse_result == ERROR) {
        return TCL_ERROR;
    } else if (pao.unhandled_opt_index > 0) {
        wrong_command_option(interp,objc,objv,pao.unhandled_opt_index);
        return TCL_ERROR;
    } else if (pao.modified_opt_class & GLOBAL_OPTION_CLASS) {
        wrong_command_option(interp,objc,objv,pao.unhandled_opt_index);
        return TCL_ERROR;
    }

    int first_non_opt_arg = pao.last_option_index + 1;
    SYSLOG_MUTEX_LOCK
    if (first_non_opt_arg == objc-2) {
        Tcl_Obj* level_o = objv[objc-2];

        int level_code = level_cli_to_code(interp,Tcl_GetString(level_o));
        if (level_code == ERROR) {
            Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown level specified.",-1));
            return TCL_ERROR;
        }
        pao.status->level = level_code;
        pao.status->message = Tcl_GetString(objv[objc-1]);
        log_message(pao.status);
    } else if (first_non_opt_arg == objc-1) {
        pao.status->message = Tcl_GetString(objv[objc-1]);
        log_message(pao.status);
    }
    SYSLOG_MUTEX_UNLOCK
    return TCL_OK;
}

