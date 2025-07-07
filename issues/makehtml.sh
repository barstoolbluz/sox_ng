#! /bin/sh

# Convert .md files into .html for viewing with a web browser
#
# Can be run in the issues/ directory or in the top level directory.
#
# For a list of the files in the issue's directories, see issues.sh

# The sed scripts convert [Link](Link) to [Link](Link.html)
# but not for http:// links (which contain a colon and slashes)
# nor for images and patches which live in a subdir.

# multimarkdown https://fletcherpenney.net/multimarkdown
# can be obtained from https://github.com/fletcher/MultiMarkdown-6/releases
# and its output is naked HTML with no <head> or <body> tags.
# so we wrap that in a simple template.

# TODO: Render images inline in the web pages.

usage() {
    echo 'Usage: $0 [-d dir]'
    echo '-d:  Work in directory dir instead of the current working directory'
}

# Process command-line arguments

issuesdir=issues
while [ $# -gt 0 ]
do
    case "$1" in
    -d) if [ $# -lt 2 ]
	then echo "-d what?" 1>&2; exit 1
	else issuesdir="$2"; shift 2
	fi
	;;
    -d*) issuesdir="$(echo "$1" | sed 's/-d//')"; shift
	;;

    *)  {
	    echo "Unknown option $1"
	    usage
	} 1>&2
	exit 1
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

# Use printf instead of echo as it is more portable.
echo() {
    local nflag

    nflag=false
    case "$1" in
    -n) nflag=true; shift ;;
    esac

    printf "%s" "$*"
    $nflag || printf '\n'
}

echo '' | multimarkdown > /dev/null || {
    {
	echo "You need to install multimarkdown to run makehtml.sh"
	echo "Fedora/Red Hat/Centos: yum install multimarkdown"
	echo "Debian/Ubuntu/Mint: apt install libtext-multimarkdown-perl"
	echo "or get the latest version from"
	echo "https://fletcherpenney.net/multimarkdown" or
	echo "https://github.com/fletcher/MultiMarkdown-6/releases"
    } 1>&2
    exit 1
}

# Let them run it in the "issues" directory or from the top-level directory
# or in the issues directory with an issues/ subdirectory
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

if [ -z "$(ls *.md 2> /dev/null)" ]
then
    echo "Fetching issues..."
    sh getissues.sh -c
fi

# When including an issue name or file name in HTML code,
# escape <, ", & and \ (and > for good measure)
hprotect ()
{
    # Do & first so it doesn't mess up the other escape sequences
    echo "$1" | sed 's/&/\&amp;/g
		     s/</\&lt;/g
		     s/>/\&gt;/g
		     s/"/\&quot;/g
		     s/\\/\&bsol;/g'
}

# When including a file or directory name in a URL, escape
# "unsafe/unwise" characters: "{", "}", "|", "\", "^", "~", "[", "]", and "`"
# "reserved" characters:  ";", "/", "?", ":", "@", "=" and "&"
# "excluded" character:  " ", "<", ">", "#", "%", """ and control characters
# See RFC1738 and RFC2396.
# "/" should never occur because we map / to \ to have a valid Unix file name
# but we include it anyway.
# UTF8 characters' top bits are always set, but test this.
# As for control characters...
uprotect ()
{
    # Do % first so as not to mess up the other escape sequences
    echo "$1" | sed 's/%/%25/g
		     s/\t/%09/g
		     s/"/%22/g
		     s/#/%23/g
		     s/ /%20/g
		     s/&/%26/g
		     s/\//%2F/g
		     s/:/%3A/g
		     s/;/%3B/g
		     s/</%3C/g
		     s/=/%3D/g
		     s/>/%3E/g
		     s/?/%3F/g
		     s/@/%40/g
		     s/\[/%5B/g
		     s/\\/%5C/g
		     s/\]/%5D/g
		     s/\^/%5E/g
		     s/`/%60/g
		     s/{/%7B/g
		     s/|/%7C/g
		     s/}/%7D/g
		     s/~/%7E/g'
}

# Generate each issue's page
makeissues() {
    local mdfile number state milestone labels
    local title ftitle htitle utitle
    local file hfile ufile size type

    for mdfile in *.md
    do
	test ! -f "$mdfile" && continue
	ftitle="$(basename "$mdfile" .md)"
	title="$(cat "$ftitle/title")"
	# Version to use when included in HTML
	htitle="$(hprotect "$title")"
	number=; state=; milestone=; labels=
	test -f "$ftitle/number"    && number="$(cat "$ftitle/number")"
	test -f "$ftitle/state"     && state="$(cat "$ftitle/state")"
	test -f "$ftitle/milestone" && milestone="$(cat "$ftitle/milestone")"
	test -f "$ftitle/labels"    && labels="$(cat "$ftitle/labels")"
	test -z "$state" && state=open
	test null != "$milestone" && milestone=
	{
	    # Imitate the format of the forgego issue pages
	    echo "<html><head>"
	    if [ -z "$number" ]
	    then echo "<title>$htitle - sox_ng/sox_ng</title>"
	    else echo "<title>#$number - $htitle - sox_ng/sox_ng</title>"
	    fi
	    echo "</head><body>"
	    if [ -z "$number" ]
	    then echo "<p>sox_ng issue - $htitle"
	    else echo "<p>sox_ng issue #$number - $htitle"
	    fi
	    echo "<br>State: $state"
	    test -n "$milestone" && echo "<br>Milestone: $milestone"
	    test -n "$labels" && echo "<br>Labels: $labels"
	    echo '</p><hr>'
	    sed 's/  /    /g' "$mdfile" | multimarkdown | \
	    sed 's/href="\([-A-Za-z0-9]*\)"/href="\1.html"/g' | \
	    sed 's/<table>/<table border=1 cellspacing=0>/'
	    # List the attachments
	    tmp=$(mktemp)
	    test -d "$ftitle"/assets && ls -A "$ftitle"/assets > $tmp
	    if [ -s $tmp ]
	    then
		echo "<hr>"
		cat $tmp | while read file
		do
		    hfile="$(hprotect "$file")"
		    ufile="$(uprotect "$file")"
		    utitle="$(uprotect "$ftitle")"
		    size="$(stat -c %s "$ftitle/assets/$file")"
		    case "$file" in
		    *.patch) type="text/plain" ;;
		    *) type="application/octet-stream" ;;
		    esac
		    echo -n "[<a target=_blank href=\"$utitle/$ufile\" type=\"$type\">&darr;</a>]\
    <a target=_blank \
    href=\"$utitle/assets/$ufile\" type=\"$type\"><strong>$hfile</strong></a> ($size bytes)<br>"
		done
	    fi
	    rm $tmp

	    echo "<hr>Generated by <a href=\"https://codeberg.org/sox_ng/sox_ng/src/branch/main/issues/makehtml.sh\"><TT>makehtml.sh</TT></a> on $(LANG=C date -u)"
	    echo "</body></html>"
	} > "$(echo "$mdfile" | sed 's/\.md$//').html"
    done
}

# Generate the HTML for the open or closed issues
# Usage: "makeindex open" or "makeindex closed"
makeindex() {
    local state h2 mdfile number label milestone
    local title ftitle htitle utitle

    state="$1"
    case "$state" in
    open|closed) ;;
    *) echo "Internal error: makeindex($1)"; exit 1;;
    esac

    # Uppercase the first letter
    h2="$(echo $state | sed 's/./\u&/')"
    echo "  <h2>$h2</h2>"
    echo "  <ul>"

    # We need to generate the list of issues and then process them
    # to be able to sort them into reverse numerical order.
    for mdfile in *.md
    do
	# Guard against issues whose title ends in ".md"
	test ! -f "$mdfile" && continue

	ftitle="$(basename "$mdfile" .md)"

	# Skip issues that are not open. Issues with no state are open.
	case $state in
	open) # Skip if closed
	    if [ -f "$ftitle"/state ] && [ open != "$(cat "$ftitle"/state)" ]
	    then continue
	    fi;;
	closed) # Skip if open
	    if [ ! -f "$ftitle"/state ] || [ closed != "$(cat "$ftitle"/state)" ]
	    then continue
	    fi;;
	esac
	number="$(cat "$ftitle/number" 2> /dev/null)"
	if [ -z "$number" ]
	then printf "?? "	# no number yet
	else printf "$number "
	fi
	echo "$ftitle"
    done | sort -nr | while read -r number ftitle
    do
	# First line
	title="$(cat "$ftitle"/title)"
	htitle="$(hprotect "$title")"
	utitle="$(uprotect "$title")"
	echo -n "<li><a href=\"$utitle.html\">$htitle</a>"
	for label in $(cat "$ftitle/labels" 2> /dev/null)
	do
	    printf " <small>%s</small>" "$label"
	done
	# Second line
	printf "<br><small>#$number"
	milestone="$(cat "$ftitle/milestone" 2> /dev/null)"
	if [ -n "$milestone" ]
	then
	    printf " => %s" "$milestone"
	fi
	echo "</small>"
    done

    echo "  </ul>"
}

makeissues

# Generate the index
{
echo "<html>
 <head><title>sox_ng issues</title></head>
 <body>
  <h1><a href=\"https://codeberg.org/sox_ng/sox_ng\">sox_ng<A> issues</h1>"

# Show open issues
makeindex open

# Show closed issues
makeindex closed

echo "  <hr>Generated by <a href=\"https://codeberg.org/sox_ng/sox_ng/src/branch/main/issues/makehtml.sh\"><TT>makehtml.sh</TT></a> on $(LANG=C date -u)"
echo " </body>"
echo "</html>"
} > index.html
