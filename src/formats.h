/* libSoX static formats list   (c) 2006-9 Chris Bagwell and SoX contributors
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*-------------------------- Static format handlers --------------------------*/

  FORMAT(aifc)
  FORMAT(aiff)
  FORMAT(al)
  FORMAT(au)
  FORMAT(avr)
  FORMAT(cdr)
  FORMAT(cvsd)
  FORMAT(cvu)
  FORMAT(dat)
  FORMAT(dsdiff)
  FORMAT(dsf)
  FORMAT(dvms)
  FORMAT(f4)
  FORMAT(f8)
  FORMAT(gsrt)
  FORMAT(hcom)
  FORMAT(htk)
  FORMAT(ima)
  FORMAT(la)
  FORMAT(lu)
  FORMAT(maud)
  FORMAT(nsp)
  FORMAT(nul)
  FORMAT(prc)
  FORMAT(raw)
  FORMAT(s1)
  FORMAT(s2)
  FORMAT(s3)
  FORMAT(s4)
  FORMAT(sf)
  FORMAT(sln)
  FORMAT(smp)
  FORMAT(sounder)
  FORMAT(soundtool)
  FORMAT(sox)
  FORMAT(sphere)
  FORMAT(svx)
  FORMAT(txw)
  FORMAT(u1)
  FORMAT(u2)
  FORMAT(u3)
  FORMAT(u4)
  FORMAT(ul)
  FORMAT(voc)
  FORMAT(vox)
  FORMAT(wav)
  FORMAT(wsd)
  FORMAT(wve)
  FORMAT(xa)

/*--------------------- Plugin or static format handlers ---------------------*/

#if defined HAVE_ALSA && (defined STATIC_ALSA || !defined HAVE_LIBLTDL)
  FORMAT(alsa)
#endif
#if defined HAVE_AMRNB && (defined STATIC_AMRNB || !defined HAVE_LIBLTDL)
  FORMAT(amr_nb)
#endif
#if defined HAVE_AMRWB && (defined STATIC_AMRWB || !defined HAVE_LIBLTDL)
  FORMAT(amr_wb)
#endif
#if defined HAVE_AO && (defined STATIC_AO || !defined HAVE_LIBLTDL)
  FORMAT(ao)
#endif
#if defined HAVE_COREAUDIO && (defined STATIC_COREAUDIO || !defined HAVE_LIBLTDL)
  FORMAT(coreaudio)
#endif
#if defined HAVE_FLAC && (defined STATIC_FLAC || !defined HAVE_LIBLTDL)
  FORMAT(flac)
#endif
#if defined HAVE_GSM && (defined STATIC_GSM || !defined HAVE_LIBLTDL)
  FORMAT(gsm)
#endif
#if defined HAVE_LPC10 && (defined STATIC_LPC10 || !defined HAVE_LIBLTDL)
  FORMAT(lpc10)
#endif
#if defined HAVE_MP3 && (defined STATIC_MP3 || !defined HAVE_LIBLTDL)
  FORMAT(mp3)
#endif
#if defined HAVE_OPUS && (defined STATIC_OPUS || !defined HAVE_LIBLTDL)
  FORMAT(opus)
#endif
#if defined HAVE_OSS && (defined STATIC_OSS || !defined HAVE_LIBLTDL)
  FORMAT(oss)
#endif
#if defined HAVE_PULSEAUDIO && (defined STATIC_PULSEAUDIO || !defined HAVE_LIBLTDL)
  FORMAT(pulseaudio)
#endif
#if defined HAVE_WAVEAUDIO && (defined STATIC_WAVEAUDIO || !defined HAVE_LIBLTDL)
  FORMAT(waveaudio)
#endif
#if defined HAVE_SNDIO && (defined STATIC_SNDIO || !defined HAVE_LIBLTDL)
  FORMAT(sndio)
#endif
#if defined HAVE_SNDFILE && (defined STATIC_SNDFILE || !defined HAVE_LIBLTDL)
  FORMAT(sndfile)
  FORMAT(caf)
  FORMAT(fap)
  FORMAT(mat4)
  FORMAT(mat5)
#if defined HAVE_SF_FORMAT_MPC2K
  FORMAT(mpc2k)
#endif
  FORMAT(paf)
  FORMAT(pvf)
  FORMAT(sd2)
  FORMAT(sds)
  FORMAT(w64)
  FORMAT(xi)
#endif
#if defined HAVE_SUN_AUDIO && (defined STATIC_SUN_AUDIO || !defined HAVE_LIBLTDL)
  FORMAT(sunau)
#endif
#if defined HAVE_OGG_VORBIS && (defined STATIC_OGG_VORBIS || !defined HAVE_LIBLTDL)
  FORMAT(vorbis)
#endif
#if defined HAVE_WAVPACK && (defined STATIC_WAVPACK || !defined HAVE_LIBLTDL)
  FORMAT(wavpack)
#endif

/*--------------------- Handlers using an external program -------------------*/

#if USING_FFMPEG
FORMAT(ffmpeg)
FORMAT(3g2)
FORMAT(3gp)
FORMAT(aac)
FORMAT(ac3)
FORMAT(adts)
FORMAT(adx)
FORMAT(ape)
FORMAT(apm)
FORMAT(aptx)
FORMAT(argo_asf)
FORMAT(asf)
FORMAT(ast)
FORMAT(avi)
FORMAT(dfpwm)
FORMAT(dts)
FORMAT(eac3)
FORMAT(f4v)
FORMAT(flv)
FORMAT(gxf)
FORMAT(ism)
FORMAT(kvag)
FORMAT(m4a)
FORMAT(m4v)
FORMAT(mkv)
FORMAT(mlp)
FORMAT(mov)
FORMAT(mp4)
FORMAT(mpeg)
FORMAT(mpegts)
FORMAT(mxf_opatom)
FORMAT(nut)
FORMAT(oga)
FORMAT(ra)
FORMAT(rm)
FORMAT(rso)
FORMAT(sbc)
FORMAT(smjpeg)
FORMAT(spdif)
FORMAT(spx)
FORMAT(tta)
FORMAT(vag)
FORMAT(wma)
FORMAT(wsaud)
FORMAT(wtv)

/* It handles the following formats if you use -t ffmpeg
caf
flac
ircam
mp2
mp3
ogg
sox
voc
w64
wv
*/
#endif /* USING_FFMPEG */
