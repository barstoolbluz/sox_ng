/*
	This source code is part of the mod decoder for SoX
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

/*
 * A few definition, not special to Amiga Modules:
 */
#define bool sox_bool
#define true sox_true
#define false sox_false
#define LSBF 1
#define MSBF 2

typedef int8_t       s8;
typedef uint8_t      u8;
typedef int16_t      s16;
typedef uint16_t     u16;
typedef int32_t      s32;
typedef uint32_t     u32;

/* A common structure */
struct SampleStruct {
	/*
	 * Gives the name of the sample. This entry is used by many modules to
	 * say something about the module itself or about the creator of the
	 * module. If you want to show this, show the texts in one column, or
	 * show the first 16 in one column, and the last 15 right beside it in
	 * one other column, line thus:
	 *
	 *   Sample  0                  [Sample 16]
	 *   Sample  1                  [Sample 17]
	 *      .                           .
	 *      .                           .
	 *      .                           .
	 *   Sample 13                  [Sample 29]
	 *   Sample 14                  [Sample 30]
	 *   [Sample 15]
	 */
	char Name[22];
	u16  Size;       /* Tells the size of the sample in 16 bit words !!! */
	u8   Unknown2;   /* Guess! */
	u8   Volume;     /* The maximum volume is 0x40, this entry tells the */
	                 /* mod decoder, in which volume this sample should */
	                 /* be played, if nothing nothing else is told in the */
	                 /* pattern. */
	u16  LoopStart;  /* This is the start offset of a loop in the sample. */
	                 /* The loop offset must be multiplied with 2, like */
	                 /* the entry 'Size' in this structure. */
	u16  LoopLength; /* This is the num of 16 bit word, which should be */
	                 /* repeated on loop. */
};

/* The stucture of an old module: */
/*
 * The entries are the same as in structure 'ModuleStructure' and explaind
 * there. Please pay attention to the fact, that the entry 'Sample' is an array
 * of 15 entries here, and an array of 31 entries in the other structure, and
 * that the entry 'Signature' is not available here.
 */
struct OldModuleStructure {
	char                Name[20];
	struct SampleStruct Sample[15];
	u8                  NumRows;
	u8                  Unknown;
	u8                  Row[128];
};

/* The stucture of a new module: */
struct ModuleStructure {
	char                Name[20];   /* The name of a module */
	struct SampleStruct Sample[31]; /* See structure 'SampleStruct' */
	u8                  NumRows;    /* Tells how many entries of 'Row' */
	                                /* are valid */
	u8                  Unknown;    /* Guess! */
	u8                  Row[128];   /* This is an array what I call */
	                                /* 'Link List' (this is NOT an */
	                                /* official for it), see the file */
	                                /* mod.txt for more about it. */
	u32                 Signature;  /* Signature: "M.K.". Please look at */
	                                /* at the different endianness! */
};

/*
 * Every time a sample may be changed, there are used 4 bytes, which are not
 * split down here:
 */  
struct NoteStruct { unsigned char Byte[4]; };

/*
 * The notes are stored in patterns. Think about patterns like bars.
 *
 * One pattern stores information for (up to) 64 beats, and 4 channels!
 */
struct PatternStruct {
	struct NoteStruct Note[64][4];
};

/*
 * The frequencies in the struct 'NoteStruct' (see the functions
 * 'st_modplay_*' for more details) are not given clean and must be translated.
 * this is the struct of this translation table, and the table by itself is
 * stored in st_modtable.c. For more details about this table, risk a look at
 * the functions 'st_modplay_*' in mod.c!
 */
struct FrequencyTableStructure {
	u16   StartNote;
	u16   StoppNote;
	float Frequency;
        char *Name;
};
#define NumNotes 76
extern struct FrequencyTableStructure FrequencyTable[NumNotes];

/*
 * While playing the module we must save many information especially betreen
 * the calls of 'st_modplay'. All this information are stored in this
 * structure.
 */
struct ChannelType {
	s16   SampleToPlay;         /* Is the sample, which is played in the */
	                            /* moment */
	s16   Volume;               /* Is the volume of the sample currently */
	                            /* played. */
	u16   Mute:1;               /* Is set, all sample data are assumed to */
	                            /* be 0! (signed format) */
	u16   UserMuted:1;          /* Like 'Mute' */
	u16   VolumeSliding:1;      /* Is set, while volume sliding */
	u16   PortaToNote:1;        /* Tells, if the current porta is a porta */
	                            /* to a specific note (else infinite). */
	                            /* The note, where the porta should stop */
	                            /* is stored in 'DestinationFrequency' */
	u16   Porting:1;            /* Is set while ANY porta is being */
	                            /* performed. */
	s8    SlidingSpeed;         /* Speed of volume sliding */
	float SlidingDelay;         /* I don't know anymore, look at the */
	                            /* sources by yourself :-) */
	float SlidingDelayValue;    /* dito. */
	float SampleIndex;          /* Is the index of the sample, where the */
	                            /* next byte to be played must be read */
	                            /* from */
	float PlayingFrequency;     /* The frequency (here the correct after */
	                            /* translation via 'FrequencyTable'). */
	float DestinationFrequency; /* For porta to note, see 'PortaToNote' */
	float PortaSpeed;           /* For ALL portas: The speed of porting. */
};
/*
 * This macro is used to initialise the structure above. So, I don't forget to
 * set default values to them, if I add new entries - and they must be set so
 * or so anywhere.
 */
#define InitChannelEntry(i) {\
	_Channel[i].SampleToPlay=-1;\
	_Channel[i].Volume=0;\
	_Channel[i].Mute=1;\
	_Channel[i].UserMuted=0;\
	_Channel[i].VolumeSliding=0;\
	_Channel[i].PortaToNote=0;\
	_Channel[i].Porting=0;\
	_Channel[i].SlidingSpeed=0;\
	_Channel[i].SlidingDelay=0;\
	_Channel[i].SlidingDelayValue=0;\
	_Channel[i].SampleIndex=0;\
	_Channel[i].PlayingFrequency=440;\
	_Channel[i].DestinationFrequency=0;\
	_Channel[i].PortaSpeed=0;\
}

/*
 * A little bit about the nature of our music: We have 12 different notes. That
 * are called C, Cis, D, Dis, E, F, Fis, G, Gis, A, Ais, H (often called B,
 * too). Than everything is reapeatet, thus after H comes C again, but now an
 * octave higher. If you have any note, the same note exact one octave higher
 * appears to be as double so high as the note before. It's frequency is
 * mathematical the double of the frequency of the deeper note. The all known
 * (?) chamber tone (?) A has the frequency 440 Hz. The A one octave higher has
 * the frequency 880, the A one octave deeper has the frequency 220. Between
 * all notes to their successor and their predecessor are the same factor:
 * 1.05946... (look below), exact the 12th quare root from 2. The next table
 * gives this factors powered to 0, 1, 2, ..., 11. This value powerd to 12
 * would be 2 again, the note, which is as double so high as itself one ocatve
 * deeper.
 *                       12th square root of 2 powered to .. (in german)
 */
#define C   1.00000000000 /* 12te Wurzel aus 2 hoch  0 */
#define Cis 1.05946309436 /* 12te Wurzel aus 2 hoch  1 */
#define D   1.12246204831 /* 12te Wurzel aus 2 hoch  2 */
#define Dis 1.18920711500 /* 12te Wurzel aus 2 hoch  3 */
#define E   1.25992104989 /* 12te Wurzel aus 2 hoch  4 */
#define F   1.33483985417 /* 12te Wurzel aus 2 hoch  5 */
#define Fis 1.41421356237 /* 12te Wurzel aus 2 hoch  6 */
#define G   1.49830707687 /* 12te Wurzel aus 2 hoch  7 */
#define Gis 1.58740105196 /* 12te Wurzel aus 2 hoch  8 */
#define A   1.68179283050 /* 12te Wurzel aus 2 hoch  9 */
#define Ais 1.78179743627 /* 12te Wurzel aus 2 hoch 10 */
#define H   1.88774862535 /* 12te Wurzel aus 2 hoch 11 */

#define B   H  /* If somebody want to use this :-) */

/* What is this */
extern float SampleFrequency[16];

/*
 * The internal module loading function may return this error codes:
 */
#define LM_NOERR 0
#define LM_RDFIL 2
#define LM_NOMEM 3
#define LM_UNSPC 4
