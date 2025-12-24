#!/bin/bash

set -e

USER_ID=${USER_ID:-1000}
GROUP_ID=${GROUP_ID:-1000}
USER_NAME=${USER_NAME:-user}
USER_NAME=${USER_NAME:-user}
OPT=${ARGS:+-c $ARGS}

COLLIDING_USER_NAME=$(id -nu "$USER_ID" 2>/dev/null)

if [[ -n "$COLLIDING_USER_NAME" ]]; then
  echo Removing colliding user "($COLLIDING_USER_NAME)"...
  userdel "$COLLIDING_USER_NAME"
fi

echo Adding invoking user "($USER_NAME)"...
groupadd -g "$GROUP_ID" "$USER_NAME"
useradd -u "$USER_ID" -g "$GROUP_ID" -s /bin/bash "$USER_NAME"

set -x
exec su "$USER_NAME" $OPT
