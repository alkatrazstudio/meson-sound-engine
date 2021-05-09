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

#include "qiodevicehelper.h"

#include <QSharedPointer>

#ifdef MSE_ICU
    #include "unicode/ucsdet.h"
    #include "unicode/ucnv.h"
#endif

MSE_Source::MSE_Source(MSE_Playlist *parent):MSE_Object(parent)
  ,sound(parent->getSound())
  ,cpTr(sound->getInitParams().useICU, sound->getInitParams().icuMinConfidence)
  ,utfFilename(nullptr)
{
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
    int i;

    while(data[p])
    {
        chunkBlockStart = p;
        while(data[p])
            p++;
        cpTr.addEntry(&data[chunkBlockStart], p - chunkBlockStart, [&](const QString& s){
            i = s.indexOf('=');
            if(i < 0)
                return;
            chunkKey = s.mid(0, i).trimmed().toUpper();
            result[chunkKey] = s.mid(i+1);
        });
        p++;
    }

    cpTr.processEntries(getTrReference());
    return result;
}

QString MSE_Source::getTrReference()
{
    if(type == mse_sctRemote)
        return QString();

    QFileInfo f(filename);
    return f.completeBaseName() + f.dir().dirName();
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


MSE_CodepageTranslator::MSE_CodepageTranslator(bool useICU, int minConfidence)
    :useICU(useICU)
    ,minConfidence(minConfidence)
{
}

void MSE_CodepageTranslator::addEntry(const char* strData, int dataLen, const Callback &callback)
{
    Entry entry {
        .callback = callback,
        .strData = QByteArray(strData, dataLen),
        .result = QString()
    };
    entries.push_back(entry);
}

void MSE_CodepageTranslator::processEntries(const QString& reference)
{
    QByteArray allText;
    QMutableListIterator<Entry> i(entries);
    bool needICU = false;

    int maxStrSize = 0;
    for(const Entry& entry : qAsConst(entries))
        maxStrSize += entry.strData.size();
    allText.reserve(maxStrSize);

    while(i.hasNext())
    {
        auto& entry = i.next();
        if(entry.strData.isEmpty())
        {
            entry.needICU = false;
            continue;
        }

        if(translateWithoutICU(entry.strData, entry.result))
        {
            entry.needICU = false;
            continue;
        }

        allText.append(entry.strData);
        needICU = true;
    }

#ifdef MSE_ICU
    if(needICU)
    {
        QList<ConvEntry> convs;
        if(!detectCodepage(allText, convs))
        {
            convertAllToLatin();
            return;
        }

        auto err = U_ZERO_ERROR;
        auto *utfCnv = ucnv_open("UTF8", &err);
        if(U_FAILURE(err))
        {
            convertAllToLatin();
            return;
        }

        QHash<int, QString> bestTr;
        int bestConfidence = 0;
        int bestRefScore = 0;

        QString validRef;
        validRef.reserve(reference.size());

        for(const QChar& c : qAsConst(reference))
        {
            if(c.unicode() < 128 || c.isSpace() || c.isDigit() || c.isPunct())
                continue;
            validRef.append(c);
        }

        for(const auto& conv : qAsConst(convs))
        {
            QMutableListIterator<Entry> i(entries);
            int entryIndex = -1;
            QHash<int, QString> trEntry;
            int refScore = 0;
            int charsCounted = 0;
            bool ok = true;
            while(i.hasNext())
            {
                entryIndex++;
                auto& entry = i.next();
                if(!entry.needICU)
                    continue;
                QString result;
                if(translateWithICU(conv.converter, utfCnv, entry.strData, result))
                {
                    if(!validRef.isEmpty())
                    {
                        for(const QChar& c : qAsConst(result))
                        {
                            if(c.unicode() < 128 || c.isSpace() || c.isDigit() || c.isPunct())
                                continue;
                            charsCounted++;
                            if(validRef.contains(c.toUpper()) || reference.contains(c.toLower()))
                                refScore++;
                        }
                    }
                    trEntry[entryIndex] = result;
                }
                else
                {
                    ok = false;
                    break;
                }
            }

            if(!ok)
                continue;

            if(validRef.isEmpty())
            {
                if(conv.confidence >= minConfidence)
                {
                    bestTr = trEntry;
                    bestConfidence = conv.confidence;
                }
                break;
            }

            if(refScore > bestRefScore)
            {
                bestTr = trEntry;
                bestConfidence = conv.confidence;
                bestRefScore = refScore;
                continue;
            }

            if(refScore == bestRefScore && conv.confidence > bestConfidence)
            {
                bestTr = trEntry;
                bestConfidence = conv.confidence;
            }
        }

        ucnv_close(utfCnv);

        if(bestRefScore > 0 || bestConfidence > 0)
        {
            for(auto i = bestTr.constKeyValueBegin(); i != bestTr.constKeyValueEnd(); i++)
            {
                auto entryIndex = (*i).first;
                entries[entryIndex].result = (*i).second;
            }
        }
        else
        {
            convertAllToLatin();
            return;
        }
    }
#endif

    for(const Entry& entry : qAsConst(entries))
    {
        const QString result = entry.result.trimmed();
        entry.callback(result);
    }

    clearEntries();
}

bool MSE_CodepageTranslator::detectCodepage(
    const QByteArray& text,
    QList<ConvEntry>& cnvPtrs
)
{
#ifndef MSE_ICU
    Q_UNUSED(text)
    Q_UNUSED(cnvPtrs)
    return false;
#else
    if(text.isEmpty())
        return false;

    auto err = U_ZERO_ERROR;
    auto *det = ucsdet_open(&err);
    auto detPtr = QSharedPointer<UCharsetDetector>(det, [](UCharsetDetector* det){
        ucsdet_close(det);
    });
    if(U_FAILURE(err))
        return false;

    err = U_ZERO_ERROR;
    ucsdet_setText(det, text.constData(), text.size(), &err);
    if(U_FAILURE(err))
        return false;

    err = U_ZERO_ERROR;
    auto const *match = ucsdet_detect(det, &err);
    if(U_FAILURE(err) || match == nullptr)
        return false;

    auto nMatches = 0;
    auto const *matches = ucsdet_detectAll(det, &nMatches, &err);
    if(U_FAILURE(err) || matches == nullptr)
        return false;

    for(int i = 0; i < nMatches; i++)
    {
        err = U_ZERO_ERROR;
        auto confidence = ucsdet_getConfidence(matches[i], &err);
        if(U_FAILURE(err))
            continue;

        if(confidence < minConfidence)
            break; // converters are sorted by confidence

        err = U_ZERO_ERROR;
        auto *detName = ucsdet_getName(matches[i], &err);
        if(U_FAILURE(err))
            continue;

        err = U_ZERO_ERROR;
        auto *cnv = ucnv_open(detName, &err);
        if(U_FAILURE(err))
            continue;

        auto cnvPtr = QSharedPointer<UConverter>(cnv, [](UConverter* cnv){
            ucnv_close(cnv);
        });

        ConvEntry conv {
            .converter = cnvPtr,
            .confidence = confidence
        };
        cnvPtrs.append(conv);
    }

    if(cnvPtrs.isEmpty())
        return false;

    return true;
#endif
}

bool MSE_CodepageTranslator::translateWithICU(
    QSharedPointer<UConverter> cnvPtr,
    UConverter* utfCnv,
    const QByteArray& strData,
    QString& result
)
{
#ifndef MSE_ICU
    return false;
#else
    if(strData.isEmpty())
        return false;

    auto utfCharSize = ucnv_getMaxCharSize(utfCnv);
    auto nTargetBytes = UCNV_GET_MAX_BYTES_FOR_STRING(strData.size(), utfCharSize);
    char utfBytes[nTargetBytes + 1];
    char* utfPtr = utfBytes;

    const char* sourceBytes = strData.constData();
    const char* endSourceBytePtr = strData.constEnd();

    auto err = U_ZERO_ERROR;
    ucnv_convertEx(
        utfCnv,
        cnvPtr.data(),
        &utfPtr,
        &utfBytes[nTargetBytes + 1],
        &sourceBytes,
        endSourceBytePtr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        true,
        true,
        &err
    );
    if(U_FAILURE(err))
        return false;

    auto nResultBytes = utfPtr - utfBytes;
    result = QString::fromUtf8(utfBytes, nResultBytes);
    return true;
#endif
}

void MSE_CodepageTranslator::convertAllToLatin()
{
    for(const Entry& entry : qAsConst(entries))
    {
        auto s = entry.result.isEmpty() ? QString::fromLatin1(entry.strData) : entry.result;
        entry.callback(s);
    }
    clearEntries();
}

void MSE_CodepageTranslator::clearEntries()
{
    entries.clear();
}

bool MSE_CodepageTranslator::translateWithoutICU(const QByteArray& strData, QString& result)
{
    // detect UTF-16 BOM 0xFEFF or 0xFFEF
    const void* rawData = strData.constData();
    if(
        (strData.size() > 1)
        &&
        (
            (
                (static_cast<const unsigned char*>(rawData)[0] == 0xFF)
                &&
                (static_cast<const unsigned char*>(rawData)[1] == 0xFE)
            )
            ||
            (
                (static_cast<const unsigned char*>(rawData)[0] == 0xFE)
                &&
                (static_cast<const unsigned char*>(rawData)[1] == 0xFF)
            )
        )
    )
    {
        result = QString::fromUtf16(static_cast<const ushort*>(rawData), strData.size()/2);
        return true;
    }

    if(QIODeviceEx::isNotUtf8(static_cast<const char*>(rawData), strData.size()))
        return false;

    result = QString::fromUtf8(strData);
    return true;
}
