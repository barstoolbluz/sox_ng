#! /bin/sh

# issue.sh: Manage issues on a forgejo instance
#
# For an issue with title "CVE-2017-18189", the markdown for its description
# is in the file issues/CVE-2017-18189.md
# It also has a directory issues/CVE-2017-18189/ with files:
# - title         The issue's real title (the filename may be munged)
# - state         Containing "open" or "closed".
#                 If absent, this is an issue created locally.
# Optional files
# - milestone     The milestone name, if any
# - labels        The labels, if any, one per line
# - assets/       A directory containing its attachments
#                 Files whose names end in ".patch" or ".diff" are downloaded
#                 with CR characters stripped out, so their size and content
#                 may not correspond exactly with the remote content.
# Files recording values assigned by Forgejo
# - number        From 1 upward monotonically
# - id            Unique number in all issues on forgejo
# - username      Who created the issue
# - created_at    When it was created
# - updated_at    When it was last updated on the server
#                 The format of *_at is "2025-02-11T00:41:06Z"
#
# It requires all issues to have different titles because
# that's what it indexes them by, and new issues won't have a number yet.
# As a result it doesn't update them in numerical order; I suppose that
# could be possible by sorting them by number and doing new issues last.
#
# putissues is chatty and its output to stdout is one line for each change.
# Errors go to stderr, so if you need it to be silent except for errors >/dev/null
#
# It can be run from the top directory or in the issues/ directory,
# which make it work both when run as issues/getissues.sh and
# if you're working in issues/ with a directory issues/issues; it will
# download them therein to avoid cluttering the working directory and
# as an easy way of keeping a copy the proper output if you're working on it
# by moving issues/ to some other name.
# The -o option tells it to use a different directory.
#
# The site, owner and repo default to the remote origin of the git
# directory you're in, can be specified as codeberg.org/sox_ng/sox_ng
# or the individual components can be overridden by the -s -o and -r flags.
#
# This is really two programs that share many subroutines:
#
# getissues()
# - erase all issues from the local directory
# - download the issue list, making an .md file and a directory for each
#   containing all the fields listed above
#
# putissues()
# - for each local .md file and its directory:
#   - if there is no number stored in the local issue,
#     it is a new, locally-created one, so create a new remote issue
#     and store locally the number, id, creatied_at and username it got
#   - if it does have a number locally, update the corresponding remote issue
#     where the fields' contents differ
# - if there are local issues whose numbers do not correspond to any remote
#   issues' numbers, that means we are "ahead" of the remote repository,
#   most likely because we are updating the issues of an older migration:
#   - create new empty issues for the whole range of new issue numbers
#     with just the title "dummy"
#   - go through the local issues filling in the remote issues' contents
#   - delete any remote issues that were not filled in
#
# Documentation for the codeberg issues API is at
# https://codeberg.org/api/swagger#/issue
#
#       Martin Guy <martinwguy@gmail.com>, July-November 2024

# TODO:
# - If a URL transfer failed due to bad network connectivity, retry it.
# - When creating new issues from local, fill in creation date and username.
#   You can't set or change the remote creation date, but it your clocks are
#   synchronized it will be the same within a second or two.

# For function-local variables, bash, dash and ksh have "local";
# ksh has "typeset"
test -n "$KSH_VERSION" && alias local=typeset

# dash's built-in "echo" always interprets backslash sequences
# so replace it with the more portable "printf".
# For issue bodies or json, don't call printf which may be an external command
# because that limits the size of the argument to 128K or whatever.
# If you know you don't want a trailing newline, use echo_n

echo() {
    case "$1" in
    -n) shift; printf %s "$*" ;;
    *)  cat << EOF
$*
EOF
	;;
    esac
}

echo_n() {
    printf %s "$*"
}

usage() {
  {
    echo 'Usage: sh issues.sh (get|put) [site/owner/repo] [options]'
    echo 'get      Delete all issues here and download them anew'
    echo 'put      Push local issues to the remote database'
    echo '-c       Use curl instead of wget'
    echo '-w       Use wget instead of curl'
    echo '-s site  Forgejo site to access (e.g. codeberg.org)'
    echo '-o owner Owner of the repo (e.g. martinwguy)'
    echo '-r repo  Repository of the owner to access (e.g. issues)'
    echo '-u user  Which user to access forgejo as'
    echo '-p pass  Password for -u user'
    echo '-d dir   Work with issues in directory dir instead of in issues/'
    echo '-f       Force updating of issues whose id doesn'\''t match'
    echo '         (useful when copying issues between forks or migrations)'
    echo '-n       Dry run: print what would be done but don'\''t update anything'
    echo '-D       Allow deletion of remote issues with no local correspondent'
    echo '         (valid only with putissues)'
    echo '-O       Only fetch open issues (valid only with getissues)'
    echo '-U       Show the URLs that curl is putting to (for debugging)'
    echo '-J       Dump the remote issue list to issues.json (for debugging)'
  } 1>&2
}

# The first argument must be the action
case "$1" in
get|put)	action="$1"; shift ;;
*)		usage; exit 1 ;;
esac

site=; owner=; repo=

# The second argument may be site/owner/repo
case "$1" in
*/*/*)
    site="$( echo "$1" | sed -n 's|^\([^/]*\)/.*|\1|p')"
    owner="$(echo "$1" | sed -n 's|^[^/]*/\([^/]*\)/.*|\1|p')"
    repo="$( echo "$1" | sed -n 's|^[^/]*/[^/]*/\(.*\)$|\1|p')"
    shift ;;
*)

    # If we're in a git repository, set the default site/owner/repo from the origin
    #
    # ssh://[user@]host.xz[:port]/path/to/repo.git/
    # git://host.xz[:port]/path/to/repo.git/
    # http[s]://host.xz[:port]/path/to/repo.git/
    # ftp[s]://host.xz[:port]/path/to/repo.git/
    # [user@]host.xz:path/to/repo.git/
    # ssh://[user@]host.xz[:port]/~[user]/path/to/repo.git/
    # git://host.xz[:port]/~[user]/path/to/repo.git/
    # [user@]host.xz:/~[user]/path/to/repo.git/
    # /path/to/repo.git/
    # file:///path/to/repo.git/

    if git remote 2>&1 > /dev/null
    then

	remotes="$(git remote -v | grep '^origin')"
	case "$action" in
	get) remote="$(echo "$remotes" | grep ' (fetch)$' | awk '{print $2}')" ;;
	put) remote="$(echo "$remotes" | grep ' (push)$'  | awk '{print $2}')" ;;
	esac

	case "$remote" in
	https://*)
	    site="$(echo "$remote" | \
		    sed -n 's|https://\([^/]*\)/.*|\1|p')"
	    owner="$(echo "$remote" | \
		    sed -n 's|https://[^/]*/\([^/]*\)/.*|\1|p')"
	    repo="$(echo "$remote" | \
		    sed -n 's|https://[^/]*/[^/]*/\(.*\)$|\1|p')"
	    if [ -z "$site" ] || [ -z "$owner" ] || [ -z "$repo" ]
	    then
		echo "I can't decode the git origin '$remote'" 1>&2
		echo "Use site/owner/repo" 1>&2
		site=; owner=; repo=
	    fi
	    ;;
	*)
	    echo "$0: I only understand https:// git origins" 1>&2
	    exit 1 ;;
	esac
    fi ;;
esac

# Process command line arguments
force=false	# put: update even if ".id" doean't match;
dryrun=false	# put: Say what we'd do but don't do it
allow_delete=false	# put: Allow eletion of remote issues
open_only=false # Only fetch open issues
debug_URLs=false # put: Print curl commands before executing them
transfer=	# wget or curl
issuesdir=issues
username=
password=
dumpissues=false

while [ $# -gt 0 ]
do
    case "$1" in
    -f) force=true; shift ;;
    -n) dryrun=true; shift ;;
    -D) allow_delete=true; shift ;;
    -O) open_only=true; shift;;
    -U) debug_URLs=true; shift ;;
    -J) dumpissues=true; shift ;;
    -c) transfer=curl; shift ;;
    -w) transfer=wget; shift ;;

    -s) if [ $# -lt 2 ]
	then echo "-s what?" 1>&2; exit 1
	else site="$2"; shift 2
	fi
	;;
    -s*) site="$(echo "$1" | sed 's/-s//')"; shift
	;;

    -o) if [ $# -lt 2 ]
	then echo "-o what?" 1>&2; exit 1
	else owner="$2"; shift 2
	fi
	;;
    -o*) owner="$(echo "$1" | sed 's/-o//')"; shift
	;;

    -r) if [ $# -lt 2 ]
	then echo "-r what?" 1>&2; exit 1
	else repo="$2"; shift 2
	fi
	;;
    -r*) repo="$(echo "$1" | sed 's/-r//')"; shift
	;;

    -u) if [ $# -lt 2 ]
	then echo "-u what?" 1>&2; exit 1
	else username="$2"; shift 2
	fi
	;;
    -u*) username="$(echo "$1" | sed 's/-u//')"; shift
	;;

    -p) if [ $# -lt 2 ]
	then echo "-p what?" 1>&2; exit 1
	else password="$2"; shift 2
	fi
	;;
    -p*) password="$(echo "$1" | sed 's/-p//')"; shift
	;;

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

# Check arguments' validity

if [ -z "$action" ]
then
   echo "issues.sh: You need to specify 'put' or 'get'"
   exit 1
fi

if $dryrun && [ "$action" != put ]
then
    echo "-n only works with $0 put" 1>&2
    exit 1
fi

if $open_only && [ "$action" != get ]
then
    echo "-O is only valid for $0 get" 1>&2
    exit 1
fi

if $debug_URLs && [ "$action" != put ]
then
    echo "-U only works with $0 put" 1>&2
    exit 1
fi

# Authentication is compulsory when putting
$dryrun || case "$action" in
put) if [ -z "$username" ]
     then echo_n "Username for $site: "
          read -r username
     fi ;;
esac

case "$username" in
*\ *) echo "The username cannot contain a space" 1>&2; exit 1 ;;
esac
case "$password" in
*\ *) echo "The password cannot contain a space" 1>&2; exit 1 ;;
esac
if [ -n "$username" ] && [ -z "$password" ]
then
    stty -echo
    printf "Password: "
    read -r password
    printf "\n"
    stty echo
    case "$password" in
    *\ *) echo "The password cannot contain a space" 1>&2; exit 1 ;;
    esac
fi

# Remove trailing /'s from the issuesdir
case "$issuesdir" in
*/)	issuesdir="$(echo "$issuesdir" | sed 's://*$::')"
esac

# Let them run it in the "issues" directory or from the top-level directory
if [ -d "$issuesdir" ]
then
    cd "$issuesdir" || {
	echo "Can't cd into $issuesdir" 1>&2
	exit 1
    }
fi
# Make sure they are in an "issues" directory
if [ "$(basename "$(pwd)")" != "$issuesdir" ]
then
    mkdir "$issuesdir"
    cd "$issuesdir" || {
	echo "Can't cd into $issuesdir" 1>&2
	exit 1
    }
fi

# Pick the program to use for URL transfers.
# If they didn't specify -w or -c,
# prefer wget for getting as it is slightly faster and
# prefer curl for putting as it's the only one that works at present.
select_transfer()
{
    local have_wget
    local have_curl

    # curl works for getting and putting issues.
    # wget only works for getting and uses less CPU.
    if [ "$transfer" = wget ] && [ "$action" = put ]
    then
	echo "wget only works when getting issues. Use curl." 1>&2
	exit 1
    fi

    # If they didn't specify -c or -w, find a program to use.
    have_curl=false
    curl 2> /dev/null; test $? -eq 2 && have_curl=true
    have_wget=false
    wget 2> /dev/null; test $? -eq 1 && have_wget=true

    # If they asked for a specific program, check it is installed.
    # If they didn't, select one automatically.
    case "$transfer" in
    curl) $have_curl && return
	  echo "curl is not installed" 1>&2; exit 1
	  ;;
    wget) $have_wget && return
	  echo "wget is not installed" 1>&2; exit 1
	  ;;
    *)    case "$action" in
	  get) $have_wget && { transfer=wget; return; }
	       $have_curl && { transfer=curl; return; }
	       echo "You need wget or curl to be able to get issues" 1>&2
	       ;;
	  put) $have_curl && { transfer=curl; return; }
	       echo "You need curl to be able to put issues" 1>&2
	       ;;
	esac ;;
    esac
    exit 1
}

select_transfer

# Make sure jq is present
if echo '' | jaq > /dev/null
then
    jq="jaq -c"
elif echo '' | jq > /dev/null
then
    jq="jq -c"
else
    {
	echo "issues.sh needs jq"
	echo "Debian: apt install jq; Fedora: yum install jq"
	echo "or you should be able to install the latest version with"
	echo "    git clone https://github.com/jqlang/jq"
	echo "    cd jq"
	echo "    git submodule update --init"
	echo "    autoreconf -i && ./configure --with-oniguruma=builtin"
	echo "    make && sudo make install"
	echo "if that doesn't work, try"
	echo "    make clean; make LDFLAGS=-all-static && make install"
	echo "Or jaq from https://github.com/01mf02/jaq -> Releases is twice as fast".
    } 1>&2
    exit 1
fi

# Turn an X.Y.Z version into something numerically comparable
# From https://stackoverflow.com/questions/4023830
version() {
    echo "$@" | awk -F. '{ printf("%d%03d%03d\n", $1,$2,$3) }'
}

api="https://${site}/api/v1"
apirepo="$api/repos/$owner/$repo"

# Fetch a URL and output the result to stdout.
# Usage: geturl method [-r] URL failure_message
#
# method: GET or DELETE
# -r  ("raw") Output exactly the reply: don't eliminate CRs
#             and don't ensure it is newline-terminated.
#	      Used when downloading binary files (attachments)
geturl() {
    local errs
    local raw
    local a
    local url
    local command
    local result
    local status
    local message

    errs=/tmp/issues-geturl-errs$$

    # Process arguments
    method="$1"; shift
    raw=false
    for a
    do
	case "$a" in
	-r) raw=true; shift;;
	-*) echo "$0: internal error: geturl called with $a" 1>&2
	    exit 1;;
	*)  break;;
	esac
    done

    url="$1"; shift
    message="$*"

    case $transfer in
    curl) command="curl --fail --silent --compressed -S -L --location-trusted \
			-H Accept:application/json -X $method" ;;
    wget) command="wget -q -O - --compression=gzip \
			--header Accept:application/json" ;;
    esac

    # I can't figure out how to handle arguments including a space in $command
    # and that makes it impossible to authenticate with an access token
    # or with username/password if the password contains a space.
    # For an access token that would be:
    #   curl -H "authorization: token $token"
    if [ -n "$username" ]
    then
	case $transfer in
	curl) command="$command -u $username:$password" ;;
	wget) command="$command --user=$username --password=$password" ;;
	esac
    fi

    # $result here is a filename into which we put the result
    # because if we do result="$($command)" a final newline is removed.
    result=/tmp/issues-geturl-result$$
    $command "$url" > $result 2> $errs
    status=$?
    if [ $status != 0 ]
    then
	{
	    echo "$method $url exited $status"
	    cat $errs
	    # If we got a JSON reply with an error message in the header, print it
	    if [ -s $result ]
	    then
		echo "$message":
		$jq -r .message < $result
	    fi
	} 1>&2
	rm $errs $result
	return $status
    fi
    if [ "$method" = GET ] && [ ! -s $result ]
    then
	{
	    echo "$method $url returned nothing"
	    cat $errs
	    rm $errs $result
	    return 1
	} 1>&2
    fi

    if $raw
    then
	cat $result
    else
	# Eliminate CR from CRLF and ensure it is newline-terminated
	echo "$(tr -d '\r' < $result)"
    fi
    rm $errs $result

    return 0
}

# puturl: Post a URL and output the result to stdout.
#
# Usage: puturl METHOD [-r] [-f] URL data failure_message [file to attach]
# -a	      Send an attachment: Use Content-Type:multipart/form-data
#	      and the name of the file to attach is in $data
# method: POST or PATCH
puturl() {
    local method
    local url
    local data
    local message
    local attachment
    local errs
    local a
    local command
    local result
    local status

    errs=/tmp/issues-puturl-errs$$

    # Process arguments
    attachment=false
    method="$1"; shift
    for a
    do
	case "$a" in
	-a) attachment=true; shift;;
	-*) echo "$0: internal error: puturl called with $a" 1>&2
	    exit 1;;
	*)  break;;
	esac
    done

    url="$1"; shift
    data="$1"; shift
    message="$1"; shift

    case $transfer in
    curl)
	curl_version="$(curl --version | head -1 | awk '{print $2}')"
	if [ "$(version "$curl_version")" -lt "$(version "7.76.0")" ]
	then fail=--fail
	else fail=--fail-with-body
	fi
	command="curl $fail --silent --show-error --globoff \
		      -L --location-trusted --post301 --post302 \
		      -H Accept:application/json -X $method"
	if $attachment
	then
	      command="$command -H Content-Type:multipart/form-data"
	else
	      command="$command -H Content-Type:application/json"
	fi
	;;
    wget) # Only works for getting
	command="wget -nv -O - --header Accept:application/json"
	;;
    esac
    # I can't figure out how to handle arguments including a space in $command
    # and that makes it impossible to authenticate with an access token
    # or with username/password if the password contains a space.
    # For an access token that would be:
    #   curl -H "authorization: token $token"
    if [ -n "$username" ]
    then
	case $transfer in
	curl) command="$command -u $username:$password" ;;
	wget) command="$command --user=$username --password=$password" ;;
	esac
    fi

    case $transfer in
    curl)
	if $attachment
	then
	    $debug_URLs && \
		 echo "$command -F attachment=@-;filename=\"$(echo "$data" | \
				sed 's/[\"]/\\&/g')\"" "$url" 1>&2
	    # Read data from stdin because @$data messes with {} and []
	    result="$($command -F "attachment=@-;filename=\"$(echo "$data" | \
				sed 's/[\"]/\\&/g')\"" "$url" < "$data" 2> $errs)"
	    status=$?
	else
	    $debug_URLs && \
		 echo "$command --data-binary \"$data\" \"$url\"" 1>&2
	    result="$($command --data-binary "$data" "$url" 2> $errs)"
	    status=$?
	fi
	;;
    wget)
	result="$($command --post-data "$data" "$url" 2> $errs)"
	status=$?
    esac

    test $status != 0 && {
	echo "$message":
	cat $errs
	test -n "$result" && echo "$result" | $jq -r .message
    } 1>&2
    rm $errs
    test $status != 0 && return $status

    # Eliminate CR from CRLF and ensure it is newline-terminated
    echo "$(echo "$result" | tr -d '\r')"

    return 0
}

settings="$(geturl GET "$api/settings/api" "Failed to fetch API settings")" \
	    || exit 1
test -z "$settings" && exit 1

# Encode the value of a shell variable so that jq gets that as the same string
# so if we're handed a"\b it return "a\"\\b"
js_quote() {
    echo "$1" | sed 's|\\|\\\\|g;s|"|\\"|g;s|.*|"&"|'
}

# filename_quote: Make a local filename out of an issue's title:
# Unix: replace / with \
# Windows and MSDOS on FAT32 also ban : \ and ". Tough.
filename_quote() {
    echo "$1" | sed 's|/|\\|g'
}

# ECMA-encode a string, including the start and end quotes.
# See https://ecma-international.org/wp-content/uploads/ECMA-404.pdf
# "Section 9. String" or search "escaped".
#
# -n: No trailing newline
ecma_quote() {
    local minus_n
    # We should quote newline too but that's hard with sed.
    # Sed has hijacked \b as a word-edge pattern matcher.
    minus_n=
    case "$1" in
    -n) minus_n=-n; shift ;;
    esac

    echo "\"$(echo $minus_n "$1" | sed 's/["\\/]/\\&/g
		     s/\x08/\\b/g
		     s/\f/\\f/g
		     s/\r/\\r/g
		     s/\t/\\t/g' | tr '\n' '\r' | sed 's/\r/\\n/g')\""
}

# url_quote: Encode special characters in URLs
# Special characters are !*'();:@&=+$,/?#[]
url_quote() {
    echo "$*" | sed 's/%/%25/g
		     s/!/%21/g
		     s/#/%23/g
		     s/\$/%24/g
		     s/\&/%26/g
		     s/'\''/%27/g
		     s/(/%28/g
		     s/)/%29/g
		     s/\*/%2A/g
		     s/+/%2B/g
		     s/,/%2C/g
		     s:/:%2F:g
		     s/:/%3A/g
		     s/;/%3B/g
		     s/=/%3D/g
		     s/?/%3F/g
		     s/@/%40/g
		     s/\[/%5B/g
		     s/\]/%5D/g'
		     #s/}/%7B/g
		     #s/}/%7D/g
}

# Download all the remote issues into global variable $rissues_json
fetchissues() {
    local limit
    local page
    local issue_page
    local state

    # The API can only return a maximum of 50 items per request so
    # fetch each page until it returns [].
    # If you get "Argument list too long", reduce $limit
    # Limit is usually 50 giving 164k per page.
    limit="$(echo "$settings" | $jq -r '.max_response_items')"
    limit=20
    page=1
    case $open_only in
    true) state=open;;
    false) state=all;;
    esac

    # Fetch the first page
    rissues_json="$(geturl GET "$apirepo/issues?limit=$limit&page=$page&state=$state" \
		      "Failed to fetch the first page of issues")"
    test -z "$rissues_json" && exit 1

    # Append the following pages
    issue_page=""
    until [ "$issue_page" = "[]" ]
    do
	page=$((page + 1))
	issue_page="$(geturl GET \
			     "$apirepo/issues?limit=$limit&page=$page&state=$state" \
			     "Failed to fetch page $page of $limit issues")"

	# if the transfer failed or there are no more issues, stop
	test -z "$issue_page" && issue_page="[]"
	test "[]" = "$issue_page" && continue

	# Append the new page of issues to our list
	rissues_json="$(echo "$rissues_json" | $jq --arg new "$issue_page" '. + ($new | fromjson)')"
    done

    # Forgejo includes pull requests in the issues. Ignore them.
    rissues_json="$(echo "$rissues_json" | \
		    $jq 'del(.[] | select(.pull_request != null))')"

    if $dumpissues
    then
	echo "$rissues_json" | eval "$(echo "$jq" | sed 's/ -c//')" > issues.json
    fi
}

# Write remote issues into the local directory
# - delete all local issues
# - for each remote issues, save interesting fields and fetch attachments
getissues() {
    local a
    local dirname
    local limit
    local page
    local number
    local issue
    local title
    local ftitle
    local id
    local milestone
    local labels
    local assets
    local nassets
    local i
    local name
    local url

    # Remove any previously produced output
    for a in *.md .*.md
    do
	# Guard against issues whose title ends in .md
	test -d "$a" && continue

	# Guard against there being no .md files
	# though they may have created an issue called "*" (!)
	test "$a" = "*.md" && [ ! -f "*.md" ] && continue

	test -f "$a" && rm "$a"
	# If we were interrupted, we may have created an .md file
	# but not its corresponding directory so check first.
	dirname="$(echo "$a" | sed 's/\.md$//')"
	test -d "$dirname" && rm -r "$dirname"
    done

    echo "$rissues_json" | $jq -r '.[].number' | while read -r number
    do
	# The list of issues doesn't include the attachments (.assets) but
	# we can get that by fetching a single issue by its issue number.
	issue="$(geturl GET -r "$apirepo/issues/$number" \
			"#$number Failed to fetch list of assets")" || break
	if [ -z "$issue" ]
	then
	    continue
	fi

	title="$(echo "$issue" | $jq -r .title)"
	ftitle="$(filename_quote "$title")"

	# Check for duplicate titles
	if [ -f "$ftitle".md ]
	then
	    echo "Ignoring issue $number with the same title as $(cat "$ftitle"/number): '$ftitle'"
	    continue
	fi
	# Some issue bodies have all \n and others have all \r\n. Go figure.
	echo "$issue" | $jq -r .body | tr -d '\r' > "$ftitle".md

	# Save metadata: number, id, milestone and labels
	mkdir "$ftitle"
	if cd "$ftitle"
	then
	    # Forgejo includes pull requests as issues with a non-null
	    # .pull_request field. We don't want them in the issues.
	    if [ "$(echo "$issue" | $jq -r .pull_request)" != null ]
	    then
		cd ..
		continue
	    fi

	    # Items that are always present
	    echo "$title" > title
	    echo "$number" > number
	    id="$(echo "$issue" | $jq -r .id)"; echo "$id" > id
	    echo "$issue" | $jq -r .state > state
	    echo "$issue" | $jq -r .created_at > created_at
	    echo "$issue" | $jq -r .updated_at > updated_at
	    echo "$issue" | $jq -r .user.username > username

	    # Items that may be present
	    milestone="$(echo "$issue" | $jq -r .milestone)"
	    if [ -n "$milestone" ] && [ null != "$milestone" ]
	    then milestone="$(echo "$milestone" | jq -r .title)"
	    fi
	    if [ -n "$milestone" ] && [ null != "$milestone" ]
	    then echo "$milestone" > milestone
	    fi
	    labels="$(echo "$issue" | $jq -r ".labels|.[].name")"
	    test -n "$labels" && echo "$labels" > labels

	    assets="$(echo "$issue" | $jq '.assets')"  # json
	    nassets="$(echo "$assets" | $jq -r length)"
	    test "$nassets" -gt 0 && mkdir assets
	    for i in $(seq 0 $((nassets - 1))); do
		name="$(echo "$assets" | $jq -r ".[$i].name")"
		url="$(echo "$assets" | $jq -r ".[$i].browser_download_url")"
		case "$name" in
		*.patch|*.diff)
		    geturl GET "$url" \
			   "Failed to fetch patch '$name' of '$title'" ;;
		*)
		    geturl GET -r "$url" \
			   "Failed to fetch attachment '$name' of '$title'" ;;
		esac > assets/"$name"
	    done
	    cd ..
	else
	    echo "Can't cd to \"$ftitle\""
	fi
    done
}

# Push all local issues to the remote version
# - Go through all local issues, updating the corresponding remote ones.
#   If the remote one doesn't exist, create a new one and store the new
#   number and id locally
putissues() {
    local ftitle
    local title
    local number
    local id
    local state
    local milestone
    local labels
    local milestone_list
    local label_list
    local rasset
    local rasset_json
    local rassets_json
    local rtitle
    local rissues_json
    local a
    local ok
    local id
    local label
    local name
    local tmp
    local result
    local url
    local updating_body
    local new_issue_nums	# A list of issue numbers that need creating
    local newnumber		# The new issue number we actually got

    new_issue_nums=

    # Fetch the list of valid milestone titles and their ids
    # Each line is "name id".
    milestone_list="$(geturl GET "$apirepo/milestones" \
			     "Failed to fetch list of valid milestones" \
		      | $jq -r '.[] | .title + " " + (.id | tostring)')"

    # Fetch the list of valid label names and their ids
    label_list="$(geturl GET "$apirepo/labels" \
			     "Failed to fetch list of valid labels" \
		  | $jq -r '.[] | .name + " " + (.id | tostring)')"

    for ftitle in *.md
    do
	# If there are no .md files here, they got something wrong.
	# There could be an issue called "*" (!)
	if [ "$ftitle" = "*.md" ] && [ ! -e "*.md" ]
	then
	    echo "There are no issues here!"
	    exit 1
	fi

	# Guard against issues whose title ends in .md
	test -d "$ftitle" && continue

	# and files called ".md"
	if [ "$ftitle" = ".md" ]
	then
	    echo "Ignoring file '.md'"
	    continue
	fi

	# Hereon, $ftitle is the issue's directory name
	ftitle="$(basename "$ftitle" .md)"

	# Ignore any pull requests left over from when we used to store them
	if [ -f "$ftitle"/pull_request ]
	then
	    continue
	fi

	# New issues may not have a directory
	mkdir -p "$ftitle"

	# Read its metadata

	if [ -f "$ftitle/title" ]
	then
	    title="$(cat "$ftitle"/title)"
	else
	    # Probably a new issue
	    title="$ftitle"
	    # Check for invalid characters in the file name.
	    # On Unix they can't include / in the file name and
	    # Windows and MSDOS on FAT32 also ban : \ and ".
	    if echo "$ftitle" | grep -q '/:\\"'
	    then echo "Warning: On Windows, characters /\\:\" are invalid in ${ftitle}.md" 1>&2
	    fi
	    $dryrun || echo "$title" > "$ftitle"/title
	fi
	if [ -f "$ftitle"/number ]
	then number="$(cat "$ftitle"/number)"
	else number=
	fi
	if [ -f "$ftitle/id" ]
	then id="$(cat "$ftitle"/id)"
	else id=
	fi
	if [ -f "$ftitle/state" ]
	then state="$(cat "$ftitle"/state)"
	else state=open
	     $dryrun || echo open > "$ftitle"/state
	fi
	if [ -f "$ftitle/milestone" ]
	then milestone="$(cat "$ftitle"/milestone)"
	else milestone=
	fi
	if [ -f "$ftitle/labels" ]
	then labels="$(cat "$ftitle"/labels)"
	else labels=
	fi

	# Sanity checking of local issue

	if [ -z "$title" ]
	then
	    echo "$ftitle/title is empty"
	    continue
	fi
	# Make sure there are no newlines in its title
	if [ 1 != "$(echo "$title" | wc -l)" ]
	then
	    echo "There's a newline in the title of '$ftitle'" 1>&2
	    continue
	fi

	if [ -n "$number" ] && [ -z "$id" ]
	then
	    $force || {
		echo "#$number '$title': No id. Use -f?"
		continue
	    }
	fi
	if [ -n "$id" ] && [ -z "$number" ]
	then
	    echo "Id \"$id\" with no number in '$ftitle'"
	    continue
	fi

	case "$state" in
	open|closed) : ;;
	*) echo "Invalid state \"$state\" in '$ftitle'"
	   continue ;;
	esac

	# Check that the milestone is valid
	if [ -n "$milestone" ]
	then
	    ok=false
	    for a in $(echo "$milestone_list" | sed 's/ .*//')
	    do
		test "$a" = "$milestone" && ok=true
	    done
	    $ok || {
		echo "#$number '$ftitle': Invalid milestone \"$milestone\""
		continue
	    }
	fi

	# Check that every label is valid
	any_bad_labels=false
	for label in $labels
	do
	    ok=false
	    for a in $(echo "$label_list" | sed 's/ .*//')
	    do
		test "$label" = "$a" && ok=true
	    done
	    $ok || {
		echo "Invalid label '$label' in '$ftitle'"
		echo "Valid labels are: $(echo "$label_list" | sed 's/ .*//')"
		any_bad_labels=true
	    }
	done
	# Is a label repeated?
	labels="$(echo "$labels" | sort)"
	if [ "$labels" != "$(echo "$labels" | uniq)" ]
	then
	    echo "Warning: A label occurs twice in \"$ftitle\"/labels"
	    labels="$(echo "$labels" | uniq)"
	fi
	if $any_bad_labels
	then
	    # Remove the existing issue from the list of remote issues
	    # otherwise we'd delete it.
	    rissues_json="$(echo "$rissues_json" | \
		       $jq "del(.[] | select(.number == $number))")"
	    continue
	fi

#
# 1. New local issues
#

	if [ -z "$number" ]
	then
	    # It's a new issue.

	    # Make sure it doesn't have the same title as an existing issue
	    if [ "" != "$(echo "$rissues_json" | \
		$jq ".[] | select(.title == $(js_quote "$title"))")" ]
	    then
		echo "An issue with title '$title' already exists"
		continue
	    fi

	    data='{'	# }
	    data="$data,\"title\":$(ecma_quote -n "$title")"
	    data="$data,\"body\":$(ecma_quote "$(tr -d '\r' < "$ftitle.md")")"
	    if [ "$state" = closed ]
	    then
		data="$data,\"closed\":true"
	    fi
	    if [ -n "$milestone" ]
	    then
		id="$(echo "$milestone_list" | grep "^$milestone " | sed 's/.* //')"
		data="$data,\"milestone\":$id"
	    fi
	    if [ -n "$labels" ]
	    then
		list=
		for label in $labels
		do
		    id="$(echo "$label_list" | grep "^$label " | sed 's/.* //')"
		    list="$list,$id"
		done
		# There is at least one label so the initial "," is certain
		list="$(echo "$list" | sed 's/^,//')"
		data="$data,\"labels\":[$list]"
	    fi

	    echo "Creating new issue: $title"

	    $dryrun && continue

	    data="$(echo "$data" | sed 's/^{,/{/;s/$/}/')"    # }

	    result="$(puturl POST "$apirepo/issues" "$data" \
			     "Failed to create '$title'")"
	    status=$?
	    if [ $status -ne 0 ]
	    then echo "curl failed with status $status"
	    fi
	    if [ -z "$result" ]
	    then echo "Result is empty"
	    fi

	    if [ $status -ne 0 ] || [ -z "$result" ]
	    then continue
	    fi

	    # Store new metadata locally
	    number="$(echo "$result" | $jq -r .number)"
		echo "$number" > "$ftitle"/number
	    id="$(echo "$result" | $jq -r .id)"
		echo "$id" > "$ftitle"/id
	    echo "$result" | $jq -r .user.login > "$ftitle"/username
	    echo "$result" | $jq -r .created_at > "$ftitle"/created_at
	    echo "$result" | $jq -r .updated_at > "$ftitle"/updated_at

	    # You have to create the issue and then add attachments to it
	    if [ -d "$ftitle"/assets ]
	    then
		test -d "$ftitle"/assets && \
		(cd "$ftitle"/assets && find . -maxdepth 1 -type f | \
					sed 's|^\./||') | \
		while read -r name
		do
		    result="$(puturl POST -a \
		              "$apirepo/issues/$number/assets" \
			      "$ftitle/assets/$name" \
			      "#$number '$title': Failed to attach '$name'")"
		done
	        # That may have updated its updated_at field
		if result="$(geturl GET -r "$apirepo/issues/$number" \
				    "#$number '$title': Failed to refetch")"
		then
		    old="$(cat "$ftitle"/updated_at)"
		    new="$(echo "$result" | $jq -r .updated_at > "$ftitle"/updated_at)"
		    if [ "$old" != "$new" ]
		    then
			echo "Updating local last-modified time from $old to $new"
			echo "$result" | $jq -r .updated_at > "$ftitle"/updated_at
		    fi
		fi
	    fi

	    echo "https://$site/$owner/$repo/issues/$number"

	    continue
	fi

#
# 2. Local issues that have a remote correspondent
#

	# We know that the local issue has a number and an id.

	# Select the remote version of the issue
	rissue_json="$(echo "$rissues_json" | \
		       $jq ".[] | select(.number == $number)")"

	if [ -z "$rissue_json" ]
	then
	    # The local issue has a number
	    # but the issue doesn't exist on the remote server.
	    # This probably means we are updating an old copy
	    # of the repository that we pulled from.
	    # We can't create a new issue with a specified number
	    # so remember it and create them all afterwards.
	    new_issue_nums="$new_issue_nums $number"
	    continue
	fi

	# Check that the remote id corresponds.
	$force || if [ "$id" != "$(echo "$rissue_json" | $jq ".id")" ]
	then
	    echo "number/id mismatch in $ftitle"
	    # Remove the existing issue from the list of remote issues
	    # so that we don't delete it from the remote server
	    rissues_json="$(echo "$rissues_json" | $jq "del(.[] | select(.number == $number))")"
	    continue
	fi

	# Make sure older local copies don't overwrite issues that have been
	# updated on the server.

	if [ -f "$ftitle"/updated_at ] && \
	   [ "$(cat "$ftitle"/updated_at)" != \
	     "$(echo "$rissue_json" | $jq -r ".updated_at")" ]
	then
	    echo "Remote issue '$ftitle' has been updated since we got it" 1>&2
	    echo "Here: '$(cat "$ftitle"/updated_at)' There: '$(echo "$rissue_json" | $jq -r ".updated_at")'" 1>&2
	    continue
	fi

	# Update the remote issue from the local one.

	body="$(tr -d '\r' < "$ftitle".md)"

	data="{"	#}
	rtitle="$(echo "$rissue_json" | $jq -r .title)"
	if [ "$title" != "$rtitle"  ]
	then data="$data,\"title\":$(ecma_quote -n "$title")"
	fi
	rbody="$(echo "$rissue_json" | $jq -r .body | tr -d '\r')"
	if [ "$body" != "$rbody" ]
	then
	     data="$data,\"body\":$(ecma_quote "$body")"
	fi
	if [ -n "$milestone" ]
	then
	    id="$(echo "$milestone_list" | grep "^$milestone " | sed 's/.* //')"
	    rid="$(echo "$rissue_json" | $jq -r .milestone.id)"
	    if [ "$id" != "$rid" ]
	    then data="$data,\"milestone\":$id"
	    fi
	else
	    # There is no milestone here so ensure the remote doesn't have one
	    if [ null != "$(echo "$rissue_json" | $jq -r .milestone)" ]
	    then
		data="$data,\"milestone\":0"
		# This "succeeds" but the remote issue still has its milestone.
		# "milestone":0 doesn't work either.
		# https://stackoverflow.com/questions/79073569
		# https://codeberg.org/forgejo/discussions/issues/230
	    fi
	fi
	rstate="$(echo "$rissue_json" | $jq -r .state)"
	if [ "$state" != "$rstate" ]
	then data="$data,\"state\":\"$state\""
	fi

	# Unfortunately you can't change the created_at date or username
	# when PATCHing, only the updated_at time.

	data="$(echo "$data" | sed 's/^{,/{/;s/$/}/')"    # }

	if [ "{}" != "$data" ]
	then
	    url="$apirepo/issues/$number"

	    # Don't include the body text in the activity report
	    updating_body=false
	    echo "$data" | grep -q '"body":' && updating_body=true
	    a="$(echo "$data" | $jq "del(.body)")"
	    if [ -n "$data" ] && [ "{}" != "$data" ]
	    then
		echo_n "#$number '$title': Updating"
		if [ "{}" != "$a" ]
		then
		    if $updating_body
		    then	# both
			echo " body and '$a'"
		    else	# metadata but not body
			echo " '$a'"
		    fi
		else
		    if $updating_body
		    then	# just the body
			echo " body"
		    else	# Not updating anything. Shouldn't happen.
		        echo " nothing: Internal error"
		    fi
		fi
		$dryrun || \
		result="$(puturl PATCH "$url" "$data" \
				 "#$number '$title': Update failed")"
	    fi
	fi

	# Update the labels

	rlabels="$(echo "$rissue_json" | $jq -r ".labels|.[].name")"
	for label in $labels
	do
	    # Is the same label already on the issue?
	    echo "$rlabels" | grep -q -E "\\<$label\\>" || {
		# Nope. Add it.
		echo "#$number '$title': Adding label '$label'"
		url="$apirepo/issues/$number/labels"
		$dryrun || \
		result="$(puturl POST "$url" "{\"labels\":[\"$label\"]}" \
			         "#$number '$title': Failed to add label '$label'")"
	    }
	    # Remove the processed issue from the list of remote labels
	    rlabels="$(echo "$rlabels" | grep -v "^$label\$")"
	done
	# Remove remote labels that don't have a local equivalent
	for label in $rlabels
	do
	    echo "#$number '$title': Removing label '$label'"
	    # Labels are removed by id, not name.
	    id="$(echo "$label_list" | grep "^$label " | sed 's/.* //')"
	    $dryrun || \
	    result="$(geturl DELETE \
			     "$apirepo/issues/$number/labels/$id"
			     "#$number '$title': Failed to remove label '$label'")"
	done

	# Compare local and remote assets

	# Fetch a list of the remote issue's assets into $rassets_json
	if issue="$(geturl GET "$apirepo/issues/$number" \
		"#$number '$title': Failed to fetch list of assets")"
	then
	    rassets_json="$(echo "$issue" | $jq .assets)"
	else
	    # If we failed to fetch the list of remote assets for this issue
	    # don't update or delete any of them.
	    rassets_json=
	fi

	# Check that local assets are present remotely with the same content
	if [ -n "$rassets_json" ]
	then
	    # The "while read" loop runs in a subshell so
	    # to get a modified "$rassets_json" out of it
	    # we dump it in a temp file.
	    tmp=/tmp/putissues-"$number"-rassets_json$$
	    echo "$rassets_json" > "$tmp"

	    test -d "$ftitle"/assets && \
	    (cd "$ftitle"/assets && find . -maxdepth 1 -type f | \
				    sed 's|^\./||') | \
	    while read -r name
	    do
		rasset_json="$(echo "$rassets_json" | \
			       $jq ".[] | select(.name == $(js_quote "$name"))")"
		if [ -z "$rasset_json" ]
		then
		    # There is no remote asset with the same filename
		    echo "#$number '$title': Adding asset '$name'"
		    $dryrun || \
		    result="$(puturl POST -a \
			      "$apirepo/issues/$number/assets" \
			      "$ftitle/assets/$name" \
			      "#$number '$title': Failed to add asset '$name'")"
		else
		    # There is a remote asset with the same filename.
		    # Check that its content is the same.
		    #
		    # We could compare the size first as a quick check
		    # to know it is different without having to download it
		    # but as we strip CRs from patch files, even if the
		    # sizes are different (for a patch file) we'd have to
		    # download it anyway to see if it's the same text.
		    # For a non-patch file, it may be worth doing.

		    url="$(echo "$rasset_json" | $jq -r ".browser_download_url")"
		    # Binary files may contain nuls so dump it in a file
		    rasset=/tmp/putissues_rasset$$
		    case "$name" in
		    *.patch|*.diff)
			geturl GET "$url" \
			       "#$number '$title': Failed to fetch asset '$name'" > $rasset ;;
		    *)
			geturl GET -r "$url" \
			       "#$number '$title': Failed to fetch asset '$name'" > $rasset ;;
		    esac && cmp -s "$ftitle/assets/$name" "$rasset" || {
			# geturl succeeded and the files are different

			echo "#$number '$title': Updating asset '$name'"
			# We can only update assets by their id and
			# can't change the contents so we delete it
			# and recreate it.
			echo "$rasset_json" > "asset-$ftitle-$name"
			id="$(echo "$rasset_json" | $jq -r ".id")"
			$dryrun || \
			geturl DELETE "$apirepo/issues/$number/assets/$id" \
			       "#$number '$title': Failed to start replacing asset '$name'" && {
			$dryrun || \
			result="$(puturl POST -a \
				  "$apirepo/issues/$number/assets" \
				  "$ftitle/assets/$name" \
				  "#$number '$title': Failed to replace asset '$name'")"
			}
		    }
		    rm $rasset
		    # Remove the asset from the list of remote assets that
		    # do not have a local equivalent
		    rassets_json="$(echo "$rassets_json" | \
			       $jq "del(.[] | select(.name == $(js_quote "$name")))")"
		    echo "$rassets_json" > "$tmp"
		fi
	    done
	    rassets_json="$(cat "$tmp")"
	    rm "$tmp"

	    # If there is anything left in $rassets_json, they are
	    # remote assets that do not have a local equivalent
	    # so delete them.
	    # Alternative strategy: use [0] like for labels above,
	    echo "$rassets_json" | \
	    $jq -r '.[] | (.id | tostring) + " " + .name' | \
	    while read -r id name
	    do
		echo "#$number '$title': Removing asset '$name'"

		$dryrun || \
		result="$(geturl DELETE "$apirepo/issues/$number/assets/$id" \
				 "#$number '$title': Failed to delete asset '$name'")"
	    done
	fi

	# Remove the existing issue from the list of remote issues
	rissues_json="$(echo "$rissues_json" | $jq "del(.[] | select(.number == $number))")"
    done

    # The only remote issues left in the list are the ones
    # that don't have a local equivalent so delete them.
    while [ "[]" != "$rissues_json" ]
    do
	number="$(echo "$rissues_json" | $jq -r ".[0].number")"
	rtitle="$(echo "$rissues_json" | $jq -r ".[0].title")"

	if $allow_delete
	then
	    echo "#$number '$rtitle': Deleting remote issue"

	    $dryrun || \
	    result="$(geturl DELETE "$apirepo/issues/$number" \
		      "#$number '$rtitle': Failed to delete remote issue")"
	else
	    : "#$number '$rtitle' would have been deleted with -D"
	fi

	# Remove the first item from the list of issues to delete
	rissues_json="$(echo "$rissues_json" | $jq -r "del(.[0])")"
    done

    # That just leaves local issues that have a number but
    # don't exist on the remote server. They should all be greater than
    # the last remote issue number and when we create new ones
    # we should get the issue numbers we want.
    # The only exception is when the last remote issue has been deleted;
    # In that case we get a higher first new issue number
    # than we expected so watch out for that case.
    # Fortunately it's rare because the main use case is updating a
    # migration of a repository, and in that case final deleted issues
    # are not migrated so the first allocated new issue number is
    # highest_existing_issue_number + 1.
    #
    # The easiest strategy seems to be to create all the remote issues
    # we want and then fill in their details as a second pass,
    # deleting any holes in the sequence of new issue numbers,
    # which do by making a list of newly-created issue numbers,
    # removing them from that list as we fill them in,
    # then deleting any left in the list when we are done.
    #
    # - Find the highest issue number that we have to create
    # - Create dummy issues up to that number
    # - Generate a list of all numbers from the first to the last
    #   of the ones to create
    # - go through the list of issues to create,
    #   filling in the issue nunber in question and removing it from the list
    # - remove the dummy issues left in the list

    if [ -z "$new_issue_nums" ]
    then return 0
    fi

    sorted_issue_nums="$(echo $new_issue_nums | sed 's/ /\n/g' | sort -n)"
    highest="$(echo "$sorted_issue_nums" | tail -n 1)"
    lowest="$(echo "$sorted_issue_nums" | head -n 1)"
    new_issue_nums="$(seq "$lowest" "$highest")"

    if [ "$lowest" -eq "$highest" ]
    then echo "Creating new issue ${lowest}..."
    else echo "Creating new issues ${lowest}-${highest}..."
    fi
    $dryrun || \
    for number in $new_issue_nums
    do
	# Create an empty issue
	result="$(puturl POST "$apirepo/issues" "{\"title\":\"dummy\"}" "#$number: Failed to create a new remote issue")"
	test $? -ne 0 && break
	newnumber="$(echo "$result" | $jq .number)"
	if [ "$newnumber" -lt "$number" ]
	then
	    # There was a gap between the last remote issue number
	    # and the first one we have to create, so keep creating dummies
	    # until we get the number we wanted.
	    while [ "$newnumber" -lt "$number" ]
	    do
		geturl DELETE "$apirepo/issues/$newnumber" \
			      "#$number: Failed to delete dummy issue" > /dev/null
		result="$(puturl POST "$apirepo/issues" "{\"title\":\"dummy\"}" "#$number: Failed to create a new remote issue")"
		test $? -ne 0 && break
		newnumber="$(echo "$result" | $jq .number)"
	    done
	elif [ "$newnumber" -gt "$number" ]
	then
	    # The new issue number is higher than we expected.
	    echo "#${number}: Tried to create dummy issue but got #$newnumber instead"
	else
	    : The new issue number was what we expected
	fi
    done
    # Now go through the list of issues to create, filling in their details
    for number in $new_issue_nums
    do
	# Select a local issue by its number
	for ftitle in *.md
	do
	    # Guard against issues whose title ends in .md
	    test -d "$ftitle" && continue

	    # Guard against there being no .md files
	    # though they may have created an issue called "*" (!)
	    test "$a" = "*.md" && [ ! -f "*.md" ] && continue

	    # and files called ".md"
	    if [ "$ftitle" = ".md" ]
	    then echo "Ignoring file '.md'"; continue
	    fi

	    # Hereon, $ftitle is the issue's directory name
	    ftitle="$(basename "$ftitle" .md)"

	    test "$(cat "$ftitle"/number)" -ne "$number" && continue

	    data="{"	# }
	    title="$(cat "$ftitle"/title)"
	    data="$data,\"title\":$(ecma_quote -n "$title")"
	    data="$data,\"body\":$(ecma_quote "$(tr -d '\r' < "$ftitle".md)")"

	    # New issues by default are created open
	    if [ -f "$ftitle"/state ] && [ "$(cat "$ftitle"/state)" = closed ]
	    then data="$data,\"state\":\"closed\""
	    fi

	    # You can't set the created_at field or username when PATCHING
	    # an issue, nor when creating it.

	    milestone="$(cat "$ftitle"/milestone 2> /dev/null)"
	    if [ -n "$milestone" ]
	    then
		# Find the milestone id from its name
		id="$(echo "$milestone_list" | grep "^$milestone " | sed 's/.* //')"
		if [ -z "$id" ]
		then
		    echo "#$number '$title': Invalid milestone '$milestone'"
		    continue
		fi
		data="$data,\"milestone\":$id"
	    fi

	    # Check that every label is valid
	    for label in $labels
	    do
		ok=false
		for a in $(echo "$label_list" | sed 's/ .*//')
		do
		    test "$label" = "$a" && ok=true
		done
		$ok || {
		    echo "Invalid label \"$label\" in '$ftitle'"
		    echo "Valid labels are: $(echo "$label_list" | sed 's/ .*//')"
		}
	    done
	    # Is a label repeated?
	    labels="$(echo "$labels" | sort)"
	    if [ "$labels" != "$(echo "$labels" | uniq)" ]
	    then
		echo "Warning: A label occurs twice in \"$ftitle\"/labels"
		labels="$(echo "$labels" | uniq)"
	    fi

	    # In the middle of creation of nonexistent issues we'll just
	    # have to ignore the bad labels.

	    # Look up the ids of the labels
	    if [ -n "$labels" ]
	    then
		list=
		for label in $labels
		do
		    id="$(echo "$label_list" | grep "^$label " | sed 's/.* //')"
		    list="$list,$id"
		done
		# There is at least one label so the initial "," is certain
		list="$(echo "$list" | sed 's/^,//')"
		data="$data,\"labels\":[$list]"
	    fi

	    data="$(echo "$data" | sed 's/^{,/{/;s/$/}/')"	# }

	    echo "#$number '$title': Filling in new issue"

	    $dryrun || \
	    result="$(puturl PATCH "$apirepo/issues/$number" "$data" \
			     "#$number '$title': Failed to fill it in")"

	    $dryrun || \
	    if [ -d "$ftitle"/assets ]
	    then
		test -d "$ftitle"/assets && \
		(cd "$ftitle"/assets && find . -maxdepth 1 -type f | \
					sed 's|^\./||') | \
		while read -r name
		do
		    result="$(puturl POST -a \
		              "$apirepo/issues/$number/assets" \
			      "$ftitle/assets/$name" \
			      "#$number '$title': Failed to attach '$name'")"
		done
	    fi

	    # Remove it from the list of issues to delete afterwards.
	    # If something went wrong with the PATCH, do so as well
	    # so it isn't deleted and we can have a second chance at
	    # filling it in when we've fixed whatever was wrong.
	    new_issue_nums="$(echo "$new_issue_nums" | sed "s/\\<$number\\>//")"
	done
	# If we didn't find the local issue by number it's because
	# it's been deleted, so nothing extra to do; the dummy issue will go.
    done
    # Lastly, delete any issues that were created but not filled in:
    # the gaps in the sequence of new local issue numbers.
    $dryrun || \
    for number in $new_issue_nums
    do
	result="$(geturl DELETE "$apirepo/issues/$number" \
			 "#${number}: Failed to delete dummy issue")"
    done
}

fetchissues

case "$action" in
get)	getissues ;;
put)	putissues ;;
esac
