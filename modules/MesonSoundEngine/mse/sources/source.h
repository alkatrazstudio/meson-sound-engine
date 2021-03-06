/****************************************************************************}
{ source.h - single playlist entry                                           }
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

#pragma once

#include "mse/object.h"
#include "mse/utils/utils.h"

#include "types/source_tags.h"
#include "mse/utils/codepage_translator.h"

struct MSE_CueSheet;

/*!
 * Description of a single track in a CUE sheet.
 */
struct MSE_CueSheetTrack {
    int index; /*!< Zero-based track index. Actual track index (as written in a CUE sheet) minus 1. */
    double startPos; /*!< Starting position of a track in seconds. */
    double endPos; /*!< End position of a track in seconds. */
    QString title; /*!< Track title. Or sheet title if a track title is unavailable. */
    QString performer; /*!< Track performer. Or sheet performer if a track performer is unavailable. */
    MSE_CueSheet* sheet; /*!< Corresponding MSE_CueSheet object. */
};

/*!
 * List of MSE_CueSheetTrack objects.
 */
typedef QList<MSE_CueSheetTrack*> MSE_CueSheetTracks;

/*!
 * CUE sheet.
 */
struct MSE_CueSheet {
    QString cueFilename; /*!< The full file path to .cue file. */
    QString dataSourceFilename; /*!< The full file path to corresponding audio file. */
    MSE_CueSheetTracks tracks; /*!< List of tracks included. */
    MSE_SoundChannelType sourceType; /*!<
    Type of an audio channel for the audio file (MSE_CueSheet::sourceFilename).

    \sa MSE_SoundChannelType
*/
    bool isValid; /*!< Is this CUE sheet valid? */
    QString title; /*!< Global TITLE value. Usually, it's an album name. */
    QString date; /*!< Global DATE. Usually, it's in the comments. */
};

/*!
 * List of MSE_CueSheet objects.
 */
typedef QList<MSE_CueSheet*> MSE_CueSheets;


typedef QHash<QString, QString> MSE_SourceAssocTags;


/*!
 * URI, filename and metadata for one line in the playlist.
 * Note: uri points to a playlist entry and may not be an actual filename
 *       in case the entry is located within a file with multiple playlist entries (e.g. CUE-sheet).
 */
struct MSE_PlaylistEntry {
    QString uri;
    QString filename;
    QSharedPointer<MSE_SourceTags> tags;
    int cueIndex = -1;

    explicit MSE_PlaylistEntry()
    {
    }

    explicit MSE_PlaylistEntry(const QString& uri, QSharedPointer<MSE_SourceTags> tags = nullptr):
        uri(MSE_Utils::normalizeUri(uri)),
        tags(tags)
    {
        int p = uri.lastIndexOf(".cue:", -2, Qt::CaseInsensitive);
        if(p <= 0)
        {
            filename = uri;
        }
        else
        {
            QChar c;
            QString indexString;
            int n = uri.size();
            bool ok = true;
            for(int a=p+5; a<n; a++)
            {
                c = uri.at(a);
                if((c < '0') || (c > '9'))
                {
                    ok = false;
                    break;
                }
                indexString.append(c);
            }

            if(ok)
            {
                cueIndex = indexString.toInt();
                filename = uri.left(p+4);
            }
            else
            {
                filename = uri;
            }
        }

        QFileInfo info;
        info.setFile(filename);
        if(info.exists())
        {
            filename = info.absoluteFilePath();
            if(cueIndex >= 0)
                this->uri = filename + ":" + QString::number(cueIndex);
        }
    }
};


/*!
 * Description of a single line in a playlist.
 */
class MSE_Source : public MSE_Object
{
    Q_OBJECT

public:
    MSE_Source(MSE_Playlist *parent);

    /*!
     * Opens a stream/module/URL/whatever and returns HCHANNEL on success.
     * Returns 0 on failure.
     */
    virtual HCHANNEL open() = 0;

    /*!
     * Frees all resources.
     */
    virtual bool close() = 0;

    /*!
     * Clears MSE_SourceTags struct, fills it with tags info, then performs tags cleanup/normalization.
     * Returns true if valid tags were actually filled in.
     */
    bool fillTags(MSE_SourceTags& tags);

    bool parseTagsOGG(HCHANNEL channel, MSE_SourceTags &tags, DWORD tagsType = BASS_TAG_OGG);

    int index; /*!< Source index */
    MSE_PlaylistEntry entry; /*!< Info about playlist entry. */
    QByteArray filenameData; /*!< Character data for a filename */
    const MSE_CueSheetTrack* cueSheetTrack; /*!<
    If a source is a part of a CUE sheet, then this property holds the information about CUE sheet track.

    If a source is not a part of a CUE sheet, then this property is nullptr.
*/
    MSE_SoundChannelType type; /*!<
    Sound channel type.

    \sa MSE_SoundChannelType
*/

    const QString& getPlaylistUri() const;

protected:
    const char *getDataSourceUtfFilename();
    virtual bool getTags(MSE_SourceTags& tags);

    MSE_Sound* sound;
    MSE_CodepageTranslator cpTr;

    MSE_SourceAssocTags processChunkedData(const char *data);
    QString getTrReference();
private:
    const char* utfFilename;

signals:
    /*!
     * Emitted when a new meta data is available. For example ICY metadata has been updated.
     */
    void onMeta();
};

/*!
 * List of MSE_Source objects.
 */
typedef QList<MSE_Source*> MSE_Sources;
