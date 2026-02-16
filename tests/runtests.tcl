#!/usr/bin/tclsh9.0

set auto_path [concat "." $auto_path]

package require tcltest
package require syslog

source [file join [file dirname [info script]] harness.tcl]

set hasSyslogWatcher 0
if {![catch {::syslogtest::harness::start 7000} startErr]} {
    set hasSyslogWatcher 1
}
::tcltest::testConstraint hasSyslogWatcher $hasSyslogWatcher

set base "TCLTEST-SYSLOG-[pid]-[clock milliseconds]"

::tcltest::test syslog-template-1.0 {literal payload roundtrip via test server} \
    -constraints hasSyslogWatcher \
    -body {
        set msg "${::base}-literal"
        syslog -pid -ident test_ident -facility local2 critical $msg
        set hit [::syslogtest::harness::waitForLiteral $msg 8000]
        expr {[dict get $hit payload] eq $msg}
    } -result 1

::tcltest::test syslog-template-1.1 {regexp payload match via test server} \
    -constraints hasSyslogWatcher \
    -body {
        set msg "${::base}-regexp seq=42"
        syslog -pid -ident test_ident -facility local2 notice $msg
        set hit [::syslogtest::harness::waitForRegexp {seq=[0-9]+} 8000]
        regexp -- {seq=42} [dict get $hit payload]
    } -result 1

# Template for future tests:
# 1. Emit a unique message through syslog.
# 2. Wait using waitForLiteral/waitForRegexp with an explicit timeout.\\
# 3. Assert against dict fields: raw, payload, timestamp_kind.

if {$hasSyslogWatcher} {
    ::syslogtest::harness::stop
}

::tcltest::cleanupTests
