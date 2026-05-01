#!/bin/sh
set -eu

mkdir -p /app/table /app/system

exec "$@"
