/*
	This source code is part of the mod decoder for the st library and sox
	Copyright (C) 2001 by Bodo Thiesen <bothie@gmx.de>
	
	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.
	
	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Amiga Module Decoder, Version 1.1.0 ALPHA
 */

#include "sox_i.h"
#include "mod.h"

/*
	In this file are only some tables.
*/

/*
	The names for the notes (like "C-7") were used in the original code
	which worked under DOS, and printed the paterns, which are currently
	played. There were printed this entries, too. I dosen't see any sense
	in trowing them away, because to comment, what this is, they are still
	good enough :-) - any maybe I will recode a function to print the rows
	under sox while decoding, and then we already have this entries ;-)
*/
struct FrequencyTableStructure FrequencyTable[NumNotes]={
	{0x000,0x000,(1<<(7+4))*C  ,"---"},
	{0x001,0x00D,(1<<(7+4))*C  ,"C-7"},
 
	{0x00E,0x00E,(1<<(6+4))*H  ,"B-6"},
	{0x00F,0x00F,(1<<(6+4))*Ais,"A#6"},
	{0x010,0x010,(1<<(6+4))*A  ,"A-6"},
	{0x011,0x011,(1<<(6+4))*Gis,"G#6"},
	{0x012,0x012,(1<<(6+4))*G  ,"G-6"},
	{0x013,0x013,(1<<(6+4))*Fis,"F#6"},
	{0x014,0x014,(1<<(6+4))*F  ,"F-6"},
	{0x015,0x015,(1<<(6+4))*E  ,"E-6"},
	{0x016,0x017,(1<<(6+4))*Dis,"D#6"},
	{0x018,0x018,(1<<(6+4))*D  ,"D-6"},
	{0x019,0x01A,(1<<(6+4))*Cis,"C#6"},
	{0x01B,0x01B,(1<<(6+4))*C  ,"C-6"},
	
	{0x01C,0x01D,(1<<(5+4))*H  ,"B-5"},
	{0x01E,0x01E,(1<<(5+4))*Ais,"A#5"},
	{0x01F,0x020,(1<<(5+4))*A  ,"A-5"},
	{0x021,0x022,(1<<(5+4))*Gis,"G#5"},
	{0x023,0x024,(1<<(5+4))*G  ,"G-5"},
	{0x025,0x027,(1<<(5+4))*Fis,"F#5"},
	{0x028,0x029,(1<<(5+4))*F  ,"F-5"},
	{0x02A,0x02C,(1<<(5+4))*E  ,"E-5"},
	{0x02D,0x02E,(1<<(5+4))*Dis,"D#5"},
	{0x02F,0x031,(1<<(5+4))*D  ,"D-5"},
	{0x032,0x034,(1<<(5+4))*Cis,"C#5"},
	{0x035,0x037,(1<<(5+4))*C  ,"C-5"},
	
	{0x038,0x03B,(1<<(4+4))*H  ,"B-4"},
	{0x03C,0x03E,(1<<(4+4))*Ais,"A#4"},
	{0x03F,0x042,(1<<(4+4))*A  ,"A-4"},
	{0x043,0x046,(1<<(4+4))*Gis,"G#4"},
	{0x047,0x04A,(1<<(4+4))*G  ,"G-4"},
	{0x04B,0x04F,(1<<(4+4))*Fis,"F#4"},
	{0x050,0x054,(1<<(4+4))*F  ,"F-4"},
	{0x055,0x059,(1<<(4+4))*E  ,"E-4"},
	{0x05A,0x05E,(1<<(4+4))*Dis,"D#4"},
	{0x05F,0x064,(1<<(4+4))*D  ,"D-4"},
	{0x065,0x06A,(1<<(4+4))*Cis,"C#4"},
	{0x06B,0x070,(1<<(4+4))*C  ,"C-4"},
	
	{0x071,0x077,(1<<(3+4))*H  ,"B-3"},
	{0x078,0x07E,(1<<(3+4))*Ais,"A#3"},
	{0x07F,0x086,(1<<(3+4))*A  ,"A-3"},
	{0x087,0x08E,(1<<(3+4))*Gis,"G#3"},
	{0x08F,0x096,(1<<(3+4))*G  ,"G-3"},
	{0x097,0x09F,(1<<(3+4))*Fis,"F#3"},
	{0x0A0,0x0A9,(1<<(3+4))*F  ,"F-3"},
	{0x0AA,0x0B3,(1<<(3+4))*E  ,"E-3"},
	{0x0B4,0x0BD,(1<<(3+4))*Dis,"D#3"},
	{0x0BE,0x0C9,(1<<(3+4))*D  ,"D-3"},
	{0x0CA,0x0D0,(1<<(3+4))*Cis,"C#3"},
	{0x0D1,0x0E1,(1<<(3+4))*C  ,"C-3"},
	
	{0x0E2,0x0EF,(1<<(2+4))*H  ,"B-2"},
	{0x0F0,0x0FD,(1<<(2+4))*Ais,"A#2"},
	{0x0FE,0x10C,(1<<(2+4))*A  ,"A-2"},
	{0x10D,0x11C,(1<<(2+4))*Gis,"G#2"},
	{0x11D,0x12D,(1<<(2+4))*G  ,"G-2"},
	{0x12E,0x13F,(1<<(2+4))*Fis,"F#2"},
	{0x140,0x152,(1<<(2+4))*F  ,"F-2"},
	{0x153,0x167,(1<<(2+4))*E  ,"E-2"},
	{0x168,0x17C,(1<<(2+4))*Dis,"D#2"},
	{0x17D,0x193,(1<<(2+4))*D  ,"D-2"},
	{0x194,0x1AB,(1<<(2+4))*Cis,"C#2"},
	{0x1AC,0x1C4,(1<<(2+4))*C  ,"C-2"},
	
	{0x1C5,0x1DF,(1<<(1+4))*H  ,"B-1"},
	{0x1E0,0x1FB,(1<<(1+4))*Ais,"A#1"},
	{0x1FC,0x219,(1<<(1+4))*A  ,"A-1"},
	{0x21A,0x239,(1<<(1+4))*Gis,"G#1"},
	{0x23A,0x25B,(1<<(1+4))*G  ,"G-1"},
	{0x25C,0x27F,(1<<(1+4))*Fis,"F#1"},
	{0x280,0x2A5,(1<<(1+4))*F  ,"F-1"},
	{0x2A6,0x2CF,(1<<(1+4))*E  ,"E-1"},
	{0x2D0,0x2F9,(1<<(1+4))*Dis,"D#1"},
	{0x2FA,0x327,(1<<(1+4))*D  ,"D-1"},
	{0x328,0x357,(1<<(1+4))*Cis,"C#1"},
	{0x358,0x389,(1<<(1+4))*C  ,"C-1"},
	
	{0x38A,0x3BF,(1<<(0+4))*H  ,"B-0"},
	{0x3C0,0xFFF,(1<<(0+4))*Ais,"A#0"}
};

float SampleFrequency[16]={
//-8  -7   -6   -5   -4   -3   -2   -1  0    1    2    3    4    5    6    7
                                      8363,8413,8500,8500,8600,8651,8700,8758,
7700,7800,7900,8000,8100,8169,8232,8280
};
