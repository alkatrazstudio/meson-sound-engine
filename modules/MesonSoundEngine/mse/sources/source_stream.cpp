/****************************************************************************}
{ source_stream.cpp - stream file                                            }
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

#include "mse/sources/source_stream.h"
#include "mse/sound.h"

#include "qiodevicehelper.h"

void CALLBACK onMetaSync(HSYNC handle, DWORD channel, DWORD data, void *user)
{
    Q_UNUSED(handle);
    Q_UNUSED(channel);
    Q_UNUSED(data);
    emit static_cast<MSE_SourceStream*>(user)->onMeta();
}

MSE_SourceStream::MSE_SourceStream(MSE_Playlist *parent) : MSE_Source(parent)
  ,stream(0)
{
    type = mse_sctStream;
}

HCHANNEL MSE_SourceStream::open()
{
    stream = BASS_StreamCreateFile(
        false,
        getUtfFilename(),
        0, 0,
        sound->getDefaultStreamFlags()
    );

    if(stream)
        BASS_ChannelSetSync(stream, BASS_SYNC_OGG_CHANGE, 0, &onMetaSync, this);

    return stream;
}

bool MSE_SourceStream::close()
{
    BASS_StreamFree(stream);
    return true;
}

bool MSE_SourceStream::isLetterOrDigit(char c)
{
    return ((c>='A')&&(c<='Z')) || ((c>='0')&&(c<='9'));
}

QString MSE_SourceStream::decodeTagValue(const void* data, quint32 len)
{
    // detect UTF-16 BOM 0xFEFF or 0xFFEF
    if(
        (len > 1)
        &&
        (
            (
                (static_cast<const unsigned char*>(data)[0] == 0xFF)
                &&
                (static_cast<const unsigned char*>(data)[1] == 0xFE)
            )
            ||
            (
                (static_cast<const unsigned char*>(data)[0] == 0xFE)
                &&
                (static_cast<const unsigned char*>(data)[1] == 0xFF)
            )
        )
    )
    {
        return QString::fromUtf16(static_cast<const ushort*>(data), len/2);
    }

    if(QIODeviceEx::isNotUtf8(static_cast<const char*>(data), len))
        return QString::fromLocal8Bit(static_cast<const char*>(data), len).trimmed();
    else
        return QString::fromUtf8(static_cast<const char*>(data), len).trimmed();
}

bool MSE_SourceStream::parseTagsID3v2(MSE_SourceTags &tags)
{
    const char* tagStart = BASS_ChannelGetTags(stream, BASS_TAG_ID3V2);
    if(!tagStart)
        return false;
    const MSE_TagInfoID3v2Header* tagh = reinterpret_cast<const MSE_TagInfoID3v2Header*>(tagStart);

    const char* tagp = tagStart + sizeof(MSE_TagInfoID3v2Header);
    const char* tagpMax = tagp + tagh->byteSize();

    quint32 tagLen;
    QString tagName;
    QString tpeValue;
    const char* tagValue;
    if(tagh->version == 2)
    {
        // ID3 v2.2
        const MSE_TagInfoID3v22* tag22;
        forever{
            if(tagp >= tagpMax)
                break;
            tag22 = reinterpret_cast<const MSE_TagInfoID3v22*>(tagp);
            if(
                !isLetterOrDigit(tag22->name[0]) ||
                !isLetterOrDigit(tag22->name[1]) ||
                !isLetterOrDigit(tag22->name[2])
            )
            {
                // invalid tag name
                break;
            }
            tagLen = tag22->byteSize() - 1;
            tagName = QString::fromLocal8Bit(&tag22->name[0], sizeof(tag22->name));
            tagp += sizeof(MSE_TagInfoID3v22);
            tagValue = tagp;
            if(tagName == "TT2")
                tags.trackTitle = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TP1")
                tags.trackArtist = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TP2")
                tpeValue = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TAL")
                tags.trackAlbum = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TYE")
                tags.trackDate = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TRK")
                tags.trackIndex = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TPA")
                tags.discIndex = decodeTagValue(tagValue, tagLen);
            if(!tags.trackTitle.isEmpty()
                && !tags.trackArtist.isEmpty()
                && !tags.trackAlbum.isEmpty()
                && !tags.trackDate.isEmpty()
                && !tags.trackIndex.isEmpty()
                && !tags.discIndex.isEmpty()
            ){
                break;
            }
            tagp += tagLen;
        }
    }
    else
    {
        // ID3 v2.3, v2.4
        const MSE_TagInfoID3v2* tag2;
        forever{
            if(tagp >= tagpMax)
                break;
            tag2 = reinterpret_cast<const MSE_TagInfoID3v2*>(tagp);
            if(!(
                isLetterOrDigit(tag2->name[0])&&
                isLetterOrDigit(tag2->name[1])&&
                isLetterOrDigit(tag2->name[2])&&
                isLetterOrDigit(tag2->name[3]))
            )
            {
                break;
            }
            tagLen = tag2->byteSize() - 1;
            tagName = QString::fromLocal8Bit(&tag2->name[0], sizeof(tag2->name));
            tagp += sizeof(MSE_TagInfoID3v2);
            tagValue = tagp;
            if(tagName == "TIT2")
                tags.trackTitle = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TPE1")
                tags.trackArtist = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TPE2")
                tpeValue = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TALB")
                tags.trackAlbum = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TYER")
                tags.trackDate = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TRCK")
                tags.trackIndex = decodeTagValue(tagValue, tagLen);
            else
            if(tagName == "TPOS")
                tags.discIndex = decodeTagValue(tagValue, tagLen);
            if(!tags.trackTitle.isEmpty()
                && !tags.trackArtist.isEmpty()
                && !tags.trackAlbum.isEmpty()
                && !tags.trackDate.isEmpty()
                && !tags.trackIndex.isEmpty()
                && !tags.discIndex.isEmpty()
            ){
                break;
            }
            tagp += tagLen;
        }
    }
    if(tags.trackArtist.isEmpty())
        tags.trackArtist = tpeValue;

    int p = tags.trackIndex.indexOf('/');
    if(p >= 0)
    {
        tags.nTracks = tags.trackIndex.mid(p+1);
        tags.trackIndex = tags.trackIndex.mid(0, p);
    }

    p = tags.discIndex.indexOf('/');
    if(p >= 0)
    {
        tags.nDiscs = tags.discIndex.mid(p+1);
        tags.discIndex = tags.discIndex.mid(0, p);
    }

    return true;
}

bool MSE_SourceStream::parseTagsID3(MSE_SourceTags& tags)
{
    const TAG_ID3* tagsData = reinterpret_cast<const TAG_ID3*>(BASS_ChannelGetTags(stream, BASS_TAG_ID3));
    if(!tagsData)
        return false;

    tags.trackArtist = QString::fromUtf8(&tagsData->artist[0], 30);
    tags.trackTitle = QString::fromUtf8(&tagsData->title[0], 30);
    tags.trackAlbum = QString::fromUtf8(&tagsData->album[0], 30);
    tags.trackDate = QString::fromUtf8(&tagsData->year[0], 4);

    return true;
}

bool MSE_SourceStream::getTags(MSE_SourceTags& tags)
{
    if(MSE_Source::getTags(tags))
        return true;

    if(!parseTagsID3v2(tags))
        if(!parseTagsID3(tags))
            return parseTagsOGG(stream, tags, BASS_TAG_OGG);

    return true;
}
