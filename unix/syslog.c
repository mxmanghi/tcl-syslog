/*
 *    syslog - Syslog loggin service for Tcl
 *    A Tcl interface to the POSIX syslog service.
 *
 *    Copyright (C) 2008  Alexandros Stergiakis
 *    Copyright 2024 Massimo Manghi <mxmanghi@apache.org>
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
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * syslog.c
 *
 */

#include <string.h> // strcmp
#include <syslog.h>
#include <tcl.h>

#define ERROR -1

/*
 * Function Prototypes
 */

static int SyslogCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

static int convert_facility(Tcl_Interp *interp, const char *facility);

static int convert_priority(Tcl_Interp *interp, const char *priority);

/*
 * Function Bodies
 */

int Syslog_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp,PACKAGE_NAME,SyslogCmd,(ClientData) NULL,NULL);

    Tcl_PkgProvide(interp,PACKAGE_NAME,PACKAGE_VERSION);
    return TCL_OK;
}

static void wrong_arguments_message (Tcl_Interp* interp,int c,Tcl_Obj *CONST86 objv[])
{
    Tcl_WrongNumArgs(interp, c, objv, "?-ident ident? ?-facility facility? ?-pid? ?-perror? priority message");
}


static int SyslogCmd(ClientData clientData,Tcl_Interp *interp,int objc,Tcl_Obj *CONST86 objv[]) {
    const char* ident       = NULL;
    const char* facility_s  = NULL;
    const char* priority_s  = NULL;
    const char* message     = NULL;
    int   facility          = LOG_USER;
    int   priority          = LOG_DEBUG;
    int   option            = LOG_NDELAY;
    const char* argument    = NULL;

    /*  
     *  having less than 2 arguments to 'syslog' is wrong 
     *  anyway and we print the usual error message about
     *  the command usage
     */

    if (objc < 3) {
        wrong_arguments_message(interp, 1, objv);
        return TCL_ERROR;
    }

    int i = 0;
    while (++i < objc) {

        argument = Tcl_GetString(objv[i]);

        if (strcmp("-ident",argument) == 0) {

            /* an end of argument list condition marks an incomplete command */
            if (objc == i+1) {
                wrong_arguments_message(interp, 1, objv);
                return TCL_ERROR;
            }

            ident = Tcl_GetString(objv[++i]);

        } else if (strcmp("-facility",argument) == 0) {

            if (objc == i+1) {
                wrong_arguments_message(interp, 1, objv);
                return TCL_ERROR;
            }

            facility_s = Tcl_GetString(objv[++i]);
            if ((facility = convert_facility(interp, facility_s)) == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown facility specified.",-1));
                return TCL_ERROR;
            }

        } else if (strcmp("-pid",argument) == 0) {

            option = option | LOG_PID;

        } else if (strcmp("-perror",argument) == 0) {

            option = option | LOG_PERROR;

        } else {

            /* checking whether we have extra non necessary arguments */

            if (objc > 2 + i) {
                wrong_arguments_message(interp, 1, objv);
                return TCL_ERROR;
            }

            priority_s = argument; 
            if ((priority = convert_priority(interp, priority_s)) == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Unknown priority specified.",-1));
                return TCL_ERROR;
            }

            message = Tcl_GetString(objv[++i]); 
            break;
        }
    }

    openlog(ident,option,facility);
    syslog(priority,"%s",message);
    closelog();

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
