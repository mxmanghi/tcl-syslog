#!/usr/bin/tclsh9.0
#
#

namespace eval ::syslogtest::server {
    variable sourceChannel ""
    variable sourceKind ""
    variable sourceCmd {}
    variable listener ""
    variable running 1
}

proc ::syslogtest::server::usage {} {
    puts stderr "usage: server.tcl ?-source syslog|journalctl?"
    exit 2
}

proc ::syslogtest::server::parseArgs {argv} {
    set opts [dict create -source auto]
    set n [llength $argv]
    for {set i 0} {$i < $n} {incr i} {
        set arg [lindex $argv $i]
        switch -- $arg {
            -port -
            -source {
                incr i
                if {$i >= $n} {
                    usage
                }
                dict set opts $arg [lindex $argv $i]
            }
            default {
                usage
            }
        }
    }

    return $opts
}

proc ::syslogtest::server::buildSourceCommand {preferred} {
    set tail [auto_execok tail]
    set journalctl [auto_execok journalctl]

    if {$preferred eq "syslog"} {
        if {[file readable /var/log/syslog] && $tail ne ""} {
            return [dict create kind syslog cmd [list $tail -n 0 -F /var/log/syslog]]
        }
        error "requested syslog source is unavailable"
    }

    if {$preferred eq "journalctl"} {
        if {$journalctl ne ""} {
            return [dict create kind journalctl cmd [list $journalctl -f -n 0 --no-pager -o short-iso]]
        }
        error "requested journalctl source is unavailable"
    }

    if {[file readable /var/log/syslog] && $tail ne ""} {
        return [dict create kind syslog cmd [list $tail -n 0 -F /var/log/syslog]]
    }

    if {$journalctl ne ""} {
        return [dict create kind journalctl cmd [list $journalctl -f -n 0 --no-pager -o short-iso]]
    }

    error "no syslog source available (need readable /var/log/syslog or journalctl)"
}

proc ::syslogtest::server::openSource {} {
    variable sourceChannel
    variable sourceKind
    variable sourceCmd

    if {$sourceChannel ne ""} {
        catch {close $sourceChannel}
    }

    set sourceChannel [open |$sourceCmd r]
    fconfigure $sourceChannel -blocking 0 -buffering line -encoding utf-8 -translation auto
}

proc ::syslogtest::server::parseLine {line} {
    set tsKind none
    set payload $line

    if {[regexp {^([A-Z][a-z]{2}\s+[ 0-9][0-9]\s[0-9]{2}:[0-9]{2}:[0-9]{2})\s+\S+\s+(.*)$} $line -> _ rest]} {
        set tsKind rfc3164
        set payload $rest
    } elseif {[regexp {^([0-9]{4}-[0-9]{2}-[0-9]{2}[T ][0-9:.+-]+)\s+\S+\s+(.*)$} $line -> _ rest]} {
        set tsKind iso8601
        set payload $rest
    } elseif {[regexp {^[A-Z][a-z]{2}\s+[ 0-9][0-9]\s[0-9]{2}:[0-9]{2}:[0-9]{2}\s+(.*)$} $line -> rest]} {
        set tsKind rfc3164
        set payload $rest
    } elseif {[regexp {^[0-9]{4}-[0-9]{2}-[0-9]{2}[T ][0-9:.+-]+\s+(.*)$} $line -> rest]} {
        set tsKind iso8601
        set payload $rest
    }

    if {[regexp {^[^:]+:\s*(.*)$} $payload -> msg]} {
        set payload $msg
    }

    return [dict create raw $line payload $payload timestamp_kind $tsKind]
}

proc ::syslogtest::server::matches {mode pattern parsed} {
    set payload [dict get $parsed payload]

    switch -- $mode {
        literal {
            return [expr {[string first $pattern $payload] >= 0}]
        }
        regexp {
            return [regexp -- $pattern $payload]
        }
        default {
            error "unsupported mode '$mode'"
        }
    }
}

proc ::syslogtest::server::waitFor {mode pattern timeoutMs} {
    variable sourceChannel

    if {![string is integer -strict $timeoutMs] || $timeoutMs <= 0} {
        error "timeout must be a positive integer in milliseconds"
    }

    set deadline [expr {[clock milliseconds] + $timeoutMs}]

    while {[clock milliseconds] <= $deadline} {
        if {[eof $sourceChannel]} {
            openSource
            after 50
            continue
        }

        set n [gets $sourceChannel line]
        if {$n >= 0} {
            set parsed [parseLine $line]
            if {[matches $mode $pattern $parsed]} {
                return $parsed
            }
            continue
        }

        after 50
    }

    error "timeout after ${timeoutMs}ms waiting for $mode match"
}

proc ::syslogtest::server::writeResponse {chan status args} {
    puts $chan [list $status {*}$args]
    flush $chan
}

proc ::syslogtest::server::onClientReadable {chan} {
    variable running

    if {[eof $chan]} {
        catch {close $chan}
        return
    }

    set n [gets $chan line]
    if {$n < 0} {
        return
    }

    if {$line eq ""} {
        writeResponse $chan error "empty request"
        return
    }
    puts "<--- $line"
    set cmd [lindex $line 0]
    switch -- $cmd {
        ping {
            writeResponse $chan ok pong
        }
        wait {
            if {[llength $line] != 4} {
                writeResponse $chan error "usage: wait literal|regexp pattern timeoutMs"
                return
            }

            set mode      [lindex $line 1]
            set pattern   [lindex $line 2]
            set timeoutMs [lindex $line 3]

            if {[catch {set parsed [waitFor $mode $pattern $timeoutMs]} err]} {
                writeResponse $chan error $err
                return
            }

            writeResponse $chan ok          \
                [dict get $parsed raw]      \
                [dict get $parsed payload]  \
                [dict get $parsed timestamp_kind]
        }
        shutdown {
            writeResponse $chan ok bye
            set running 0
        }
        default {
            writeResponse $chan error "unknown command '$cmd'"
        }
    }
}

proc ::syslogtest::server::acceptClient {chan _addr _port} {
    chan $chan -blocking 0 -buffering line -encoding utf-8 -translation lf
    chan event $chan readable [list ::syslogtest::server::onClientReadable $chan]
}

proc ::syslogtest::server::main {argv} {
    variable sourceCmd
    variable sourceKind
    variable listener
    variable running

    set opts [parseArgs $argv]
    set source [buildSourceCommand [dict get $opts -source]]
    set sourceCmd [dict get $source cmd]
    set sourceKind [dict get $source kind]
    openSource

    if {![dict exists $opts -port]} { usage }
    set port [dict get $opts -port]

    set listener [socket -server ::syslogtest::server::acceptClient -myaddr 127.0.0.1 $port]
    while {$running} {
        vwait ::syslogtest::server::running
    }

    catch {close $listener}
    catch {close $::syslogtest::server::sourceChannel}
    catch {file delete -force -- $portfile}
}

if {[catch {::syslogtest::server::main $argv} err opts]} {
    puts stderr "syslog test server error: $err"
    if {[dict exists $opts -errorinfo]} {
        puts stderr [dict get $opts -errorinfo]
    }
    exit 1
}

