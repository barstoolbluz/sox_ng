bindir="."
libdir="."
srcdir="."
effect=""

if [ -f ./sox_ng.exe ] ; then
  EXEEXT=".exe"
else
  EXEEXXT=""
fi

# Allow user to override paths.  Useful for testing an installed
# sox_ng.
while [ $# -ne 0 ]; do
    case "$1" in
        --bindir=*)
        bindir=`echo $1 | sed 's/.*=//'`
        ;;

        -i)
        shift
        bindir=$1
        ;;

        --libdir=*)
        libdir=`echo $1 | sed 's/.*=//'`
        ;;

        -i)
        shift
        libdir=$1
        ;;

        --srcdir=*)
        srcdir=`echo $1 | sed 's/.*=//'`
        ;;

        -c)
        shift
        srcdir=$1
        ;;

        *)
        effect="$effect $1"
    esac
    shift
done

t() {
	format=$1
	shift
	opts="$*"

	echo "Format: $format   Options: $opts"
	LD_LIBRARY_PATH=${libdir} ${bindir}/sox_ng${EXEEXT} ${srcdir}/monkey.wav $opts /tmp/monkey.$format $effect || {
		status=$?
		echo Test to $format $opts $effect failed. 1>&2
	}
	LD_LIBRARY_PATH=${libdir} ${bindir}/sox_ng${EXEEXT} $opts /tmp/monkey.$format /tmp/monkey1.wav  $effect || {
		status=$?
		echo Test from $format $opts $effect failed. 1>&2
	}
}

status=0

t 8svx
t aiff
t aifc
t au 
t avr -e unsigned-integer
t cdr -r 44100 -c 2
t cvs
t dat
t hcom -r 22050
t maud
t prc -r 8000		# a-law by default
t prc -r 8000 -e ima-adpcm
t sf 
t smp
t sndt 
t txw -r 50000
t ub -r 8130 -c 1
t vms
t voc
t vox -r 8130
t wav
t wve -r 8000
t wav -e gsm-full-rate

exit $status
