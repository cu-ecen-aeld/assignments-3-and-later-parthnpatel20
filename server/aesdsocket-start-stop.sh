#!/bin/sh

ACTION=$1
SERVICE_NAME="aesdsocket"
SERVICE_PATH="/usr/bin/aesdsocket"

if [ -z "$ACTION" ]; then
    echo "Usage: $0 start|stop"
    exit 1
fi

case "$ACTION" in
    start)
        echo "Starting ${SERVICE_NAME}..."
        if start-stop-daemon -S -n ${SERVICE_NAME} -a ${SERVICE_PATH} -- -d; then
            echo "${SERVICE_NAME} is now running."
        else
            echo "Error: Unable to start ${SERVICE_NAME}."
            exit 1
        fi
        ;;

    stop)
        echo "Stopping ${SERVICE_NAME}..."
        if start-stop-daemon -K -n ${SERVICE_NAME} --signal SIGTERM; then
            echo "${SERVICE_NAME} has been stopped."
        else
            echo "Error: Unable to stop ${SERVICE_NAME}."
            exit 1
        fi
        ;;

    *)
        echo "Invalid option. Usage: $0 start|stop"
        exit 1
        ;;
esac

exit 0

