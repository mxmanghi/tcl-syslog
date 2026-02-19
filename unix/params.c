/*
 *    params.c - syslog command options definitions
 *
 *    A Tcl interface to the POSIX syslog service.
 *
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

#include <stddef.h>
#include <syslog.h>
#include <tcl.h>

#include "syslog.h"
#include "params.h"


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

int facility_cli_to_code (Tcl_Interp *interp, const char *facility) {
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

char* facility_code_to_cli (int code) {
    int index;
    for (index = 0; index < num_syslog_facilities; index++) {
        if (facility_code[index] == code) { return facilities[index]; }
    }
    return NULL;
}

/* Facility and level mapping */

int level_cli_to_code (Tcl_Interp *interp, const char *level) {
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

char* level_code_to_cli (int code) {
    int index;
    for (index = 0; index < num_syslog_levels; index++) {
        if (level_code[index] == code) { return levels[index]; }
    }
    return NULL;
}

