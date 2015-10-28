#!/bin/bash
#
# Easy Publish and Consume Library
# Copyright (C) 2007, 2008  Openismus GmbH
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# Authors:
#      Mathias Hasselmann
#

test -n "$EPC_DEBUG" && echo "$0: running $1..."

default_system_bus_address=$(pkg-config dbus-1 --variable=system_bus_default_address)

EPC_DEBUG="${EPC_DEBUG:-1}" G_DEBUG="${G_DEBUG:-fatal-warnings}" \
DBUS_SYSTEM_BUS_ADDRESS="${DBUS_SYSTEM_BUS_ADDRESS:-${default_system_bus_address}}" \
"$@" 2> "$2.err" > "$2.out"

result=$?

if test -n "$EPC_DEBUG"
then
  echo "===== BEGIN OF STDOUT ====="
  cat "$1.out"
  echo "===== END OF STDOUT ====="

  echo "===== BEGIN OF STDERR ====="
  cat "$1.out"
  echo "===== END OF STDERR ====="
fi

exit $result
