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
#include "params.h"

extern SyslogGlobalStatus *g_status;
extern char* g_default_format;
extern int opt_class[];
extern int opt_code[];
extern const char* options[];

static void missing_option_value (Tcl_Interp* interp,char* cmd,Tcl_Obj* option)
{
    Tcl_Obj* error_code_list = Tcl_NewObj();
    Tcl_IncrRefCount(error_code_list);

    Tcl_ListObjAppendElement(interp, error_code_list, Tcl_NewStringObj("missing_argument_value", -1));
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
 * parse_options
 *
 */

int parse_options (Tcl_Interp *interp, int objc, Tcl_Obj *CONST86 objv[],ParseArgsOptions* pao)
{
    int  index    = 1;
    int  fchanged = 0;
    int  option_idx;
    char* tcl_command = Tcl_GetString(objv[0]);

    pao->status->facility = -1;
    if (pao->status->format != g_default_format) {
        Tcl_Free(pao->status->format);
        pao->status->format = (char *) g_default_format;
    }

    while (index < objc) {

        /* Read the docs! Tcl_GetIndexFromObj stores an 
         * error in the interpreter if a the search
         * string is not found in the string array.
         * Setting it to NULL avoids this sid 
         */

        if (Tcl_GetIndexFromObj(NULL,objv[index],options,"option",TCL_EXACT,&option_idx) == TCL_ERROR) {
            const char* argument = Tcl_GetString(objv[index]);

            if (strcmp(argument,"--") == 0) {
                /* end of options seqeunce, we proceed with the follwing opt */
                if (index == objc-1) {
                    missing_option_value(interp,tcl_command,objv[index]);
                    return ERROR;
                }
                return fchanged;
            } else if (argument[0] == '-') {
                pao->unhandled_opt_index = index;
                return INVALID_OPTION;
            } else {

                /* we hit a log message or log facility argument */

                return fchanged;
            }
            
        }

        if ((opt_class[option_idx] & pao->option_class) == 0) {
            pao->unhandled_opt_index = index;
            return INVALID_OPTION_CLASS;
        }

        pao->modified_opt_class |= opt_class[option_idx];

        switch (option_idx) {
            case ident_idx:
            {
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
                pao->last_option_index = index;
                break;
            }
            case log_ndelay_idx:
            {
                g_status->options = g_status->options | LOG_NDELAY;
                fchanged++;
                pao->last_option_index = index;
                break;
            }
            case log_console_idx:
            {
                g_status->options = g_status->options | LOG_CONS;
                fchanged++;
                pao->last_option_index = index;
                break;
            }
            case log_pid_idx:
            {
                g_status->options = g_status->options | LOG_PID;
                fchanged++;
                pao->last_option_index = index;
                break;
            }
            case log_perror_idx:
            {
                g_status->options = g_status->options | LOG_PERROR;
                fchanged++;
                pao->last_option_index = index;
                break;
            }
            case priority_idx:
            case level_idx:
            {
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
                pao->status->level = p;
                fchanged++;
                pao->last_option_index = index;
                break;
            }
            case facility_idx:
            {
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
                pao->status->facility = f;
                fchanged++;
                pao->last_option_index = index;
                break;
            }   
            case format_idx:
            {
                if (index == objc-1) {
                    missing_option_value(interp,tcl_command,objv[index]);
                    return ERROR;
                }

                const char *format_s = Tcl_GetString(objv[++index]);
                size_t len = strlen(format_s);
                char *copy = (char *) Tcl_Alloc(len + 1);
                memcpy(copy,format_s,len + 1);
                if ((pao->status->format != NULL) && (pao->status->format != g_default_format)) {
                    Tcl_Free(pao->status->format);
                }
                pao->status->format = copy;
                fchanged++;
                pao->last_option_index = index;
                break;
            }
        }
        index++;
    }
    return fchanged;
}
