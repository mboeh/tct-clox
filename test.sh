#!/bin/sh

set -e

for i in $(cat supported-tests.txt); do
  /bin/echo -n ... $i
  time (./clox $i >/dev/null)
done
