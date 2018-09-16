taglib_BASE = ../taglib/taglib

taglib_HEADERS = \
  $$taglib_BASE/tag.h \
  $$taglib_BASE/fileref.h \
  $$taglib_BASE/audioproperties.h \
  $$taglib_BASE/taglib_export.h \
  $$taglib_BASE/taglib_config.h \
  $$taglib_BASE/config.h \
  $$taglib_BASE/toolkit/taglib.h \
  $$taglib_BASE/toolkit/tstring.h \
  $$taglib_BASE/toolkit/tlist.h \
  $$taglib_BASE/toolkit/tlist.tcc \
  $$taglib_BASE/toolkit/tstringlist.h \
  $$taglib_BASE/toolkit/tbytevector.h \
  $$taglib_BASE/toolkit/tbytevectorlist.h \
  $$taglib_BASE/toolkit/tbytevectorstream.h \
  $$taglib_BASE/toolkit/tiostream.h \
  $$taglib_BASE/toolkit/tfile.h \
  $$taglib_BASE/toolkit/tfilestream.h \
  $$taglib_BASE/toolkit/tmap.h \
  $$taglib_BASE/toolkit/tmap.tcc \
  $$taglib_BASE/toolkit/tpropertymap.h \
  $$taglib_BASE/toolkit/trefcounter.h \
  $$taglib_BASE/toolkit/tdebuglistener.h \
  $$taglib_BASE/mpeg/mpegfile.h \
  $$taglib_BASE/mpeg/mpegproperties.h \
  $$taglib_BASE/mpeg/mpegheader.h \
  $$taglib_BASE/mpeg/xingheader.h \
  $$taglib_BASE/mpeg/id3v1/id3v1tag.h \
  $$taglib_BASE/mpeg/id3v1/id3v1genres.h \
  $$taglib_BASE/mpeg/id3v2/id3v2extendedheader.h \
  $$taglib_BASE/mpeg/id3v2/id3v2frame.h \
  $$taglib_BASE/mpeg/id3v2/id3v2header.h \
  $$taglib_BASE/mpeg/id3v2/id3v2synchdata.h \
  $$taglib_BASE/mpeg/id3v2/id3v2footer.h \
  $$taglib_BASE/mpeg/id3v2/id3v2framefactory.h \
  $$taglib_BASE/mpeg/id3v2/id3v2tag.h \
  $$taglib_BASE/mpeg/id3v2/frames/attachedpictureframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/commentsframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/eventtimingcodesframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/generalencapsulatedobjectframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/ownershipframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/popularimeterframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/privateframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/relativevolumeframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/synchronizedlyricsframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/textidentificationframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/uniquefileidentifierframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/unknownframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/unsynchronizedlyricsframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/urllinkframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/chapterframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/tableofcontentsframe.h \
  $$taglib_BASE/mpeg/id3v2/frames/podcastframe.h \
  $$taglib_BASE/ogg/oggfile.h \
  $$taglib_BASE/ogg/oggpage.h \
  $$taglib_BASE/ogg/oggpageheader.h \
  $$taglib_BASE/ogg/xiphcomment.h \
  $$taglib_BASE/ogg/vorbis/vorbisfile.h \
  $$taglib_BASE/ogg/vorbis/vorbisproperties.h \
  $$taglib_BASE/ogg/flac/oggflacfile.h \
  $$taglib_BASE/ogg/speex/speexfile.h \
  $$taglib_BASE/ogg/speex/speexproperties.h \
  $$taglib_BASE/ogg/opus/opusfile.h \
  $$taglib_BASE/ogg/opus/opusproperties.h \
  $$taglib_BASE/flac/flacfile.h \
  $$taglib_BASE/flac/flacpicture.h \
  $$taglib_BASE/flac/flacproperties.h \
  $$taglib_BASE/flac/flacmetadatablock.h \
  $$taglib_BASE/ape/apefile.h \
  $$taglib_BASE/ape/apeproperties.h \
  $$taglib_BASE/ape/apetag.h \
  $$taglib_BASE/ape/apefooter.h \
  $$taglib_BASE/ape/apeitem.h \
  $$taglib_BASE/mpc/mpcfile.h \
  $$taglib_BASE/mpc/mpcproperties.h \
  $$taglib_BASE/wavpack/wavpackfile.h \
  $$taglib_BASE/wavpack/wavpackproperties.h \
  $$taglib_BASE/trueaudio/trueaudiofile.h \
  $$taglib_BASE/trueaudio/trueaudioproperties.h \
  $$taglib_BASE/riff/rifffile.h \
  $$taglib_BASE/riff/aiff/aifffile.h \
  $$taglib_BASE/riff/aiff/aiffproperties.h \
  $$taglib_BASE/riff/wav/wavfile.h \
  $$taglib_BASE/riff/wav/wavproperties.h \
  $$taglib_BASE/riff/wav/infotag.h \
  $$taglib_BASE/asf/asffile.h \
  $$taglib_BASE/asf/asfproperties.h \
  $$taglib_BASE/asf/asftag.h \
  $$taglib_BASE/asf/asfattribute.h \
  $$taglib_BASE/asf/asfpicture.h \
  $$taglib_BASE/mp4/mp4file.h \
  $$taglib_BASE/mp4/mp4atom.h \
  $$taglib_BASE/mp4/mp4tag.h \
  $$taglib_BASE/mp4/mp4item.h \
  $$taglib_BASE/mp4/mp4properties.h \
  $$taglib_BASE/mp4/mp4coverart.h \
  $$taglib_BASE/mod/modfilebase.h \
  $$taglib_BASE/mod/modfile.h \
  $$taglib_BASE/mod/modtag.h \
  $$taglib_BASE/mod/modproperties.h \
  $$taglib_BASE/it/itfile.h \
  $$taglib_BASE/it/itproperties.h \
  $$taglib_BASE/s3m/s3mfile.h \
  $$taglib_BASE/s3m/s3mproperties.h \
  $$taglib_BASE/xm/xmfile.h \
  $$taglib_BASE/xm/xmproperties.h
  
mpeg_SOURCES = \
  $$taglib_BASE/mpeg/mpegfile.cpp \
  $$taglib_BASE/mpeg/mpegproperties.cpp \
  $$taglib_BASE/mpeg/mpegheader.cpp \
  $$taglib_BASE/mpeg/xingheader.cpp

id3v1_SOURCES = \
  $$taglib_BASE/mpeg/id3v1/id3v1tag.cpp \
  $$taglib_BASE/mpeg/id3v1/id3v1genres.cpp

id3v2_SOURCES = \
  $$taglib_BASE/mpeg/id3v2/id3v2framefactory.cpp \
  $$taglib_BASE/mpeg/id3v2/id3v2synchdata.cpp \
  $$taglib_BASE/mpeg/id3v2/id3v2tag.cpp \
  $$taglib_BASE/mpeg/id3v2/id3v2header.cpp \
  $$taglib_BASE/mpeg/id3v2/id3v2frame.cpp \
  $$taglib_BASE/mpeg/id3v2/id3v2footer.cpp \
  $$taglib_BASE/mpeg/id3v2/id3v2extendedheader.cpp

frames_SOURCES = \
  $$taglib_BASE/mpeg/id3v2/frames/attachedpictureframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/commentsframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/eventtimingcodesframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/generalencapsulatedobjectframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/ownershipframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/popularimeterframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/privateframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/relativevolumeframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/synchronizedlyricsframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/textidentificationframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/uniquefileidentifierframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/unknownframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/unsynchronizedlyricsframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/urllinkframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/chapterframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/tableofcontentsframe.cpp \
  $$taglib_BASE/mpeg/id3v2/frames/podcastframe.cpp

ogg_SOURCES = \
  $$taglib_BASE/ogg/oggfile.cpp \
  $$taglib_BASE/ogg/oggpage.cpp \
  $$taglib_BASE/ogg/oggpageheader.cpp \
  $$taglib_BASE/ogg/xiphcomment.cpp

vorbis_SOURCES = \
  $$taglib_BASE/ogg/vorbis/vorbisfile.cpp \
  $$taglib_BASE/ogg/vorbis/vorbisproperties.cpp

flacs_SOURCES = \
  $$taglib_BASE/flac/flacfile.cpp \
  $$taglib_BASE/flac/flacpicture.cpp \
  $$taglib_BASE/flac/flacproperties.cpp \
  $$taglib_BASE/flac/flacmetadatablock.cpp \
  $$taglib_BASE/flac/flacunknownmetadatablock.cpp

oggflacs_SOURCES = \
  $$taglib_BASE/ogg/flac/oggflacfile.cpp

mpc_SOURCES = \
  $$taglib_BASE/mpc/mpcfile.cpp \
  $$taglib_BASE/mpc/mpcproperties.cpp

mp4_SOURCES = \
  $$taglib_BASE/mp4/mp4file.cpp \
  $$taglib_BASE/mp4/mp4atom.cpp \
  $$taglib_BASE/mp4/mp4tag.cpp \
  $$taglib_BASE/mp4/mp4item.cpp \
  $$taglib_BASE/mp4/mp4properties.cpp \
  $$taglib_BASE/mp4/mp4coverart.cpp

ape_SOURCES = \
  $$taglib_BASE/ape/apetag.cpp \
  $$taglib_BASE/ape/apefooter.cpp \
  $$taglib_BASE/ape/apeitem.cpp \
  $$taglib_BASE/ape/apefile.cpp \
  $$taglib_BASE/ape/apeproperties.cpp

wavpack_SOURCES = \
  $$taglib_BASE/wavpack/wavpackfile.cpp \
  $$taglib_BASE/wavpack/wavpackproperties.cpp

speex_SOURCES = \
  $$taglib_BASE/ogg/speex/speexfile.cpp \
  $$taglib_BASE/ogg/speex/speexproperties.cpp

opus_SOURCES = \
  $$taglib_BASE/ogg/opus/opusfile.cpp \
  $$taglib_BASE/ogg/opus/opusproperties.cpp

trueaudio_SOURCES = \
  $$taglib_BASE/trueaudio/trueaudiofile.cpp \
  $$taglib_BASE/trueaudio/trueaudioproperties.cpp

asf_SOURCES = \
  $$taglib_BASE/asf/asftag.cpp \
  $$taglib_BASE/asf/asffile.cpp \
  $$taglib_BASE/asf/asfproperties.cpp \
  $$taglib_BASE/asf/asfattribute.cpp \
  $$taglib_BASE/asf/asfpicture.cpp

riff_SOURCES = \
  $$taglib_BASE/riff/rifffile.cpp

aiff_SOURCES = \
  $$taglib_BASE/riff/aiff/aifffile.cpp \
  $$taglib_BASE/riff/aiff/aiffproperties.cpp

wav_SOURCES = \
  $$taglib_BASE/riff/wav/wavfile.cpp \
  $$taglib_BASE/riff/wav/wavproperties.cpp \
  $$taglib_BASE/riff/wav/infotag.cpp

mod_SOURCES = \
  $$taglib_BASE/mod/modfilebase.cpp \
  $$taglib_BASE/mod/modfile.cpp \
  $$taglib_BASE/mod/modtag.cpp \
  $$taglib_BASE/mod/modproperties.cpp

s3m_SOURCES = \
  $$taglib_BASE/s3m/s3mfile.cpp \
  $$taglib_BASE/s3m/s3mproperties.cpp

it_SOURCES = \
  $$taglib_BASE/it/itfile.cpp \
  $$taglib_BASE/it/itproperties.cpp

xm_SOURCES = \
  $$taglib_BASE/xm/xmfile.cpp \
  $$taglib_BASE/xm/xmproperties.cpp

toolkit_SOURCES = \
  $$taglib_BASE/toolkit/tstring.cpp \
  $$taglib_BASE/toolkit/tstringlist.cpp \
  $$taglib_BASE/toolkit/tbytevector.cpp \
  $$taglib_BASE/toolkit/tbytevectorlist.cpp \
  $$taglib_BASE/toolkit/tbytevectorstream.cpp \
  $$taglib_BASE/toolkit/tiostream.cpp \
  $$taglib_BASE/toolkit/tfile.cpp \
  $$taglib_BASE/toolkit/tfilestream.cpp \
  $$taglib_BASE/toolkit/tdebug.cpp \
  $$taglib_BASE/toolkit/tpropertymap.cpp \
  $$taglib_BASE/toolkit/trefcounter.cpp \
  $$taglib_BASE/toolkit/tdebuglistener.cpp \
  $$taglib_BASE/toolkit/tzlib.cpp

unicode_SOURCES = \
  $$taglib_BASE/toolkit/unicode.cpp

HEADERS += $$taglib_HEADERS

SOURCES += \
  $$mpeg_SOURCES \
  $$id3v1_SOURCES \
  $$id3v2_SOURCES \
  $$frames_SOURCES \
  $$ogg_SOURCES \
  $$vorbis_SOURCES \
  $$flacs_SOURCES \
  $$oggflacs_SOURCES \
  $$mpc_SOURCES \
  $$mp4_SOURCES \
  $$ape_SOURCES \
  $$wavpack_SOURCES \
  $$speex_SOURCES \
  $$opus_SOURCES \
  $$trueaudio_SOURCES \
  $$asf_SOURCES \
  $$riff_SOURCES \
  $$aiff_SOURCES \
  $$wav_SOURCES \
  $$mod_SOURCES \
  $$s3m_SOURCES \
  $$it_SOURCES \
  $$xm_SOURCES \
  $$toolkit_SOURCES \
  $$unicode_SOURCES \
  $$taglib_BASE/tag.cpp \
  $$taglib_BASE/tagunion.cpp \
  $$taglib_BASE/fileref.cpp \
  $$taglib_BASE/audioproperties.cpp \
  $$taglib_BASE/tagutils.cpp

INCLUDEPATH += \
  $$taglib_BASE \
  $$taglib_BASE/toolkit \
  $$taglib_BASE/asf \
  $$taglib_BASE/mpeg \
  $$taglib_BASE/ogg \
  $$taglib_BASE/ogg/flac \
  $$taglib_BASE/flac \
  $$taglib_BASE/mpc \
  $$taglib_BASE/mp4 \
  $$taglib_BASE/ogg/vorbis \
  $$taglib_BASE/ogg/speex \
  $$taglib_BASE/ogg/opus \
  $$taglib_BASE/mpeg/id3v2 \
  $$taglib_BASE/mpeg/id3v2/frames \
  $$taglib_BASE/mpeg/id3v1 \
  $$taglib_BASE/ape \
  $$taglib_BASE/wavpack \
  $$taglib_BASE/trueaudio \
  $$taglib_BASE/riff \
  $$taglib_BASE/riff/aiff \
  $$taglib_BASE/riff/wav \
  $$taglib_BASE/mod \
  $$taglib_BASE/s3m \
  $$taglib_BASE/it \
  $$taglib_BASE/xm
