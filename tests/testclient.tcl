load ../libtcl9syslog1.3.0.so
proc writeServerChan {sk args} { puts $sk $args; flush $sk }
proc readServerChan {sk} { puts [gets $sk] }
set sk [socket 127.0.0.1 8888]

chan event $sk readable [list readServerChan $sk]
writeServerChan $sk wait literal "info message" 120000
::syslog::open -ident test
::syslog::log info "info message"



