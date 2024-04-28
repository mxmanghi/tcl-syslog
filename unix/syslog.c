/*
    syslog - Syslog loggin service for Tcl
    A Tcl interface to the POSIX syslog service.

    Copyright (C) 2008  Alexandros Stergiakis

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

static int SyslogCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
    char *ident = NULL;
    char *facility_s = NULL;
    char *priority_s = NULL;
    char *message = NULL;
    int facility = LOG_USER;
    int priority = LOG_DEBUG;

    int i = 1;
    while (1) {
        if (objc < 2 + ((i%2 == 0) ? (i+1) : i)) {
            Tcl_WrongNumArgs(interp, 1, objv, "?-ident ident? ?-facility facility? priority message");
            return TCL_ERROR;
        }

        if (strcmp("-ident", Tcl_GetString(objv[i])) == 0) {
            ident = Tcl_GetString(objv[++i]);
            i++;
            continue;

        } else if (strcmp("-facility", Tcl_GetString(objv[i])) == 0) {
            facility_s = Tcl_GetString(objv[++i]);
            i++;
            if ((facility = convert_facility(interp, facility_s)) == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Erroneous facility specified.",-1));
                return TCL_ERROR;
            }
            continue;

        } else {
            if (objc > 2 + i) {
                Tcl_WrongNumArgs(interp, 1, objv, "?-ident ident? ?-facility facility? priority message");
                return TCL_ERROR;
            }

            priority_s = Tcl_GetString(objv[i]); 
            if ((priority = convert_priority(interp, priority_s)) == ERROR) {
                Tcl_SetObjResult(interp,Tcl_NewStringObj("Erroneous priority specified.",-1));
                return TCL_ERROR;
            }

            message = Tcl_GetString(objv[i + 1]); 
            break;
        }
    }

    openlog(ident, LOG_NDELAY, facility);
    syslog(priority, "%s", message);
    closelog();

    return TCL_OK;
}

static int convert_facility (Tcl_Interp *interp, const char *facility) {
    static char* facilities[] = { "kern", "user", "mail", "daemon", "auth", "syslog",
                                  "lrp", "news", "uucp", "cron", "authpriv", "ftp",
                                  "local0", "local1", "local2", "local3", "local4",
                                  "local5", "local6", "local7", NULL };
    int index;
    Tcl_Obj *facility_o;

    facility_o = Tcl_NewStringObj(facility, -1);

    /* The Tcl object reference counter must be incremented, because it's decremented
     * before returning, in the first place. Moreover Tcl_GetIndexFromObj could 
     * legitimately increment and later decrement it
     * before returning causing the object_o memory to be released. 
     */

    Tcl_IncrRefCount(facility_o);

    if (Tcl_GetIndexFromObj(interp, facility_o, facilities, "facility", 0, &index) != TCL_OK) {
        Tcl_DecrRefCount(facility_o);
        return ERROR;
    }
    Tcl_DecrRefCount(facility_o);

    return index<<3;
}

static int convert_priority (Tcl_Interp *interp, const char *priority) {
    static char* priorities[] = {"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", NULL};
    int index;
    Tcl_Obj *priority_o;
    priority_o = Tcl_NewStringObj(priority, -1);

    /* see convert_facility */

    Tcl_IncrRefCount(priority_o);

    if (Tcl_GetIndexFromObj(interp, priority_o, priorities, "priority", 0, &index) != TCL_OK) {
        Tcl_DecrRefCount(priority_o);
        return ERROR;
    }
    Tcl_DecrRefCount(priority_o);

    return index;
}
