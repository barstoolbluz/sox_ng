# Copyright (C) 2006 Reuben Thomas <rrt@sc3d.org>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libdolbyb lpc10 libgsm src test
DIST_SUBDIRS = libdolbyb lpc10 libgsm src msvc9 msvc10 test

RM = rm -f

# man pages are not considered to be sources, so need to add "dist_"
# prefix to ensure they are added to the distribution.
dist_man_MANS = sox_ng.1 soxi_ng.1 soxformat_ng.7 libsox_ng.3
EXTRA_DIST = sox_ng.txt soxi_ng.txt soxformat_ng.txt libsox_ng.txt \
	     CMakeLists.txt sox_ng.pc.in COPYING AUTHORS INSTALL \
	     README.win32 README.osx scripts/batch-example.bat \
	     README.md RULES.md RULES-issues.md RULES-wiki.md \
	     README.sh FEATURES.in test/* issues wiki/*

if HAVE_PKGCONFIG
pkgconfig_DATA = sox_ng.pc
endif

play_ng.1 rec_ng.1: sox_ng.1
	@$(RM) $@ && $(LN_S) $< $@

README: README.sh FEATURES.in
	./README.sh

# Rule for making text man pages.  Uses GNU nroff's -c so not portable.
.1.txt:
	tbl $(srcdir)/$< | LANG=C LC_ALL=C nroff -man -c | sed "s/.//g" > $@
.3.txt:
	tbl $(srcdir)/$< | LANG=C LC_ALL=C nroff -man -c | sed "s/.//g" > $@
.7.txt:
	tbl $(srcdir)/$< | LANG=C LC_ALL=C nroff -man -c | sed "s/.//g" > $@

DOCTXT = sox_ng.txt soxi_ng.txt soxformat_ng.txt libsox_ng.txt README
txt: $(DOCTXT)

# Rule for making PDF man pages
.1.pdf:
	pdfroff -t -man --no-toc-relocation -Tps $(srcdir)/$< > $@
.3.pdf:
	pdfroff -t -man --no-toc-relocation -Tps $(srcdir)/$< > $@
.7.pdf:
	pdfroff -t -man --no-toc-relocation -Tps $(srcdir)/$< > $@

DOCPDF = sox_ng.pdf soxi_ng.pdf soxformat_ng.pdf libsox_ng.pdf
pdf: $(DOCPDF)

install-pdf: pdf
	@$(NORMAL_INSTALL)
	test -z "$(pdfdir)" || $(MKDIR_P) "$(DESTDIR)$(pdfdir)"
	@list='$(DOCPDF)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f=$(am__strip_dir) \
	  echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(pdfdir)/$$f'"; \
	  $(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(pdfdir)/$$f"; \
	  done

uninstall-pdf:
	@$(NORMAL_UNINSTALL)
	@list='$(DOCPDF)'; for p in $$list; do \
	  f=$(am__strip_dir) \
	  echo " $(RM) '$(DESTDIR)$(pdfdir)/$$f'"; \
	  $(RM) "$(DESTDIR)$(pdfdir)/$$f"; \
	  done

distclean-local:
	$(RM) mingw32-config.cache
	$(RM) -r -f soxpng

CLEANFILES = $(DOCTXT) $(DOCPDF) README

# Rule for making HTML man pages
#
# Get rid on unneeded Content-Type, man directory and manpage index in extension.
# e.g. ../man1/soxi_ng.1.html becomes ../soxi_ng.html
# Return to Main Contents and link to man2html are also modified.
FIXHTML = sed 's,<head>,&<link rel="stylesheet" href="http://sox.sourceforge.net/pub/skins/sox/sox.css" type="text/css"></link>,' 

.1.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@
.3.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@
.7.html:
	groff -t -man -Thtml -P -Dsoxpng $(srcdir)/$< | $(FIXHTML) > $@

DOCHTML = sox_ng.html soxi_ng.html soxformat_ng.html libsox_ng.html
html: $(DOCHTML)

install-html: html 	 
	@$(NORMAL_INSTALL) 	 
	test -z "$(htmldir)/soxpng" || $(MKDIR_P) "$(DESTDIR)$(htmldir)/soxpng"
	@list='$(DOCHTML)'; for p in $$list; do \
	if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	f=$(am__strip_dir) \
	echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(htmldir)//$$f'"; \
	$(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(htmldir)/$$f"; \
	done; \
	@for p in soxpng/*; do \
	if test -f "$$p"; then d=; else d="$(srcdir)"; fi; \
	f=$(am__strip_dir) \
	echo " $(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(htmldir)/soxpng/$$f'"; \
	$(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(htmldir)/soxpng/$$f"; \
	done

uninstall-html: 	 
	@$(NORMAL_UNINSTALL)
	@list='$(DOCHTML)'; for p in $$list; do \
	f=$(am__strip_dir) \
	echo " $(RM) '$(DESTDIR)$(htmldir)/$$f'"; \
	$(RM) "$(DESTDIR)$(htmldir)/$$f"; \
	done; \
	$(RM) -fr "$(DESTDIR)$(htmldir)/soxpng";

DISTCLEANFILES = $(DOCHTML) $(DOCPDF)
MAINTAINERCLEANFILES = $(DOCTXT)

install-data-hook:
	@if test "$(PLAYRECLINKS)" = "yes"; then	\
	    cd $(DESTDIR)$(mandir)/man1 && $(RM) play_ng.1 && $(LN_S) sox_ng.1 play_ng.1; \
	    cd $(DESTDIR)$(mandir)/man1 && $(RM) rec_ng.1 && $(LN_S) sox_ng.1 rec_ng.1; \
	fi
	@cd $(DESTDIR)$(mandir)/man7 && $(RM) soxeffect_ng.7 && $(LN_S) ../man1/sox_ng.1 soxeffect_ng.7
	@if test "$(REPLACE)" = "yes"; then \
		cd $(DESTDIR)$(mandir)/man1 && $(RM) sox.1 && $(LN_S) sox_ng.1 sox.1; \
		cd $(DESTDIR)$(mandir)/man3 && $(RM) libsox.3 && $(LN_S) libsox_ng.3 libsox.3; \
		cd $(DESTDIR)$(mandir)/man7 && $(RM) soxformat.7 && $(LN_S) soxformat_ng.7 soxformat.7; \
		cd $(DESTDIR)$(mandir)/man7 && $(RM) soxeffect.7 && $(LN_S) soxeffect_ng.7 soxeffect.7; \
		if test "$(PLAYRECLINKS)" = "yes"; then	\
			cd $(DESTDIR)$(mandir)/man1 && $(RM) play.1 rec.1 && \
			    $(LN_S) play_ng.1 play.1 && \
			    $(LN_S) rec_ng.1 rec.1; \
		fi; \
		if test "$(SYMLINKS)" = "yes"; then \
			cd $(DESTDIR)$(mandir)/man1 && $(RM) soxi.1 && \
			    $(LN_S) soxi_ng.1 soxi.1; \
		fi; \
		if test "$(pkgconfigdir)"; then \
			cd $(DESTDIR)$(pkgconfigdir); \
			$(RM) "sox.pc"; $(LN_S) sox_ng.pc sox.pc; \
		fi; \
	fi

uninstall-hook:
	@if test "$(PLAYRECLINKS)" = "yes"; then	\
		$(RM) $(DESTDIR)$(mandir)/man1/play_ng.1; \
		$(RM) $(DESTDIR)$(mandir)/man1/rec_ng.1; \
	fi
	@$(RM) $(DESTDIR)$(mandir)/man7/soxeffect_ng.7
	@if test "$(REPLACE)" = "yes"; then \
		$(RM) $(DESTDIR)$(mandir)/man1/sox.1; \
		$(RM) $(DESTDIR)$(mandir)/man3/libsox.3; \
		$(RM) $(DESTDIR)$(mandir)/man7/soxformat.7; \
		$(RM) $(DESTDIR)$(mandir)/man7/soxeffect.7; \
		if test "$(PLAYRECLINKS)" = "yes"; then	\
			$(RM) $(DESTDIR)$(mandir)/man1/play.1; \
			$(RM) $(DESTDIR)$(mandir)/man1/rec.1; \
		fi; \
		if test "$(SYMLINKS)" = "yes"; then \
			$(RM) $(DESTDIR)$(mandir)/man1/soxi.1; \
		fi; \
		if test "$(pkgconfigdir)"; then \
			cd $(DESTDIR)$(pkgconfigdir); \
			$(RM) "sox.pc"; \
		fi; \
	fi

# Automatically update libtool script if it gets out-of-date
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

winexe-prepare:
	test -d mxe || git clone https://github.com/mxe/mxe
	rm -f *.tar.gz
	$(MAKE) -s dist
	cp sox_ng-*.tar.gz mxe/pkg/
	sed "/_VERSION *:= /s/:=.*/:= $(PACKAGE_VERSION)/" sox_ng.mk | \
	sed '/_CHECKSUM *:= /s/:=.*/:= '\
"`sha256sum sox_ng-*.tar.gz | sed 's/[ \t].*//'`"/ > mxe/src/sox_ng.mk

win32.exe: winexe-prepare
	cd mxe && make MXE_TARGETS=i686-w64-mingw32.static sox_ng
	cp mxe/usr/i686-w64-mingw32.static/bin/sox_ng.exe win32.exe

win64.exe: winexe-prepare
	cd mxe && make MXE_TARGETS=x86_64-w64-mingw32.static sox_ng
	cp mxe/usr/x86_64-w64-mingw32.static/bin/sox_ng.exe win64.exe
