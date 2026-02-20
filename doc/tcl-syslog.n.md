title: tcl-syslog
section: n
date: 1.3.0
source: Tcl-Extensions

# NAME

tcl-syslog - Syslog interface for Tcl

# SYNOPSIS

```tcl
package require syslog

::syslog::open ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-console? ?-nodelay?
::syslog::close
::syslog::log ?-level level? ?-priority level? ?-facility facility? ?-format message_format? message
::syslog::configure ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-console? ?-nodelay?
::syslog::configure ?-level level? ?-priority level? ?-facility facility? ?-format message_format?
::syslog::cget
::syslog::cget -global

syslog ?-ident ident? ?-facility facility? ?-pid? ?-perror? ?-console? ?-nodelay? ?-level level? message
```

# DESCRIPTION

Syslog is a standard for forwarding log messages to local and other logging
services available on a TCP/IP network. It is typically used for computer
system management and security auditing; usually to aggregate log messages in a
central repository. It is standardized within the Syslog working group of the
IETF. Syslog is available in all POSIX compliant and POSIX-like Operating
Systems.

This package provides a Tcl interface to the standard *syslog* service. It
creates a Tcl namespace with commands for opening and closing the connection to
syslogd, sending messages to the syslog service, and querying configuration.

The current version also provides the global namespace command *syslog* for
compatibility with earlier versions of the package. It is deprecated and kept
only for backward compatibility.

# COMMANDS

## ::syslog::open

Establish the process-wide connection to the syslog facility (calls *openlog*).
This command should be called once per process (typically at startup). Options
set here are global and shared among threads.

Options:

- `-ident` *ident*  
  Optional tag used by syslog to identify the process or log context.

- `-pid`  
  Include the process ID in each syslog message.

- `-perror`  
  Print the message also to stderr.

- `-console`  
  Write directly to the system console if syslogd is unavailable.

- `-nodelay`  
  Open the connection immediately instead of waiting for the first message.

- `-facility` *facility*  
  Set the default facility for this process. Valid facilities are:

```
Facility    Description
auth        security/authorization messages
authpriv    security/authorization messages (private)
cron        clock daemon (cron and at)
daemon      system daemons without separate facility value
ftp         ftp daemon
kern        kernel messages
local0      local use (0)
local1      local use (1)
local2      local use (2)
local3      local use (3)
local4      local use (4)
local5      local use (5)
local6      local use (6)
local7      local use (7)
lpr         line printer subsystem
mail        mail subsystem
news        USENET news subsystem
syslog      messages generated internally by syslogd(8)
user        generic user-level messages
uucp        UUCP subsystem
```

The default facility is `user`.

## ::syslog::close

Close the process-wide connection to syslog (calls *closelog*). It is safe to
call this multiple times, but unwise if the code is supposed to log more
lines. It will work, but it comes at the cost of re-establishing the connection
to the syslog service

## ::syslog::log

Send a log message. Options set here are per-thread (interpreter-local) and
persist across subsequent calls in the same thread.

Options:

- `-level` *level*  
  Describe the severity of the message. Valid levels are:

```
Priority    Description
emergency   system is unusable
alert       action must be taken immediately
critical    critical conditions
error       error conditions
warning     warning conditions
notice      normal, but significant, condition
info        informational message
debug       debug-level message
```

The default level is `info`.

- `-priority`  
  Synonym for `-level`.

- `-format` *message_format*  
  Limited support for syslog formatting. The format string must contain `%s`
  where the message will be substituted.

- `-facility` *facility*  
  Override the global facility for this thread only.

## ::syslog::configure

Set configuration options without emitting a message. This command accepts both
global options (same as `::syslog::open`) and per-thread options (same as
`::syslog::log`). If a global option changes, the process-wide connection is
reopened.

## ::syslog::cget

Return the current configuration.

- With no arguments, returns the per-thread options as a Tcl list of
  `-option value` pairs.
- With `-global`, returns the process-wide options (global state) as a Tcl list.

Option lists returned by *::syslog::cget* have a form suitable to be passed as
as arguments to *::syslog::configure* and are equivalent to storing the
configuration for later use.

# RATIONALE

The syslog connection established by *openlog* is process-wide, while per-thread
settings (level, facility override, format) should be isolated in each thread.
The separation between `::syslog::open` and `::syslog::log` mirrors this design:

- `::syslog::open` manages the process-level connection and options that should
  be set once and shared.
- `::syslog::log` manages per-thread options that can differ across interpreters.

This avoids the overhead of reopening the syslog connection for every log call,
while still allowing per-thread customization.

# EXAMPLES

```tcl
package require syslog

::syslog::open -ident myapp -facility local0 -pid
::syslog::log -level info "An info message..."
# equivalently
::syslog::log info "An info message...."
::syslog::log -facility user info "Info message 2...."
# subsequent calls preserve thread-local options
::syslog::log "Info message 3...."
```

# COMPATIBILITY

The global space command *syslog* is provided for compatibility but it is
deprecated because it can reopen *openlog* implicitly. This command accepts
both global options and per-thread options, but it should be replaced by
explicit `::syslog::open` and `::syslog::log` calls.

```tcl
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
```

Outputs in `/var/log/syslog` (syslog timestamps and line prefix removed):

```
tclsh: Message 1
tclsh: Message 2
tclsh: Message 3
testident: Message 4
tclsh[7049]: Message 5.0
my_ident[7049]: Message 5.1
tclsh[7049]: Message 5.1
my_ident[7049]: Message 5.2
my_ident[7049]: Message 5.3
```

# REFERENCES

- Michael Kerrisk, *The Linux Programming Interface*, No Starch Press, 2010,
  ISBN 978-1-59327-220-3.

# AUTHORS

- Alexandros Stergiakis <sterg@kth.se>
- Massimo Manghi <mxmanghi@apache.org>

# COPYRIGHT

Copyright (C) 2008 Alexandros Stergiakis  
Copyright (C) 2024-2026 Massimo Manghi

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.
