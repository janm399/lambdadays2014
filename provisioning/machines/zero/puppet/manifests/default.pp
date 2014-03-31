Exec { path => [ "/bin/", "/sbin/" , "/usr/bin/", "/usr/sbin/" ] }

# The Zero machine needs the following components
include '::rabbitmq'
include development-tools