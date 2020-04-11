/****************************************************************************}
{ source.cpp - single playlist entry                                         }
{                                                                            }
{ Copyright (c) 2013 Alexey Parfenov <zxed@alkatrazstudio.net>               }
{                                                                            }
{ This file is a part of Meson Sound Engine.                                 }
{                                                                            }
{ Meson Sound Engine is free software: you can redistribute it and/or        }
{ modify it under the terms of the GNU General Public License as published   }
{ by the Free Software Foundation, either version 3 of the License,          }
{ or (at your option) any later version.                                     }
{                                                                            }
{ Meson Sound Engine is distributed in the hope that it will be useful,      }
{ but WITHOUT ANY WARRANTY; without even the implied warranty of             }
{ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU           }
{ General Public License for more details: https://gnu.org/licenses/gpl.html }
{****************************************************************************/

#include "mse/sources/source.h"
#include "mse/playlist.h"

MSE_Source::MSE_Source(MSE_Playlist *parent):MSE_Object(parent)
  ,utfFilename(nullptr)
{
    sound = parent->getSound();
}

bool MSE_Source::fillTags(MSE_SourceTags &tags)
{
    tags.clear();
    bool result = getTags(tags);
    tags.clean();
    return result;
}

/*!
 * If the track is a part of CUE-sheet, then this function returns artist/title information from CUE.
 * Otherwise it depends on implementaion in child classes.
 */
bool MSE_Source::getTags(MSE_SourceTags &tags)
{
    if(cueSheetTrack)
    {
        tags.trackTitle = cueSheetTrack->title;
        tags.trackArtist = cueSheetTrack->performer;
        tags.trackAlbum = cueSheetTrack->sheet->title;
        tags.trackDate = cueSheetTrack->sheet->date;
        tags.nTracks = QString::number(cueSheetTrack->sheet->tracks.size());
        tags.trackIndex = QString::number(cueSheetTrack->index+1);
        return true;
    }
    return false;
}


/*!
 * Processes OGG-like data in "key=value\0" format.
 */
MSE_SourceAssocTags MSE_Source::processChunkedData(const char* data)
{
    MSE_SourceAssocTags result;
    if(!data)
        return result;

    quint64 p = 0;
    quint64 chunkBlockStart;
    QString chunkKey;
    QString tagPair;
    int i;

    while(data[p])
    {
        chunkBlockStart = p;
        while(data[p])
            p++;
        tagPair = QString::fromUtf8(&data[chunkBlockStart], p - chunkBlockStart);
        p++;
        i = tagPair.indexOf('=');
        if(i < 0)
            continue;
        chunkKey = tagPair.mid(0, i).trimmed().toUpper();
        result[chunkKey] = tagPair.mid(i+1);
    }
    return result;
}

/*!
 * Parses OGG-like tags in various formats.
 * *tagsType* must be one of BASS_TAG_*
 */
bool MSE_Source::parseTagsOGG(HCHANNEL channel, MSE_SourceTags &tags, DWORD tagsType)
{
    const char* tagsData = BASS_ChannelGetTags(channel, tagsType);
    if(!tagsData)
        return false;
    MSE_SourceAssocTags theTags = processChunkedData(tagsData);
    if(theTags.isEmpty())
        return false;

    tags.trackArtist = theTags["ALBUMARTIST"];
    if(tags.trackArtist.isEmpty())
    {
        tags.trackArtist = theTags["ARTIST"];
        if(tags.trackArtist.isEmpty())
            tags.trackArtist = theTags["AUTHOR"];
    }
    tags.trackTitle = theTags["TITLE"];
    if(tags.trackArtist.isEmpty() && tags.trackTitle.isEmpty())
        return false;

    tags.trackAlbum = theTags["ALBUM"];
    tags.trackDate = theTags["DATE"];
    tags.genre = theTags["GENRE"];

    tags.trackIndex = theTags["TRACKNUMBER"];
    if(tags.trackIndex.isEmpty())
    {
        tags.trackIndex = theTags["TRACK"];
        int p = tags.trackIndex.indexOf('/');
        if(p >= 0)
        {
            tags.nTracks = tags.trackIndex.mid(p+1);
            tags.trackIndex = tags.trackIndex.mid(0, p);
        }
    }
    if(tags.nTracks.isEmpty())
        tags.nTracks = theTags["TRACKTOTAL"];
    if(tags.nTracks.isEmpty())
        tags.nTracks = theTags["TOTALTRACKS"];

    tags.discIndex = theTags["DISCNUMBER"];
    if(tags.discIndex.isEmpty())
        tags.discIndex = theTags["DISC"];
    int p = tags.discIndex.indexOf('/');
    if(p >= 0)
    {
        tags.nDiscs = tags.discIndex.mid(p+1);
        tags.discIndex = tags.discIndex.mid(0, p);
    }
    if(tags.nDiscs.isEmpty())
        tags.nDiscs = theTags["DISCTOTAL"];
    if(tags.nDiscs.isEmpty())
        tags.nDiscs = theTags["TOTALDISCS"];

    return true;
}

/*!
 * If a source is a part of a CUE sheet,
 * then this function returns the full file path to .cue file
 * followed by a colon and a track index (MSE_CueSheetTrack::index).
 *
 * example:
 *
 *     D:\music\The Best Ballads.cue:2
 *
 * If a source is not a part of a CUE sheet, then this property returns MSE_SoundSource::filename.
 *
 * example:
 *
 *     D:\music\The Best Ballads\3.flac
 */
QString MSE_Source::getFullFilename() const
{
    if(cueSheetTrack)
    {
        return cueSheetTrack->sheet->cueFilename+
                ":"+
                QString::number(cueSheetTrack->index);
    }
    else
    {
        return filename;
    }
}

const char *MSE_Source::getUtfFilename()
{
    if(!utfFilename)
    {
#ifdef Q_OS_WIN
        utfFilename = (char*)(filename.utf16());
#else
        filenameData = filename.toUtf8();
        utfFilename = filenameData.constData();
#endif
    }
    return utfFilename;
}
