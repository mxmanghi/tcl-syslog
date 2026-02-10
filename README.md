# tcl-syslog

Simple Tcl interface to the Unix syslog facility

    package require syslog

    syslog -facility my_application info "...an informational message to be sent to syslog"


This package was originally written in 2008 Alexandros Stergiakis and adopted in 2024 by
Massimo Manghi since the author is unresponsive at the email declared in the documentation.

See COPYING for a legal notice recognizing the original authorship

```
# tcl-syslog(n) - Syslog interface for Tcl

Tcl-Extensions, 1.2.5

package require syslog 
 syslog ?-ident ident? ?-facility facility? ?-pid? ?-perror? priority message
Returns: nothing

# Description

This project provides a Tcl interface to the standard *nix syslog service. It implements a Tcl package that exports the full functionality of the underlying syslog facility to the Tcl programming language. This includes local and remote logging.

Syslog is a standard for forwarding log messages in an IP network. It is also used to refer to the implementation of this standard and API, that supports both remote and local logging. It is typically used for computer system management and security auditing; usually to aggregate log messages in a central repository. It is standardized within the Syslog working group of the IETF.

Syslog is available in all POSIX compliant and POSIX-like Operating Systems.

_-ident_ is a optional string argument that is used by syslog to 
differentiate between processes and log contexts. It is up to the user 
to specify any string here.

_-pid_ prints also the process pid in the log message

_-perror_ prints the message also to stderr

_-facility_ is an optional string dictated by syslog, and categorizes
the entity that logs the message, in the following categories/facilities:  
           Facility    Description
           auth        security/authorization messages
           authpriv    security/authorization messages (private)
           cron        clock daemon (cron and at)
           daemon      system daemons without separate facility value
           ftp         ftp daemon
           kern        kernel messages
           local[0-7]  local0 to local7 are reserved for local use
           lpr         line printer subsystem
           mail        mail subsystem
           news        USENET news subsystem
           syslog      messages generated internally by syslogd(8)
           user        generic user-level messages
           uucp        UUCP subsystem
    
    The default facility is "user".
    
    priority is another string dictated by syslog, that describes
    the severity of the message. In order of higher to lower severity, the
    possible strings are:  

           Priority    Description
           emergency   system is unusable
           alert       action must be taken immediately
           critical    critical conditions
           error       error conditions
           warning     warning conditions
           notice      normal, but significant, condition
           info        informational message
           debug       debug-level message
    
    This argument is mandatory.

#   EXAMPLE

    package require syslog
    
    syslog critical "Message 1"
    syslog -ident tclsh warning "Message 2"
    syslog -facility local0 notice "Message 3"
    syslog -ident tclsh -facility user error "Message 4"
    syslog -pid critical "Message 5.0"
    syslog -pid -ident tclsh critical "Message 5.1"
    syslog -pid -facility local0 debug "Message 5.1"
    syslog -pid -facility local1 -ident tclsh info "Message 5.2"
    syslog -pid -ident tclsh -facility local2 warning "Message 5.3"
    
    Outputs in /var/log/syslog (syslog timestamps and line prefix removed) :
    
     tclsh: Message 1
     tclsh: Message 2
     tclsh: Message 3
     testident: Message 4
     tclsh[7049]: Message 5.0
     my_ident[7049]: Message 5.1
     tclsh[7049]: Message 5.1
     my_ident[7049]: Message 5.2
     my_ident[7049]: Message 5.3

#   DOCUMENTATION

    The package provides the *tcl-syslog* man page.

#   AUTHORS

    Alexandros Stergiakis sterg@kth.se  
    Massimo Manghi mxmanghi@apache.org
#   COPYRIGHT
    
    Copyright (C) 2008  Alexandros Stergiakis  
    Copyright (C) 2024  Massimo Manghi
    
    This program is free software: you can redistribute it and/or   
    modify it under the terms of the GNU General Public License as   
    published by the Free Software Foundation, either version 3 of   
    the License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,  
    but WITHOUT ANY WARRANTY; without even the implied warranty of  
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License  
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
```
