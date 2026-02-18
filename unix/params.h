/*
 *    params.h - syslog command options definitions
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

#ifndef __params_h__
#define __params_h__

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

#define SYSLOG_LEVELS(X) \
	X("emergency",LOG_EMERG,log_emerg_idx,0) \
	X("alert",LOG_ALERT,log_alert_idx,1) \
	X("critical",LOG_CRIT,log_crit_idx,2) \
	X("error",LOG_ERR,log_err_idx,3) \
	X("warning",LOG_WARNING,log_warning_idx,4) \
	X("notice",LOG_NOTICE,log_notice_idx,5) \
	X("info",LOG_INFO,log_info_idx,6) \
	X("debug",LOG_DEBUG,log_debug_idx,7)

#define SYSLOG_OPTIONS(X) \
    X("-pid",LOG_PID,log_pid_idx) \
    X("-perror",LOG_PERROR,log_perror_idx) \
    X("-console",LOG_CONS,log_cons_idx) \
    X("-nodelay",LOG_NDELAY,log_ndelay_idx)

/* these enums just provide a way to count how many
 * elements for each parameter exist
 */

enum SyslogOptions {
#define SYSLOG_OPTIONS_IDX(option,optcode,option_idx) option_idx,
    SYSLOG_OPTIONS(SYSLOG_OPTIONS_IDX)
    num_syslog_options
};

enum SyslogLevelsIndices {
#define SYSLOG_LEVEL_IDX(level,level_code,level_idx,n) level_idx = n,
    SYSLOG_LEVELS(SYSLOG_LEVEL_IDX)
    num_syslog_levels
};

enum SyslogFacilitiesIndices {
#define SYSLOG_FAC_IDX(facility,facility_code,facility_idx,n) facility_idx = n,
    SYSLOG_FACILITIES(SYSLOG_FAC_IDX)
    num_syslog_facilities
};

#endif /* __params_h__ */
