#! /bin/sh

# Run all the tests
#
# To test a different executable than the system "sox",
# set environment variable "sox" to the name or path to the executable.
# e.g.
#	sox=$HOME/sox-14.4.2/src/sox sh check.sh
#
# You can also just run one or more tests,
# e.g.
#	sh check.sh CVE-2017-18189

usage() {
    {
	echo "Usage: check.sh [-n] [test] ..."
	echo "-n  Don't print the legend"
    } 1>&2
}

# Process command line arguments

legend=true
while [ $# -gt 0 ]
do
    case "$1" in
    -n) legend=false; shift;;
    -*) usage; exit 1;;
    *)  break;;
    esac
done

args="$*"
if [ -z "$args" ]
then
    args="`ls`"
fi

# By default, check the sox in the source tree
sox=${sox:-../../src/sox_ng}
export sox

# We don't want memory leaks to report a failure.
ASAN_OPTIONS=detect_leaks=0
export ASAN_OPTIONS

errs=/tmp/check$$

# Print the legend
$legend && cat << \EOF
Legend
OK   The test succeeded and sox succeeded or failed as it should
FAIL The test ran but gave the wrong result.
SUCC sox "succeeded" (exit 0) when it should have failed (exit 2)
ABRT sox Aborted (core dumped)
SEGV sox got a Segmentation fault (core dumped)
FPE  sox got a Floating Point Exception (core dumped)
LOOP sox ran for more than four minutes of CPU.
EXEC Can't execute the sox binary. Missing shared libraries also provoke this.
VOID This test cannot be run with the sox you have installed
ASAN The Address Sanitizer reports problems other than memory leaks:
     buffer overflows, freeing free memory, allocating until running out of VM.
     It means exit(1), which sox only gives for command-line syntax errors.
     Other failures (SEGV, FPE etc) will be caught by ASAN in such builds.
     You will also get an ASAN error if your sox wasn't built with support
     for the input or output format, in which case it exits 1.

EOF

# The exit code the whole script should give: 0 unless anything fails
endexit=0

for a in $args
do
    if [ -d "$a" -a -f "$a"/run ]
    then
	printf "${a}: "
        (
	    cd "$a"
	    if [ ! -f run ]
	    then
		echo "No test file"
		continue
	    fi
	    # cfarm185 takes 3m09 to run CVE-2019-8357 so max 4m
	    ulimit -t 240
	    # Stop CVE-2019-853[47] from taking forever on 64-bit machines.
	    ulimit -v 1048576
	    sh run
	) > $errs 2>&1
	status=$?
	case $status in
	0)   result=OK ;;
	1)   result=ASAN ;;
	2)   result=FAIL ;;
	127) result=EXEC ;;
	134) result=ABRT ;;
	136) result=FPE ;;
	137) result=LOOP ;;
	139) result=SEGV ;;
	254) result=VOID ;;
	255) result=SUCC ;;
	*)   result=$status ;;
	esac
	echo $result
	rm $errs
	if [ $status -ne 0 ] && [ $status -ne 254 ]
	then
	    endexit=$status
	fi
    fi
done

exit $endexit
