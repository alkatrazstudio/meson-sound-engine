/****************************************************************************}
{ sound.h - playback & decoding                                              }
{                                                                            }
{ Copyright (c) 2011 Alexey Parfenov <zxed@alkatrazstudio.net>               }
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
#include "mse/engine.h"
#include "mse/playlist.h"
#include "mse/sources/source.h"

#ifdef QT_NETWORK_LIB
    #include <QtNetwork/QNetworkAccessManager>
    #include <QtNetwork/QNetworkReply>
#endif

class MSE_SoundPositionCallback;
typedef void (*MSE_SoundPositionCallbackFunc)(MSE_SoundPositionCallback*);

/*!
 * Holds information about a callback.
 *
 * Create this objects with MSE_Sound::installPositionCallback() function only.
 * Delete the object to remove the callback.
 */
class MSE_SoundPositionCallback : public QObject {
    Q_OBJECT
    friend class MSE_Sound;
public:
    inline MSE_Sound* getSound() const {return sound;}
    inline double getPos() const {return pos;}
    inline void invoke(){func(this);}
    inline void* getData() const {return data;}
    MSE_SoundPositionCallback() = delete;

protected:
    inline MSE_SoundPositionCallback(
            QObject* callbacks, double pos, MSE_SoundPositionCallbackFunc func, void* data)
        :QObject(callbacks)
        ,sound(qobject_cast<MSE_Sound*>(callbacks->parent())),pos(pos),func(func),data(data),sync(0),channel(0){
    }

    MSE_Sound* sound;
    double pos;
    MSE_SoundPositionCallbackFunc func;
    void* data;
    HSYNC sync;
    HCHANNEL channel;
};


/*!
 * This class represents a single audio input/output.
 */
class MSE_Sound : public MSE_Object
{
    Q_OBJECT

public:
    explicit MSE_Sound();
    ~MSE_Sound();

    bool init(const MSE_SoundInitParams &params = MSE_SoundInitParams());

    /*!
     * Returns parameters the sound object was initialized with.
     *
     * \note These parameters can be slightly different to those passed to init() function.
     */
    inline const MSE_SoundInitParams& getInitParams() const {return initParams;}

    /*!
     * Returns true if a sound source is loaded into a memory.
     */
    inline bool isOpen() {return handle != 0;}

    /*!
     * Returns MSE_Engine instance.
     */
    inline MSE_Engine* getEngine() const {return engine;}

    /*!
     * Returns a playlist object.
     */
    inline MSE_Playlist* getPlaylist() const {return playlist;}

    /*!
     * Returns a channel type of a current sound source.
     */
    inline MSE_SoundChannelType getType() const {return channelType;}

    /*!
     * Returns a playback state of a current sound source.
     */
    inline MSE_SoundChannelState getState() const {return channelState;}

    /*!
     * Returns a playback continuous state of a current sound source.
     *
     * \sa onContinuousStateChange
     */
    inline MSE_SoundChannelState getContinuousState() const {return channelContState;}

    /*!
     * Returns a sample rate conversion index
     */
    inline int getSampleRateConversion() const {return sampleRateConversion;}

    bool startRecord();
    bool openFromFile(const MSE_PlaylistEntry &entry);
    bool openFromList(int index = 0);
    bool playFromList(int index = 0);
    bool openNextValid();
    bool openPrevValid();
    bool openFirstValidInPrevDir();
    bool openFirstValidInNextDir();
    bool openFirstValidInDir();
    bool openNext();
    bool openPrev();
    bool openValid(int index = 0);
    bool playValid(int index = 0);
    bool playNextValid();
    bool playPrevValid();
    bool playFirstValidInNextDir();
    bool playFirstValidInPrevDir();
    bool playFirstValidInDir();
    bool playNext();
    bool playPrev();
    bool open();
    bool stop();
    bool pause();
    bool unpause();
    bool playOrUnpause();
    bool restart();
    bool close();
    bool play();
    int getData(void *buffer, int length);

    /*!
     * Returns a current Performer info for a current sound source.
     * Returns an empty string if such an information is unavailable at the moment.
     */
    inline const QString& getTrackArtist() const {return sourceTags.trackArtist;}

    /*!
     * Returns a Title for a current sound source.
     * Returns a filename without an extension or full URL
     * if an information about track title is unavailable at the moment.
     */
    inline const QString& getTrackTitle() const {return sourceTags.trackTitle;}

    /*!
     * Returns all supported tags for a current sound source.
     */
    inline const MSE_SourceTags& getTags() const {return sourceTags;}

    /*!
     * Returns true if artist info was retreived from tags.
     */
    inline bool isTrackArtistFromTags() const {return trackArtistFromTags;}

    /*!
     * Returns true if title info was retreived from tags.
     */
    inline bool isTrackTitleFromTags() const {return trackTitleFromTags;}

    /*!
     * Returns a string in format *%artist% - %title%* if an artist information is available.
     * Otherwise returns the same value as getTrackTitle() does.
     */
    inline QString getTrackFormattedTitle() const {return trackFormattedTitle;}

    /*!
     * Returns a full absolute path to a current sound source.
     */
    inline QString getTrackFilename() const {return trackFilename;}

    /*!
     * Returns a duration of a current sound source in seconds
     * or zero if such an information is unavailable.
     *
     * getFullTrackDuration() returns the same value as getTrackDuration()
     * unless the current file is split via CUE sheet.
     * If a sound source is a part of a CUE-splitted file, then
     * getTrackDuration() returns a length of a sound source,
     * not a length of a whole file.
     *
     * \sa getFullTrackDuration
     */
    inline double getTrackDuration() const {return trackDuration;}

    /*!
     * Returns a duration of a current sound file in seconds
     * or zero if such an information is unavailable.
     *
     * getFullTrackDuration() returns the same value as getTrackDuration()
     * unless the current file is split via CUE sheet.
     * If a sound source is a part of a CUE-splitted file, then
     * getTrackDuration() returns a length of a sound source,
     * not a length of a whole file.
     *
     * \sa getFullTrackDuration
     */
    inline double getFullTrackDuration() const {return fullTrackDuration;}

    /*!
     * Returns a current volume for a sound object in range [0;1].
     *
     * \note If a sound level was changed somewhere outside MSE,
     * then this function still return the same value.
     * Use refreshVolume() to update volume information.
     */
    inline float getVolume() const {return volume;}

    double getRealPosition();
    double getPosition();
    bool setPosition(double secs);

    bool setVolume(float value);
    void refreshVolume();
    bool changeVolume(float diff, bool snapToGrid = false);
    static MSE_SoundSampleType sampleTypeFromString(const QString& str, bool* ok = nullptr);
    static MSE_SoundSampleInterpolation sampleInterpolationFromString(const QString& str, bool* ok = nullptr);
    static MSE_SoundSampleRamping sampleRampingFromString(const QString& str, bool* ok = nullptr);
    static MSE_SoundSurroundMode surroundModeFromString(const QString& str, bool* ok = nullptr);
    static MSE_SoundTrackerEmulation trackerEmulationFromString(const QString& str, bool* ok = nullptr);
    static int sampleRateConversionFromSincPoints(int count);
    static int sincPointsFromSampleRateConversion(int value);
    static MSE_SoundChannelState channelStateFromString(const QString& str, bool* ok = nullptr);
    static QString channelStateToString(MSE_SoundChannelState state);

    QString getDirName(const QString &source, bool base = false) const;
    int getFrequency();
    int getChannelsCount();
    MSE_SoundPositionCallback* installPositionCallback(int pos, MSE_SoundPositionCallbackFunc func, void* data = nullptr);

    /*!
     * Get default flags for BASS_StreamCreateFile.
     */
    inline DWORD getDefaultStreamFlags() const {return defaultStreamFlags;}

    /*!
     * Get default flags for BASS_MusicLoad.
     */
    inline DWORD getDefaultMusicFlags() const {return defaultMusicFlags;}

protected:
    MSE_Engine* engine;
    MSE_SoundInitParams initParams;
    HCHANNEL handle;
    DWORD defaultStreamFlags;
    DWORD defaultMusicFlags;
    MSE_Playlist* playlist;
    MSE_SoundChannelType channelType;
    MSE_SoundChannelState channelState;
    MSE_Source* currentSource;
    MSE_SourceTags sourceTags;
    bool trackArtistFromTags;
    bool trackTitleFromTags;
    QString trackFormattedTitle;
    QString trackFilename;
    double trackDuration;
    double fullTrackDuration;
    QByteArray memFile;
    float volume;
    MSE_PlaylistFormatType remotePlaylistType;
    int errCount;
    HSYNC hSyncEnd;
    int endBytePos;
    int sampleRateConversion;
    QObject positionCallbacks;

#ifdef QT_NETWORK_LIB
    QEventLoop sockSync;
    int chunkLen;
    int chunkPos;
    int chunkLength;
    QPointer<QNetworkReply> curReply;
    QPointer<QNetworkReply> newReply;
    int metaLen;
    QByteArray metaData;
    static BASS_FILEPROCS fileProcTable;
    QNetworkAccessManager netMan;
#endif

    double remoteStreamShift;
    QTimer contStateTimer;
    MSE_SoundChannelState channelContState;

    bool openByOffset(int offset);
    bool playByOffset(int offset);
    void fillTrackInfo();
    void getChannelDurations(HCHANNEL theHandle, const MSE_Source* source, double& duration, double& fullDuration);
    bool open(MSE_Source *source);
    bool incErrCount();
    bool _openNextValid();
    bool _playNextValid();
    bool _openPrevValid();
    bool _playPrevValid();
    bool _openFirstValidInPrevDir();
    bool _openFirstValidInNextDir();
    bool _openFirstValidInDir();
    bool setEndSync(HCHANNEL theHandle, const MSE_Source *source);
    bool setPosSyncs(const MSE_Source *source);
    bool setPosSync(const MSE_Source *source, MSE_SoundPositionCallback* callback, double duration);
    void setState(MSE_SoundChannelState newState);
    void setContinuousState(MSE_SoundChannelState newState);

    virtual void onSyncEnd();
    virtual void onDSPProc(HDSP handle, void *buffer, DWORD length);
    virtual bool onRecordProc(const void *buffer, DWORD length);
    virtual void onSyncPos(HSYNC handle, MSE_SoundPositionCallback* callback);

private:
    static void CALLBACK syncEnd(HSYNC handle, DWORD channel, DWORD data, void *user);
    static void CALLBACK DSPProc(HDSP handle, DWORD channel, void *buffer, DWORD length, void *user);
    static BOOL CALLBACK recordProc(HRECORD handle, const void *buffer, DWORD length, void *user);
    static void CALLBACK syncPos(HSYNC handle, DWORD channel, DWORD data, void *user);

signals:
    /*!
     * Emitted when a current sound source has reached its end.
     * This doesn't apply to cases when a sound had been stopped by MSE functions.
     *
     * \warn You should not change a track state in any way in a slots connected to this signal!
     * For example, you should not jump to a next track.
     */
    void onPlayEnd();

    /*!
     * Emitted when a sound source (file or URL) is open.
     */
    void onOpen();

    /*!
     * Emitted every time a new meta-information (tags) is resolved for a current sound source.
     * For common files it occurs immediately after a file is open.
     * If a current sound source is a SHOUTcast stream,
     * then this signal will be emitted every time new meta-information arrives.
     *
     * \sa getTrackArtist, getTrackTitle, getTrackFormattedTitle
     */
    void onInfoChange();

    /*!
     * Emitted when channel state changes.
     *
     * \sa getState
     */
    void onStateChange();

    /*!
     * Same as onStateChange, but only emitted
     * when the state is changed after a function call.
     * Also it will be set to mse_scsPlaying when opening a remote URL.
     * This eleminates "jitter" when the channel state
     * changes multiple times per function.
     *
     * \sa onStateChange, getState
     */
    void onContinuousStateChange();

    /*!
     * User defined DSP callback function.
     *
     * This signal will be emitted every time a new sound data is decoded for a playback.
     * *buffer* is a pointer to the sample data to apply the DSP to.
     * The data is as follows: 8-bit samples are unsigned,
     * 16-bit samples are signed, 32-bit floating-point samples range from -1 to +1
     * (not clipped, so can actually be outside this range).
     *
     * \note A DSP function should be as quick as possible;
     * playing streams and MOD musics, and other DSP functions cannot be processed until it has finished.
     *
     * \note You should set MSE_SoundInitParams.enableDSP flag when initializing a sound object.
     *
     * \sa MSE_SoundInitParams.enableDSP, init, onDSP
     */
    void onDSP(void *buffer, quint32 length);

    /*!
     * When a sound object has mse_sctRecord type,
     * then this signal will be emitted every time a new data is captured by a recording device.
     * The format of data depends on a value returned by getType().
     * See onDSP() signal description for more information.
     *
     * \sa onDSP
     */
    void onRecordData(const void *buffer, quint32 length);

    void onPosSync(MSE_Sound* sender, MSE_SoundPositionCallback* callback);

    void onVolumeChange();
    void onPositionChange();

private slots:
    void invokePlayNextValid();
#ifdef QT_NETWORK_LIB
    void onSockReadyRead();
#endif
    void onContStateTimer();

public slots:
    void onMeta();
};
