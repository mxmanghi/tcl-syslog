#!/usr/bin/tclsh9.0

#    Copyright (C) 2026 Massimo Manghi <mxmanghi@apache.org>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

package require fileutil

set auto_path [concat "." $auto_path]

set version [string trim [::fileutil::cat [file join .. VERSION]]]

package require tcltest
#package require syslog

load ../libtcl9syslog${version}.so

source [file join [file dirname [info script]] harness.tcl]

set hasSyslogWatcher 0
if {![catch {::syslogtest::harness::start 7000} startErr]} {
    set hasSyslogWatcher 1
}
::tcltest::testConstraint hasSyslogWatcher $hasSyslogWatcher

set base "TCLTEST-SYSLOG-[pid]-[clock milliseconds]"

# Template for future tests:
# 1. Emit a unique message through syslog.
# 2. Wait using wait_for_response with an explicit timeout.\\
# 3. Assert against dict fields: raw, payload, timestamp_kind.

::tcltest::configure -singleproc 1 -testdir [file dirname [file normalize [info script]]] {*}$argv
::tcltest::runAllTests

if {$hasSyslogWatcher} {
    ::syslogtest::harness::stop
}
::tcltest::cleanupTests
