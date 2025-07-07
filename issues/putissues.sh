#! /bin/sh

# putissues.sh, a wrapper for "issues.sh put"

exec sh $(echo $0 | sed 's/putissues/issues/') put "$@"
