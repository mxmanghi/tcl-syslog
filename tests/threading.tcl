package require Tcl
package require Thread

set auto_path [concat "." ".." $auto_path]
package require syslog 2.0.0

proc broadcast {cmd} {
    foreach t [array names ::thread_pool] {
        uplevel [list ::thread::send -async $::thread_pool($t) $cmd]
    }
}

::syslog::open -ident test -pid

set n -1
while {$n < 20} {
    set thread_n [incr n]
    set thread_pool($thread_n) [::thread::create {
        set auto_path [concat "." ".." $auto_path]
        package require syslog 2.0.0

        set nlog_msg      0
        set batch         10
        set nlog_in_batch 0
        proc log_message {} {
            ::syslog::log info "\[[::thread::id]\] logging msg [incr ::nlog_msg]"
            if {$::nlog_in_batch > $::batch} {
                set ::nlog_in_batch 0
            } else {
                incr ::nlog_in_batch
                after 50 log_message
            }
        }

        proc start_logging {} {
            after 50 log_message
        }
        ::syslog::log info "\[[::thread::id]\] posed to wait for command"

        ::thread::wait
    }]

    ::thread::preserve $thread_pool($thread_n)
}

