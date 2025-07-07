#! /bin/sh

# getissues.sh, a wrapper for "issues.sh get"

exec sh $(echo $0 | sed 's/getissues/issues/') get "$@"
