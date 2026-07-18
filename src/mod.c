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

About this file
***************

In this document, I want to describe the Amiga Module Format and the playing of
a module for further patching by other people and to make them able to
understand the code. First to say: If you know what the wave format is, and you
know what the midi format is, think about a mix of this to files. This in short
is the Amiga Module format. For the others: One Amiga Module consists of tree
parts, the first is a table describing the samples and defining in which order
the patterns should be played. Then the patterns follows, a list for 64 Notes,
4 Channels simultaneous, defining which sample should be played in which
frequency how long and so on. The third part is an enumeration of the actually
samples.

Recording the Amiga Module format
*********************************

Because of the structure of the Amiga Module format, the mod decoder is only a
decoder. I don't know, is there is ANY mod encoder available world wide, but I
don't believe.

Playing the Amiga Module format
*******************************

To play an Amiga Module, I read the entire file into memory, and don't need to
access the file anymore. So, all the file input is done in 'st_modstartread',
the actual creating of the sample data is done in 'st_modread' in full 32 Bit
linear interpolating, best in the frequency of the output, or if not able to
detect in 44.1kHz.

Now, lets go into details
*************************

Loading the module
==================

Two different Amiga Module formats
----------------------------------

The first problem, which we must solve are the two different Amiga Module
formats. The old format allows up to 15 samples, the new allows up to 31
samples. This is the only difference, I know between them, but there may be
others (i. e. effects that are allowed in the new format only or different
semantics in effects like volume slide). The formats may be distinguished from
each other by looking at file offset 0x438: If the four bytes there are the
character constant "M.K.", it is the new (? Protracker ?) format, else it is
the old format. How to autodetect the old format? There is no save way to. One
can try to load the module, and check if it works. If there are too many
inconsistencies, one would say, it isn't an Amiga Module, else one would say it
is. (My code doesn't do any attempt to autodetect the old format. Only the new
format may be autodetected in the kind of asking 'Is it an Amiga Module? -
Yes/No' where 'Yes' means it is the new module format, and 'No' means, it is
the old module format or no Amiga Module anyway.)

Loading the module header
-------------------------

First, take a look at the file 'mod.h', and look for the structures
'SampleStruct', 'OldModuleStructure', and 'ModuleStructure'.

*/

#include "sox_i.h"
#include "mod.h"

#define Log(x,y) (log(y)/log(x))

static unsigned short Frequency = 44100; /* This MUSTN'T be a define!!! */

/*
 * FRAGE: Ist die Konstante 2.44 korrekt
 *        (ungefähr stimmt sie, das weiß ich,
 *        aber wie kommt sie zustande?) ...
 */
#define KONSTANTE1 2.44

/*
 * This variable should be moved to an global file!
 * (But then, move the definitions in mod.h, too.
 */
static bool endian=false; /* This is the beginning of fixing
                           * the bug of the endianness :-)
                           */
/*
 * The following pretendation is incorrect,
 * but I think, the joke should survive ...
 */

/* Private data for MOD file */
typedef struct {
    /* XXX: We have 320 bytes only!!!!!!!! */
    bool                   ModuleInitialised;
    struct ModuleStructure ModuleHead; /* HO HO */
    struct PatternStruct*  Pattern;
    char*                  Sample[2][31]; /* HO HO */
    int                    NumSamples;
    bool                   SampleEverPlayed[31]; /* HO HO */
    bool                   SamplePlaying[31]; /* HO HO */
    u16                    SpeedZ,SpeedN; 
    bool                   PatternEverPlayed[128]; /* HO HO */
    struct ChannelType     Channel[5]; /* HO HO */
    int                    i,j,k,l,l_end;
    int                    FadeOutPattern;
    int                    FadeOutRow;
    int                    FadingOut;
    int                    JumpToPatternCaused;
    unsigned               UserVolume;
} mod_t;

/*
 * _SpeedZ and _SpeedN are abbreviations for Speed Zähler and Speed Nenner. Ok,
 * Speed you understand, but 'Zähler' and 'Nenner' are German words. I don't
 * know the correct english words for that (maybe counter for Zähler and base
 * for Nenner, but I'm not sure) so let me give an example: The value 5/7 is a
 * rational but not natural value. The 5 is the Zähler, and the 7 is the
 * Nenner. To get a float value, divide 5 by 7 (something about 0.7).
 *
 * Do you know what I mean? If yes AND YOU HAVE ANYTHING ELSE TO SAY TO ME,
 * TOO, tell me what are the English word for it at <bothie@gmx.de>.
 */

#define _ModuleInitialised  (((mod_t *)(ft->priv))->ModuleInitialised)
#define _ModuleHead         (((mod_t *)(ft->priv))->ModuleHead)
#define _Pattern            (((mod_t *)(ft->priv))->Pattern)
#define _Sample             (((mod_t *)(ft->priv))->Sample)
#define _NumSamples         (((mod_t *)(ft->priv))->NumSamples)
#define _SampleEverPlayed   (((mod_t *)(ft->priv))->SampleEverPlayed)
#define _SamplePlaying      (((mod_t *)(ft->priv))->SamplePlaying)
#define _SpeedZ             (((mod_t *)(ft->priv))->SpeedZ)
#define _SpeedN             (((mod_t *)(ft->priv))->SpeedN)
#define _PatternEverPlayed  (((mod_t *)(ft->priv))->PatternEverPlayed)
#define _Channel            (((mod_t *)(ft->priv))->Channel)
#define FadeOutPattern      (((mod_t *)(ft->priv))->FadeOutPattern)
#define FadeOutRow          (((mod_t *)(ft->priv))->FadeOutRow)
#define FadingOut           (((mod_t *)(ft->priv))->FadingOut)
#define JumpToPatternCaused (((mod_t *)(ft->priv))->JumpToPatternCaused)
#define UserVolume          (((mod_t *)(ft->priv))->UserVolume)

/*
 * We haven't got any user interface anymore. So define
 * the defaults here and don't declare variables anymore
 */
#define UserSpeed  256.0

/*
 * A simple macro:
 *
 * I have some trouble under DOS on reading data: The function read reads a
 * specified num of bytes, BUT TELL TO HAVE READ A LESSER NUM OF BYTES. This
 * error appears in the function read() and write(), but not in the pedants
 * _read() and _write(), so I need to use that. The action seems to be the
 * same except the error.
 */
#ifdef DOS
#define read _read
#endif

#define Read(h,a,c) {\
    if (read((h),(a),(c))!=(c)) {\
        close(h);\
        h=-1;\
        return LM_RDFIL;\
    }\
}

/*
 * This function is called from mod_loadmodule() from
 * modstartread() and loads the samples into memory.
 */

int mod_loadsamples(sox_format_t *ft) {
    int i,Error=0;
    
    for (i=0;i<_NumSamples;i++) {
        register char TempByte;
        
        TempByte=((char*)&_ModuleHead.Sample[i].Size)[0];
        ((char*)&_ModuleHead.Sample[i].Size      )[0]=((char*)&_ModuleHead.Sample[i].Size      )[1];
        ((char*)&_ModuleHead.Sample[i].Size      )[1]=TempByte;
        _ModuleHead.Sample[i].Size<<=1;
        
        TempByte=((char*)&_ModuleHead.Sample[i].LoopStart)[0];
        ((char*)&_ModuleHead.Sample[i].LoopStart )[0]=((char*)&_ModuleHead.Sample[i].LoopStart )[1];
        ((char*)&_ModuleHead.Sample[i].LoopStart )[1]=TempByte;
        _ModuleHead.Sample[i].LoopStart<<=1;
        
        TempByte=((char*)&_ModuleHead.Sample[i].LoopLength)[0];
        ((char*)&_ModuleHead.Sample[i].LoopLength)[0]=((char*)&_ModuleHead.Sample[i].LoopLength)[1];
        ((char*)&_ModuleHead.Sample[i].LoopLength)[1]=TempByte;
        _ModuleHead.Sample[i].LoopLength<<=1;
        
        if (!_ModuleHead.Sample[i].Volume) _ModuleHead.Sample[i].Volume=64;
        
        if (_ModuleHead.Sample[i].Size) {
            _Sample[0][i]=malloc(_ModuleHead.Sample[i].Size);
            if (!_Sample[0][i]) {
                Error=LM_NOMEM;
            } else {
                /* Einige Modules sind in der Hinsicht fehlerhaft, als das
                 * einzelne Bytes einzelner Samples schlichtweg FEHLEN! (Was
                 * wahrscheinlich auf fehlerhafte Module-Editoren
                 * zurrückzuführen ist ...) Um das Laden dieser Module trotzdem
                 * zu ermöglichen, wird einfach ignoriert, ob die zu lesenden
                 * Bytes auch tatsächlich gelesen werden konnten ...
                 * INFO: Diesen Fehler gibt es auch in der anderen Richtung:
                 *       Einige Module definieren ZU VIELE Bytes, so daß am
                 *       Ende einige ignoriert werden.
                 */
                fread(_Sample[0][i],1,_ModuleHead.Sample[i].Size,ft->fp);
            }
            if (Error) {
                /* Fehler beim Belegen des Speicherblocks oder laden des Samples.
                 * Aber die bereits belegten Speicherblocks müssen wir trotzdem
                 * wieder frei geben ... */
                for (;i--;) if (_Sample[0][i]) free(_Sample[0][i]);
                return Error;
            }
        } else {
            _Sample[0][i]=0;
        }
    }
    for (;i<31;i++) _Sample[0][i]=0;

    return LM_NOERR;
}

/*
    This function is called from modstartread() and loads the entire
    module into memory.
*/
int mod_loadmodule(sox_format_t *ft) {
    int i,NumPatterns,rc,AlreadyRead;
    char* Buffer;
    
    if (!(Buffer=malloc(sizeof(_ModuleHead)))) {
        return LM_NOMEM;
    }
    
    /* No news are good news :-) - ignore the return code */
    fread(Buffer,1,sizeof(_ModuleHead),ft->fp);
    for (i=sizeof(_ModuleHead);i--;) ((char*)&_ModuleHead)[i]=Buffer[i];
    
    if (*(uint32_t*)&(Buffer[0x438])==*(uint32_t *)&"M.K.") {
        _NumSamples=31;
        AlreadyRead=0;
    } else {
        _NumSamples=15;
        _ModuleHead.Signature=0;
        for (i=130;i--;)
            ((char*)&_ModuleHead.NumRows)[i]=
            ((char*)&_ModuleHead.Sample[15])[i];
        AlreadyRead=sizeof(struct SampleStruct)*16+4;
    }
    
    NumPatterns=0;
    for (i=0;i<_ModuleHead.NumRows;i++) {
        NumPatterns=_ModuleHead.Row[i]>NumPatterns?_ModuleHead.Row[i]:NumPatterns;
    }
    NumPatterns++;
    if (!(_Pattern=malloc(sizeof(*_Pattern)*NumPatterns))) {
        free(Buffer);
        return LM_NOMEM;
    }
    
    for (i=AlreadyRead;i--;) {
        ((char*)_Pattern)[i]=
        (&Buffer[sizeof(_ModuleHead)-AlreadyRead])[i];
    }
    free(Buffer); /* Not used anymore! */
    fread(_Pattern,sizeof(*_Pattern)*NumPatterns-AlreadyRead,1,ft->fp);
    
    if ((rc=mod_loadsamples(ft))) free(_Pattern);
    return rc;
}

/*
 * Figure out, which endian is used on this machine
 */
static void check_endian(void) {
    u16 endian_test_value;
    
    if (!endian) {
        endian_test_value=0x1234;
        if (((char*)&endian_test_value)[0]==0x34) {
            endian=LSBF;
        } else {
            endian=MSBF;
        }
    }
}

/*
 * We simply read the entire module into memory (in
 * an extreme case, this may be something about 2MB)
 */
static int modstartread(sox_format_t * ft) {
    int i;
    
    ft->priv = (struct mod_t *)malloc(sizeof(mod_t));
    
    if (!ft->priv) {
        /* FIXME: Return with an appropriate error code on errors */
        fprintf(stderr,"stlib/mod.c: Not enough memory\n");
        exit(EFAULT);
    }
    check_endian();
    
    if (endian==MSBF) {
        fprintf(stderr,"Warning: big endian machine isn't supported well!\n");
    }
    
    _ModuleInitialised=false;
    
    ft->signal.rate     = Frequency;
    ft->signal.channels = 1;
    ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
    ft->encoding.bits_per_sample = 32;
    
    switch (mod_loadmodule(ft)) {
        /* FIXME: Return with an apropriate error code on errors */
        case LM_RDFIL:
            fprintf(stderr,"stlib/mod.c: Error reading from mod file\n");
            exit(EFAULT);
        
        case LM_NOMEM:
            fprintf(stderr,"stlib/mod.c: Not enough memory\n");
            exit(EFAULT);
        
        case LM_UNSPC:
            fprintf(stderr,"stlib/mod.c: Unknown error\n");
            exit(EFAULT);
    }
    
    for (i=0;i<31;i++) {
        _SampleEverPlayed[i]=0;
        _SamplePlaying[i]=0;
    }
    
    for (i=0;i<5;i++) {
        InitChannelEntry(i)
    }
    
    _SpeedZ=  6;
    _SpeedN=125;
    
    for (i=128;i--;) _PatternEverPlayed[i]=0;
    
    (((mod_t *)(ft->priv))->i)=-1;
    (((mod_t *)(ft->priv))->j)=63;
    (((mod_t *)(ft->priv))->l)=0;
    (((mod_t *)(ft->priv))->l_end)=0;
    FadeOutPattern=255;
    FadeOutRow=0;
    FadingOut=0;
    JumpToPatternCaused=0;
    UserVolume=64;
    
    _ModuleInitialised=true;
    
    return SOX_SUCCESS;
}

/*
 * And here, we only need to free all the memory we allocated elsewhere!
 */
static int modstopread(sox_format_t * ft) {
    int i;
    for (i=0;i<31;i++) if (_Sample[0][i]) free(_Sample[0][i]);
    free(_Pattern);
    return SOX_SUCCESS;
}

static float GetNoteFrequency(int Note) {
    int l;
    
    for (l=0;l<NumNotes;l++)
        if ((FrequencyTable[l].StartNote<=Note)
        &&  (FrequencyTable[l].StoppNote>=Note))
            return FrequencyTable[l].Frequency;
    
    return 1; /* Wenn nicht gefunden: Irgendetwas müssen wir zurrückliefern... */
}

#define i     (((mod_t *)(ft->priv))->i)
#define j     (((mod_t *)(ft->priv))->j)
#define k     (((mod_t *)(ft->priv))->k)
#define l     (((mod_t *)(ft->priv))->l)
#define l_end (((mod_t *)(ft->priv))->l_end)

/*
 * Ok, now the _real_ work beginns: Decode
 * the module part, which has been requested
 *
 * Don't try to understand this except that
 * you _really_ want to patch anything here!!
 *
 * XXX: Most of the things in this function are there from historical reasons.
 *      I will less or more late rewrite the ENTIRE CODE! So please tell me WHY
 *      you something pached, and not only THAT you patched something, so that
 *      I can use the changes on a rewrite, too.
 */
size_t modread(sox_format_t *ft, sox_sample_t *buf, size_t len) {
    size_t done=0;
    
    for(;done<len;done++) {
        float HalfBufferSample;
        
        if (l>=l_end) {
            int BreakPattern=0;
            int DontResetSampleIndex;
            int LastVolumeSlide;
            
            j++;
            if (j>=64) {
                i++;
                if (i>=_ModuleHead.NumRows) {
                    /*
                     * If we are called another time,
                     * we need to land here again :-)
                     */
                    i--;
                    j--;
                    return done;
                }
                j=0;
            }
            
            if ((FadeOutPattern==_ModuleHead.Row[i])
            &&  (FadeOutRow==j))
                FadingOut=true;
            
            for (k=0;k<4;k++) {
                unsigned char c0,c1,c2,c3;
                unsigned char NewSample,OldSample;
                c0=_Pattern[_ModuleHead.Row[i]].Note[j][k].Byte[0];
                c1=_Pattern[_ModuleHead.Row[i]].Note[j][k].Byte[1];
                c2=_Pattern[_ModuleHead.Row[i]].Note[j][k].Byte[2];
                c3=_Pattern[_ModuleHead.Row[i]].Note[j][k].Byte[3];
                OldSample=NewSample=_Channel[k].SampleToPlay;
                if ((c0&16) || (c2>>4)) NewSample=((c0&16)+(c2>>4)-1);
                if ((c0&16) || (c2>>4)) {
                    if (((c2&15)!=0x1)
                    &&  ((c2&15)!=0x2)
                    &&  ((c2&15)!=0x3)
                    &&  ((c2&15)!=0xA)
                    ) {
                        if (_Channel[k].SampleToPlay==((c0&16)+(c2>>4)-1)) {
                            _Channel[k].Mute=0;
                            _Channel[k].SampleIndex=0;
                        }
                    }
                    _Channel[k].SampleToPlay=(c0&16)+(c2>>4)-1;
                    if (((c2&0xF)!=0xA) || (c3&0xF)) {
                        _Channel[k].Volume=_ModuleHead.Sample[_Channel[k].SampleToPlay].Volume;
                    } else {
                        if ((c2&0xF)==0xA) {
                            if (c3&0xF0)
                                _Channel[k].Volume=0;
                        }
                    }
                    _Channel[k].VolumeSliding=0;
                }
                _Channel[k].Porting=0;
                DontResetSampleIndex=0;
                LastVolumeSlide=_Channel[k].VolumeSliding;
                _Channel[k].VolumeSliding=0;
                
/* Für Volume-Slide: */
#define STARTSLIDEDELAY ((256L*Frequency)/11025L)
                
                switch (c2&15) {
                    float Temp;

                    case 0x4: /* Vibrato */
                    case 0x6: /* Vibrato + Note */
                        /* Vibrato Effekt initiieren! */
                        if (((c2&15)==0x6) && (NewSample==OldSample)) goto Case3;
                        break;
                    case 0x0: /* Apr ... ? */
                        if (!c3) break;
                    case 0x7: /* Tremor */
                    case 0x8: /* Pan */
                        break;
                    case 0xE: /* Filter */
                        if ((c3&0xF0)==0xB0) {;
                            _Channel[k].VolumeSliding=1;
                            if (c3) {
                                _Channel[k].SlidingSpeed=-1;
                            } else {
                                if (!LastVolumeSlide) {
                                    _Channel[k].VolumeSliding=0;
                                    break;
                                }
                            }
                            _Channel[k].SlidingDelay=
                            _Channel[k].SlidingDelayValue=(5.0/(c3&0x0F?c3&0x0F:0.01))*STARTSLIDEDELAY;
                        }
                        break;
/*
    I have some trouble with the porta effect: I don't know how fast the
    porta must be performed on which porta speed values. Here are some
    defines with which I tried to get the answer, BUT THEY ARE _ALL_ WRONG
    (including the code currently used).
*/
                    case 0x1: /* Porta Up */

#define TempTerm \
    switch(_SpeedZ) {\
/*4/125*/\
        case 4: Temp=pow(1.0+1.0/(348*KONSTANTE1),c3*(float)_SpeedN/(float)Frequency); break;\
/*5/125*/\
        case 5: Temp=pow(1.0+1.0/(242*KONSTANTE1),c3*(float)_SpeedN/(float)Frequency); break;\
/*6/125*/\
        case 6: Temp=pow(1.0+1.0/(280*KONSTANTE1),c3*(float)_SpeedN/(float)Frequency); break;\
        default:Temp=pow(1.0+1.0/(280*KONSTANTE1),c3*(float)_SpeedN/(float)Frequency); break;\
    }
#define CalcPortaSpeed Temp
                        _Channel[k].Porting=1;
                        if (c3) {
                            TempTerm;
                            _Channel[k].PortaSpeed=CalcPortaSpeed;
                        } else {
                            if (_Channel[k].PortaSpeed<1) _Channel[k].PortaSpeed=2-_Channel[k].PortaSpeed;
                        }
                        _Channel[k].DestinationFrequency=1000000;
                        break;
                    case 0x2: /* Porta Down */
                        _Channel[k].Porting=1;
                        if (c3) {
                            TempTerm;
                            _Channel[k].PortaSpeed=2-CalcPortaSpeed;
                        } else {
                            if (_Channel[k].PortaSpeed>1) _Channel[k].PortaSpeed=2-_Channel[k].PortaSpeed;
                        }
                        _Channel[k].DestinationFrequency=1;
                        break;
Case3:
                    case 0x3: /* Porta To Note */
                        _Channel[k].Porting=1;
                        if (c3) {
                            TempTerm;
                            _Channel[k].PortaSpeed=CalcPortaSpeed;
                        }
                        _Channel[k].PortaToNote=(c0&15)||c1; /*  Ist nur ein Flag !!! */
                        break;
                    case 0x5: /* Tone + Volume */
                        _Channel[k].Porting=1;
                        goto CaseA;
                    case 0x9: /* Sample Offset */
                        _Channel[k].SampleIndex=(c3/256.0)*_ModuleHead.Sample[_Channel[k].SampleToPlay].Size;
                        _Channel[k].Mute=0;
                        DontResetSampleIndex=-1;
                        break;
CaseA:
                    case 0xA: /* Volume Slide */
                        _Channel[k].VolumeSliding=1;
                        if (c3) {
                            _Channel[k].SlidingSpeed=((int)(unsigned)(c3>>4)-(int)(unsigned)(c3&15));
                        } else {
                            _Channel[k].VolumeSliding=0;
                        }
                        _Channel[k].SlidingDelay=
                        _Channel[k].SlidingDelayValue=STARTSLIDEDELAY;
                        break;
                    case 0xB: /* Jump To Pattern */
                        BreakPattern=1;
                        JumpToPatternCaused=(c3&0x7F);
                        if (_PatternEverPlayed[JumpToPatternCaused]) {
                            if (FadeOutPattern==255) {
                                FadeOutPattern=_ModuleHead.Row[i];
                                FadeOutRow=0;
                            }
                        }
                        break;
                    case 0xC: /* Set Volume */
                        _Channel[k].Volume=c3>63?64:c3;
                        break;
                    case 0xD: /* Break Current Pattern */
                        BreakPattern=1;
                        break;
                    case 0xF: /* Set Speed */
                        if (c3) {
                            _SpeedZ=c3< 32?c3:_SpeedZ;
                            _SpeedN=c3>=32?c3:_SpeedN;
                        }
                        break;
                }
                if ((c0&15) || c1) {
                    if (_Channel[k].Porting && _Channel[k].PortaToNote) {
                        _Channel[k].DestinationFrequency=GetNoteFrequency((((int)(c0&15))<<8)+(int)c1);
                        if (_Channel[k].DestinationFrequency<_Channel[k].PlayingFrequency) {
                            _Channel[k].PortaSpeed=2-_Channel[k].PortaSpeed;
                        }
                        _Channel[k].PortaToNote=0;
                    } else {
                        _Channel[k].PlayingFrequency=GetNoteFrequency((((int)(c0&15))<<8)+(int)c1);
                        if (!_Channel[k].Porting) _Channel[k].DestinationFrequency=_Channel[k].PlayingFrequency;
                        _Channel[k].Mute=0;
                        if (!DontResetSampleIndex) _Channel[k].SampleIndex=0;
                    }
                }
            }
            /* Je nach 'Speed' muß hier mehr oder
             * weniger oft durchlaufen werden. */
            if (FadingOut) if (UserVolume) UserVolume--;
            
            l_end=((KONSTANTE1*Frequency)*_SpeedZ*UserSpeed)
                /(_SpeedN*0x100L);
            l=0;
            
            if (BreakPattern) j=63;
        }
        
        HalfBufferSample=0;
        for (k=0;k<5;k++) {
            if (!_Channel[k].Mute) {
                unsigned ints;
                float    faktors;
                int      ks,gs;
                
                if (_ModuleHead.Sample[_Channel[k].SampleToPlay].LoopStart!=0
                ||  _ModuleHead.Sample[_Channel[k].SampleToPlay].LoopLength>2) {
                    while ((unsigned long)_Channel[k].SampleIndex>=
                        ((unsigned long)_ModuleHead.Sample[_Channel[k].SampleToPlay].LoopStart)
                        +((unsigned long)_ModuleHead.Sample[_Channel[k].SampleToPlay].LoopLength)
                    ) {
                        _Channel[k].SampleIndex-=(unsigned long)_ModuleHead.Sample[_Channel[k].SampleToPlay].LoopLength;
                    }
                }
                if (_Channel[k].SampleIndex>=(float)_ModuleHead.Sample[_Channel[k].SampleToPlay].Size) {
                    _Channel[k].Mute=1;
                    _Channel[k].SampleIndex=0;
                    break;
                }
                ints=_Channel[k].SampleIndex;
                faktors=_Channel[k].SampleIndex-(float)ints;
                ks=_Sample[0][_Channel[k].SampleToPlay][ints  ];
                gs=_Sample[0][_Channel[k].SampleToPlay][ints+1];
                if (!_Channel[k].UserMuted) {
                    HalfBufferSample+=(
                        ks+((gs-ks)*faktors)
                    )*(
                        (
                            _Channel[k].Volume*(u32)UserVolume
                        )/(
                            64.0*64.0
                        )
                    );
                }
                _Channel[k].SampleIndex+=(
                    _Channel[k].PlayingFrequency
                    *SampleFrequency[_ModuleHead.Sample[_Channel[k].SampleToPlay].Unknown2&15]
                    /64.0
                    /(float)Frequency
                );
                if (_Channel[k].VolumeSliding) {
                    _Channel[k].SlidingDelay-=1*((float)256.0/UserSpeed);
                    if (_Channel[k].SlidingDelay<=0) {
                        _Channel[k].SlidingDelay=_Channel[k].SlidingDelayValue;
                        _Channel[k].Volume+=_Channel[k].SlidingSpeed;
                        if (_Channel[k].Volume< 0) _Channel[k].Volume= 0;
                        if (_Channel[k].Volume>64) _Channel[k].Volume=64;
                    }
                }
                if (_Channel[k].Porting) {
                    if (_Channel[k].PortaSpeed!=1.0) {
                        if (_Channel[k].PortaSpeed>1.0) { /* Porta Up (ggf. To Note) */
                            if ((_Channel[k].PlayingFrequency*=1+(_Channel[k].PortaSpeed-1)*((float)0x100L/UserSpeed))>_Channel[k].DestinationFrequency) {
                                _Channel[k].PlayingFrequency=_Channel[k].DestinationFrequency;
                            }
                        } else {                         /* Porta Down (ggf. To Note) */
                            if ((_Channel[k].PlayingFrequency*=1+(_Channel[k].PortaSpeed-1)*((float)0x100L/UserSpeed))<_Channel[k].DestinationFrequency) {
                                _Channel[k].PlayingFrequency=_Channel[k].DestinationFrequency;
                            }
                        }
                    }
                }
            }
        }
        HalfBufferSample/=4;
        HalfBufferSample+=0.5; /* Damit die Nachkommastellen gerundet, und
                                * nicht abgeschnitten werden! */
        if ((int)HalfBufferSample> 127) HalfBufferSample= 127;
        if ((int)HalfBufferSample<-128) HalfBufferSample=-128;
        *buf++=((u32)HalfBufferSample)<<24;
        
        l++;
    }
    return done;
}

LSX_FORMAT_HANDLER(mod)
{
	static char const * const names[] = {"mod", NULL};
	static sox_format_handler_t const handler = {
		SOX_LIB_VERSION_CODE, "Protracker Amiga Module", names, 0,
		modstartread, modread, modstopread,
		NULL, NULL, NULL,
		NULL, NULL, NULL, sizeof(mod_t),
        };

	return &handler;
}
