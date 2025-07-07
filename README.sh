#!/bin/sh

cat > README << .
			   SoX: Sound eXchange
			   ===================

SoX (Sound eXchange) is another Swiss Army knife of sound processing tools:
it can convert sound files between many different file formats & audio devices,
and can apply many sound effects & transformations, as well as doing basic
analysis and providing input to more capable analysis and plotting tools.

SoX is licensed under the GNU GPL and GNU LGPL.  To be precise, the 'sox_ng'
and 'soxi_ng' programs are distributed under the GPL, while the library
'libsox_ng' (in which most of SoX's functionality resides) is dual-licensed.
Note that some optional components of libsox_ng are GPL only: if you use these,
you must use libsox_ng under the GPL.  See INSTALL for the list of optional
components and their licences.

If this distribution is of source code (as opposed to pre-built binaries),
then you will need to compile and install SoX as described in the 'INSTALL'
file.

Changes between this release and previous releases of SoX can be found in
the 'ChangeLog' file; a summary of the file formats and effects supported in
this release can be found below.  Detailed documentation for using SoX can
be found in the distributed 'man' pages:
  o  sox_ng(1)
  o  soxi_ng(1)
  o  soxformat_ng(7)
  o  libsox_ng(3)

or in plain text or PDF files for those systems without man.

The majority of SoX features and fixes are contributed by SoX users - thank
you very much for making SoX a success!  There are several new features
wanted for SoX, listed on the feature request tracker at the SoX
project home-page:

		    https://codeberg.org/sox_ng/sox_ng/issues

users are encouraged to implement them!

Please submit bug reports, new feature requests, and patches to the relevant
tracker at the above address, or by email:

		   mailto:sox-ng@groups.io

.
cat FEATURES.in \
	| sed "s/!!/			/" \
	| sed "s/^\*\*/    o /" \
	| sed "s/^\*/  o /" \
	| sed "s/(:[a-zA-Z0-9_= -]*:)//g" \
       	>> README
