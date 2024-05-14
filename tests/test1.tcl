#!/usr/bin/tclsh
#
# basic tests. Run this script from tcl-syslog root
#
# tclsh ./tests/test1.tcl
#
# 

lappend auto_path .

package require syslog

puts Loaded

syslog critical "Message 1"
syslog -ident tclsh warning "Message 2"
syslog -facility local0 notice "Message 3"
syslog -ident testident -facility user error "Message 4"
syslog -pid critical "Message 5.0"
syslog -pid -ident my_ident critical "Message 5.1"
syslog -pid -facility local0 debug "Message 5.1"
syslog -pid -facility local1 -ident my_ident info "Message 5.2"
syslog -pid -ident my_ident -facility local2 warning "Message 5.3"

puts OK
