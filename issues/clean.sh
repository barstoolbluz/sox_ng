#! /bin/sh

# clean.sh - Remove auto-generated files (the HTML)
# -a  Also remove downloaded issues that haven't been modified locally
# -f  ...even if they've been modified locally

clean_all=false	# Also remove downloaded issues whose body has not been modified
force=false	# Remove local issues even if their body been modified

usage() {
  {
    echo 'Usage: sh clean.sh [-a [-f]]'
    echo '-a      Remove unmodified .md files and their directories'
    echo '-f      Remove all issues, even if they have been modified since download.'
    echo '-d dir  Work with issues in directory dir instead of in issues/'
  } 1>&2
}

# Process command line arguments

issuesdir=issues
while [ $# -gt 0 ]
do
    case "$1" in
    -f) force=true; shift ;;
    -a) clean_all=true; shift ;;

    -d) if [ $# -lt 2 ]
	then echo "-d what?" 1>&2; exit 1
	else issuesdir="$2"; shift 2
	fi
	;;
    -d*) issuesdir="$(echo "$1" | sed 's/-d//')"; shift
	;;

    *)  echo "Unknown option $1" 1>&2
        usage; exit 1
	;;
    esac
done

# Remove trailing /'s from the issuesdir
case "$issuesdir" in
*/)	issuesdir="$(echo "$issuesdir" | sed 's://*$::')"
esac

# Let them run it in the "issues" directory or from the top-level directory
if [ -d "$issuesdir" ]
then
    cd "$issuesdir"
fi
# Make sure they are in the issues directory
if [ "$(basename "$(pwd)")" != "$issuesdir" ]
then
    mkdir "$issuesdir"
    cd "$issuesdir"
fi

# Do it

rm -f *.html		# -f so it doesn't fail if there are none

$clean_all || exit

for mdfile in *.md
do
    if [ "*.md" = "$mdfile" ]
    then
	# No matching files (or one called "*.md"!)
	test -f "$mdfile" || continue
    fi

    # Guard against issues whose name ends in ".md"
    test -d "$mdfile" && continue

    # The directory of metadata belonging to this issue
    dirname="$(basename "$mdfile" .md)"

    if $force
    then
	rm -f "$mdfile"
	rm -rf "$dirname"
	continue
    fi

    # Only remove when both .md file and corresponding directory exist
    # to avoid wiping any other .md files and defend against issue titles
    # that end in ".md"
    # .md files that have been modified locally can be protected by chmod 444
    # and new ones are protected by not having a "number" file in their dir.
    # We also check that the .md file hasn't been modified since its directory
    # (the .md file is created by getissues.sh before the directory)
    if [ -f "$mdfile" -a -w "$mdfile" \
	 -a -d "$dirname" -a -f "$dirname"/number \
	 -a ! \( "$mdfile" -nt "$dirname" \) ]
    then
	rm "$mdfile"
	rm -r "$dirname"
    fi
done
