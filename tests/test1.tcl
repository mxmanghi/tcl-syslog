#!/usr/bin/tclsh
package require syslog

puts Loaded

syslog critical "Message 1"
syslog -ident tclsh warning "Message 2"
syslog -severity local0 notice "Message 3"
syslog -ident tclsh -severity user error "Message 4"

if {! [catch {syslog -ident tclsh -severity user3 error "Message"}]} {
    error
}

if {! [catch {syslog -ident tclsh -severity user error3 "Message"}]} {
    error
}

puts OK
