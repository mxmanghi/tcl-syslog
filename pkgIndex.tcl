#
# Tcl package index file
#
package ifneeded syslog 1.2.4 \
    [list load [file join $dir libsyslog1.2.4.so] syslog]
