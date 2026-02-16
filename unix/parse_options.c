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

#include <string.h>
#include <syslog.h>
#include "syslog.h"

extern SyslogGlobalStatus *g_status;
extern char* g_default_format;

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

/*
 * parse_open_options
 *
 */

int parse_open_options(Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[],
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
            Tcl_WrongNumArgs(interp,objc,objv,
            "?open|close|log? ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-level level? message");
            return ERROR;
        }
        index++;
    }
    *last_option_p = last_option_index;
    return fchanged;
}

/*
 * parse_options
 *
 */

int parse_options (Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[],
                   SyslogThreadStatus* status, int* last_option_p, bool *unhandled_opt,
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
        } 
        index++;
    }
    *last_option_p = last_option_index;
    return fchanged;
}

