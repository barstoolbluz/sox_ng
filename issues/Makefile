all:
	@echo "There's nothing here to make. Only 'make check' and 'make clean'"

check:
	@# Don't warn about "local"'s non-POSIXness
	@# or about echo "Message '$ftitle'"
	@# or about A && B || C is not if-then-else
	@# or about "useless echo" - we use it to ensure trailing newline.
	@shellcheck -e SC3043,SC2288,SC2015,SC2005 issues.sh

clean:
	@sh clean.sh -a
