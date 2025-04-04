#!/bin/sh

case "$1" in 
    start)
        echo "STARTED AESD SOCKET"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "STOPPED AESD SOCKET"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "USAGE :$0 {start|stop}"
    exit 1
esac
exit 0

