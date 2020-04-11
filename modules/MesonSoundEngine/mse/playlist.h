/****************************************************************************}
{ playlist.h - playlist management                                           }
{                                                                            }
{ Copyright (c) 2012 Alexey Parfenov <zxed@alkatrazstudio.net>               }
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

#include "mse/types.h"
#include "mse/sound.h"
#include "mse/sources/source.h"

/*!
 * MSE_Playlist manages lists of music files.
 * It can load/save a playlist/files from/to a file/URL, add files from directories recursively, shuffle playlist.
 *
 * Normally you don't need to create MSE_Playlist object.
 * Instead, use MSE_Sound::getPlaylist() to fetch a reference to a playlist for that sound object.
 */
class MSE_Playlist : public MSE_Object
{
    Q_OBJECT

public:
    MSE_Playlist(MSE_Sound* parent = nullptr);
    ~MSE_Playlist();

    /*!
     * Returns a list of sound sources.
     *
     * \sa MSE_SoundSources
     */
    inline const MSE_Sources* getList() const {return &playlist;}

    /*!
     * Returns a parent sound object.
     *
     * \sa MSE_Playlist()
     */
    inline MSE_Sound* getSound() const {return sound;}

    void getNextSources(QList<const MSE_Source*> &nextList, int count = 30);

    /*!
     * Returns queued sound sources.
     */
    inline const MSE_Sources* getQueue() const {return &queue;}

    /*!
     * Clears a queue.
     */
    inline void clearQueue(){queue.clear();}

    bool appendToQueue(int index);
    bool insertIntoQueue(int index, int pos = 0);
    bool removeFromQueue(int index);

    /*!
     * Removes a first entry of a specified source from a queue if any.
     */
    inline bool removeSourceFromQueue(MSE_Source* source){return queue.removeOne(source);}

    /*!
     * Removes all entries of a specified source from a queue.
     */
    inline bool removeAllSourcesFromQueue(MSE_Source* source){return queue.removeAll(source);}

    bool removeSourceFromQueue(int sourceIndex);
    bool removeAllSourcesFromQueue(int sourceIndex);

    static MSE_PlaylistFormatType typeByHeader(QIODevice* dev);
    static bool skipBOM(QIODevice* dev);
    static bool hasSupportedExtension(const QString &filename);
    static bool hasSupportedExtension(const QString &filename, bool &isCue);
    static MSE_PlaylistFormatType typeByName(const QString& name);
    static QString extByType(MSE_PlaylistFormatType playlistType);
    static MSE_PlaylistPlaybackMode playbackModeFromString(const QString& str, bool* ok = nullptr);
    static QString playbackModeToString(MSE_PlaylistPlaybackMode mode);

    bool setFile(const QString& filename);
    bool addFile(const QString& filename);
    bool addUrl(const QString& url);
    bool addUrl(const QString& url, MSE_Sources& sourcesList);
    int addFromDirectory(const QString& dirname, MSE_SourceLoadFlags sourceLoadFlags = mse_slfDefault);
    int addFromDirectory(const QStringList& dirnames, MSE_SourceLoadFlags sourceLoadFlags = mse_slfDefault);
    int addFromPlaylist(const QString& filename, MSE_SourceLoadFlags sourceLoadFlags = mse_slfDefault);
    int addFromPlaylist(const QStringList& filenames, MSE_SourceLoadFlags sourceLoadFlags = mse_slfDefault);
    int addAnything(const QString& source, MSE_SourceLoadFlags sourceLoadFlags = mse_slfDefault);
    int addAnything(const QStringList& sources, MSE_SourceLoadFlags sourceLoadFlags = mse_slfDefault);
    void clear();

    static bool write(QIODevice *dev, const QStringList &playlist, MSE_PlaylistFormatType playlistType = mse_pftM3U);
    bool write(QIODevice *dev, MSE_PlaylistFormatType playlistType = mse_pftM3U) const;
    static bool write(const QString& filename, const QStringList &playlist, MSE_PlaylistFormatType playlistType = mse_pftM3U);
    bool write(const QString& filename, MSE_PlaylistFormatType playlistType = mse_pftM3U) const;
    static bool parse(QIODevice* dev, QStringList &playlist);
    static bool parse(const QString& filename, QStringList& playlist);

    MSE_Source* filenameToSource(const QString& filename);
    MSE_Source* createSourceFromType(MSE_SoundChannelType type);
    MSE_CueSheet* getCueSheet(const QString& filename);

    int indexOfFullName(const QString& fullSourceName);

    void shuffle();

    /*!
     * Returns the current playback mode.
     *
     * \sa MSE_PlaylistPlaybackMode
     */
    inline MSE_PlaylistPlaybackMode getPlaybackMode() const {return playbackMode;}

    void setPlaybackMode(MSE_PlaylistPlaybackMode mode);

    /*!
     * Returns the current position in the playback history.
     *
     * \sa getHistory
     */
    inline int getHistoryIndex() const {return historyIndex;}

    /*!
     * Returns the playback history.
     * Only valid if a current playback mode is mse_ppmRandom.
     *
     * \sa getHistoryPos, getPlaybackMode
     */
    inline const MSE_Sources* getHistory() const {return history;}

    /*!
     * Returns a current track's index in the playlist.
     * Returns -1 if there is no valid sound source selected.
     */
    inline int getIndex() const {return index;}

    /*!
     * Returns a current sound source.
     * Returns nullptr if there is no valid sound source selected.
     */
    inline MSE_Source* getCurrentSource() const {return currentSource;}

    void regenerateHistory();
    int getNextIndex();
    int getPrevIndex();
    bool tryMoveToNext();
    bool tryMoveToPrev();
    bool moveToNext();
    bool moveToPrev();
    bool setIndex(int newIndex);
    MSE_Source *getNextSource();
    MSE_Source* getPrevSource();
    bool isFirstInDir();
    bool isLastInDir();
    bool isAtStart();
    bool isAtEnd();
    bool moveToFirstInDir();
    bool moveToFirstInPrevDir();
    bool moveToFirstInNextDir();

    static const int detectLength;

protected:
    MSE_Engine* engine;  /*!< Main MSE_Engine object. */
    MSE_PlaylistPlaybackMode playbackMode; /*!< Playback mode. */
    int index; /*!< A current source's position in playlist. -1 if there's no current source loaded. */
    MSE_Source* currentSource; /*!< A current sound source. */
    MSE_Sources* history; /*!< Playback history. Only valid if a current playback mode is mse_ppmRandom. */
    int historyIndex; /*!< A position of a current sound source in a playback history. */

    MSE_Sources playlist; /*!< List of sound sources. */
    MSE_Sources queue; /*!< A queue of sound sources to be played */

    MSE_Sound* sound; /*!< A parent MSE_Sound object passed into a constructor. */
    MSE_CueSheets cueSheetsCache; /*!< An in-memory cache of CUE sheets. */

    void generateShuffle(MSE_Sources* sources);
    void appendHistoryShuffle();
    void prependHistoryShuffle();
    void updateHistoryIndex();
    void addToPlaylistRaw(MSE_Source* src);
    bool setSourceDataForCueSheet(MSE_CueSheet* cueSheet);

    static bool writeASX(QIODevice* dev, const QStringList& realPlaylist);
    static bool writeM3U(QIODevice* dev, const QStringList& realPlaylist);
    static bool writeXSPF(QIODevice* dev, const QStringList& realPlaylist);
    static bool writePLS(QIODevice* dev, const QStringList& realPlaylist);
    static bool writeWPL(QIODevice *dev, const QStringList& realPlaylist);

    static bool parseASX(QIODevice* dev, QStringList& realPlaylist);
    static bool parseM3U(QIODevice* dev, QStringList& realPlaylist);
    static bool parseXSPF(QIODevice* dev, QStringList& realPlaylist);
    static bool parsePLS(QIODevice* dev, QStringList& realPlaylist);
    static bool parseWPL(QIODevice *dev, QStringList& realPlaylist);

signals:
    void onPlaybackModeChange();
};
