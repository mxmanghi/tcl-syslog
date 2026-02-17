
namespace eval ::syslogtest::harness {
    variable channel            ""
    variable server_pid         ""
    variable log_file           ""
    variable server_script      [file join [file dirname [info script]] server.tcl]
    variable server_port        8888
}

proc ::syslogtest::harness::request {args} {
    variable channel

    if {$channel eq ""} {
        error "server is not running"
    }

    puts $channel $args
    flush $channel
    if {[chan gets $channel response] < 0} {
        error "no response from test server (--> $args)"
    }

    set status [lindex $response 0]
    if {$status eq "ok"} {
        return [lrange $response 1 end]
    }

    set message [lindex $response 1]
    if {$message eq ""} {
        set message "unknown server error"
    }
    error $message
}

proc ::syslogtest::harness::start {{timeoutMs 5000}} {
    variable channel
    variable server_pid
    variable log_file
    variable server_script
    variable server_port

    if {$channel ne ""} {
        return
    }

    set port $::syslogtest::harness::server_port

    set token   "[pid]-[clock milliseconds]-[expr {int(rand() * 100000)}]"
    set log_file [file join /tmp "tcl-syslog-test-server-${token}.log"]

    set cmd [list [info nameofexecutable] $server_script -port $port]
    set pids [exec {*}$cmd >$log_file 2>@1 &]
    set server_pid [lindex $pids 0]

    puts "pid: $server_pid"

    set deadline [expr {[clock milliseconds] + $timeoutMs}]
    while {[clock milliseconds] <= $deadline} {
        if {[catch {exec kill -0 $server_pid}]} {
            set details ""
            if {[file exists $log_file]} {
                set in [open $log_file r]
                set details [string trim [read $in]]
                close $in
            }
            [namespace current]::stop
            if {$details ne ""} {
                error "test server exited during startup: $details"
            }
            error "test server exited during startup"
        }
        after 50
    }
    set deadline [expr {[clock milliseconds] + $timeoutMs}]

    set connected 0
    while {[clock milliseconds] <= $deadline} {
        if {![catch {set channel [socket 127.0.0.1 $port]}]} {
            set connected 1
            break
        }
        after 50
    }
    if {!$connected} {
        [namespace current]::stop
        error "timed out connecting to test server on port $port"
    }

    chan configure $channel -blocking 1 -buffering line -encoding utf-8 -translation lf
    request ping
}

proc ::syslogtest::harness::stop {} {
    variable channel
    variable server_pid
    variable log_file

    if {$channel ne ""} {
        catch {request shutdown}
        catch {close $channel}
    }
    set channel ""

    if {$server_pid ne ""} {
        catch {exec kill $server_pid}
    }
    set server_pid ""

    if {$log_file ne "" && [file exists $log_file]} {
        catch {file delete -force -- $log_file}
    }
    set log_file ""
}

proc ::syslogtest::harness::waitForLiteral {text {timeoutMs 5000}} {
    set response [request wait literal $text $timeoutMs]
    return [dict create \
        raw [lindex $response 0] \
        payload [lindex $response 1] \
        timestamp_kind [lindex $response 2]]
}

proc ::syslogtest::harness::waitForRegexp {pattern {timeoutMs 5000}} {
    set response [request wait regexp $pattern $timeoutMs]
    return [dict create \
        raw [lindex $response 0] \
        payload [lindex $response 1] \
        timestamp_kind [lindex $response 2]]
}
