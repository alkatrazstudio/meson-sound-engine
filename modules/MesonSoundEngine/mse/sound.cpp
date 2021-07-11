/****************************************************************************}
{ sound.cpp - playback & decoding                                            }
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

#include "mse/sound.h"

void CALLBACK MSE_Sound::syncEnd(HSYNC handle, DWORD channel, DWORD data, void *user)
{
    Q_UNUSED(handle);
    Q_UNUSED(channel);
    Q_UNUSED(data);
    static_cast<MSE_Sound*>(user)->onSyncEnd();
}

void MSE_Sound::onSyncEnd()
{
    emit onPlayEnd();
    // since it's a different thread
    // we'll only send signals, because
    // it's unsafe to call the following method directly from this thread
    QTimer::singleShot(0, this, SLOT(invokePlayNextValid()));
}

void CALLBACK MSE_Sound::DSPProc(HDSP handle, DWORD channel, void *buffer, DWORD length, void *user)
{
    Q_UNUSED(channel);
    static_cast<MSE_Sound*>(user)->onDSPProc(handle, buffer, length);
}

void MSE_Sound::onDSPProc(HDSP handle, void *buffer, DWORD length)
{
    Q_UNUSED(handle);
    emit onDSP(buffer, length);
}

BOOL CALLBACK MSE_Sound::recordProc(HRECORD handle, const void *buffer, DWORD length, void *user)
{
    Q_UNUSED(handle);
    return static_cast<MSE_Sound*>(user)->onRecordProc(buffer, length);
}

bool MSE_Sound::onRecordProc(const void *buffer, DWORD length)
{
    emit onRecordData(buffer, length);
    return true;
}

void MSE_Sound::syncPos(HSYNC handle, DWORD channel, DWORD data, void *user)
{
    Q_UNUSED(channel);
    Q_UNUSED(data);
    MSE_SoundPositionCallback* callback = static_cast<MSE_SoundPositionCallback*>(user);
    callback->getSound()->onSyncPos(handle, callback);
}

void MSE_Sound::onSyncPos(HSYNC handle, MSE_SoundPositionCallback *callback)
{
    Q_UNUSED(handle);
    if(positionCallbacks.children().indexOf(callback) < 0)
        return;
    callback->invoke();
}

/*!
 * Constructs MSE_Sound instance.
 */
MSE_Sound::MSE_Sound() : MSE_Object()
{
    engine = MSE_Engine::getInstance();
    handle = 0;
    channelType = mse_sctUnknown;
    channelState = mse_scsIdle;
    channelContState = mse_scsIdle;
    playlist = new MSE_Playlist(this);
    volume = 1;
    errCount = 0;
    currentSource = nullptr;
    hSyncEnd = 0;
    endBytePos = 0;
    sampleRateConversion = 0;
    trackArtistFromTags = false;
    trackTitleFromTags = false;
    contStateTimer.setInterval(0);
    contStateTimer.setSingleShot(true);
    connect(&contStateTimer, SIGNAL(timeout()), SLOT(onContStateTimer()));
}

/*!
 * Destroys the object.
 * Make sure to destroy all children objects first if any.
 */
MSE_Sound::~MSE_Sound()
{
    close();
    onContStateTimer(); // because contStateTimer will be dead already
    delete playlist;
}

/*!
 * Initializes the sound object.
 *
 * If params not specified, then the object will be initialized with a default parameters.
 *
 * Refer to MSE_SoundInitParams for the information about initialization parameters.
 *
 * \sa getDefaultInitParams
 */
bool MSE_Sound::init(const MSE_SoundInitParams& params)
{
    initParams = params;
    CHECK(!initParams.use3D || engine->getIs3DSupported(), MSE_Object::Err::no3dSupport);

#ifdef Q_OS_WIN
    defaultStreamFlags = BASS_UNICODE;
#else
    defaultStreamFlags = 0;
#endif
    if(initParams.use3D)
        defaultStreamFlags |= BASS_SAMPLE_3D;
    switch(initParams.sampleType)
    {
        case mse_sst8Bits:
            defaultStreamFlags |= BASS_SAMPLE_8BITS;
            break;
        case mse_sstFloat32:
            defaultStreamFlags |= BASS_SAMPLE_FLOAT;
            break;
        default:
            break;
    }
    if(initParams.useOldFx)
        defaultStreamFlags |= BASS_SAMPLE_FX;
    if(initParams.useSoftware)
        defaultStreamFlags |= BASS_SAMPLE_SOFTWARE;

    defaultMusicFlags = defaultStreamFlags;

    if(initParams.decodeOnly)
    {
        defaultStreamFlags |= BASS_STREAM_DECODE;
        defaultMusicFlags |= BASS_MUSIC_DECODE;
    }
    if(initParams.doPrescan)
    {
        defaultStreamFlags |= BASS_STREAM_PRESCAN;
        defaultMusicFlags |= BASS_MUSIC_PRESCAN;
    }
    switch(initParams.sampleInterpolation)
    {
        case mse_ssiNone:
            defaultMusicFlags |= BASS_MUSIC_NONINTER;
            break;
        case mse_ssiSinc:
            defaultMusicFlags |= BASS_MUSIC_SINCINTER;
            break;
        default:
            break;
    }
    switch(initParams.sampleRamping)
    {
        case mse_ssrNormal:
            defaultMusicFlags |= BASS_MUSIC_RAMP;
            break;
        case mse_ssrSensitive:
            defaultMusicFlags |= BASS_MUSIC_RAMPS;
            break;
        default:
            break;
    }
    switch(initParams.surroundMode)
    {
        case mse_ssmMode1:
            defaultMusicFlags |= BASS_MUSIC_SURROUND;
            break;
        case mse_ssmMode2:
            defaultMusicFlags |= BASS_MUSIC_SURROUND2;
            break;
        default:
            break;
    }
    switch(initParams.trackerEmulation)
    {
        case mse_steFastTracker2:
            defaultMusicFlags |= BASS_MUSIC_FT2MOD;
            break;
        case mse_steProTracker1:
            defaultMusicFlags |= BASS_MUSIC_PT1MOD;
            break;
        default:
            break;
    }

    sampleRateConversion = sampleRateConversionFromSincPoints(initParams.sincPoints);
    initParams.sincPoints = sincPointsFromSampleRateConversion(sampleRateConversion);

    return true;
}

/*!
 * Loads a sound file.
 * The file cannot be a playlist.
 * This function will clear the playlist first.
 */
bool MSE_Sound::openFromFile(const MSE_PlaylistEntry& entry)
{
    if(!playlist->setFile(entry))
        return false;
    return openFromList(0);
}

/*!
 * Starts playing a next file.
 */
bool MSE_Sound::playNext()
{
    if(!openNext())
        return false;
    return play();
}

/*!
 * Starts playing a (possible) previous file.
 */
bool MSE_Sound::playPrev()
{
    if(!openPrev())
        return false;
    return play();
}

/*!
 * If the *offset* is non-negative,
 * then open the next file in a playlist,
 * otherwise - open previous file.
 */
bool MSE_Sound::openByOffset(int offset)
{
    if(offset >= 0)
        playlist->moveToNext();
    else
        playlist->moveToPrev();
    return open();
}

/*!
 * Opens a file at a specified playlist index.
 *
 * \sa playFromList
 */
bool MSE_Sound::openFromList(int index)
{
    if(!playlist->setIndex(index))
        return false;
    return open();
}

/*!
 * Opens a current sound source.
 *
 * \sa MSE_Playlist::getCurrentSource, close
 */
bool MSE_Sound::open()
{
    MSE_Source* src = playlist->getCurrentSource();
    if(!src)
        return false;
    return open(src);
}

/*!
 * Closes a currently opened audio source.
 *
 * \sa open
 */
bool MSE_Sound::close()
{
    if(currentSource)
        disconnect(currentSource, SIGNAL(onMeta()), this, SLOT(onMeta()));

    if(!stop())
        return false;

    if(currentSource)
        if(!currentSource->close())
            return false;

    channelType = mse_sctUnknown;
    currentSource = nullptr;
    hSyncEnd = 0;
    endBytePos = 0;
    handle = 0;
    sourceTags.clear();
    trackFilename.clear();
    trackDuration = -1;
    fullTrackDuration = -1;
    return true;
}

/*!
 * Stops a playback.
 */
bool MSE_Sound::stop()
{
    if(channelState != mse_scsIdle)
    {
        BASS_ChannelStop(handle); // makes no sense to check for the error here
        setState(mse_scsIdle);
    }
    else
    {
        if(channelContState != mse_scsIdle)
            setState(mse_scsIdle);
    }
    return true;
}

/*!
 * Pauses a playback.
 *
 * \sa unpause
 */
bool MSE_Sound::pause()
{
    CHECK(channelState == mse_scsPlaying, MSE_Object::Err::invalidState);
    if(!initParams.decodeOnly)
        CHECK(BASS_ChannelPause(handle), MSE_Object::Err::operationFailed);
    setState(mse_scsPaused);
    return true;
}

/*!
 * Unpauses a playback if it is paused.
 */
bool MSE_Sound::unpause()
{
    CHECK(channelState == mse_scsPaused, MSE_Object::Err::invalidState);
    if(!initParams.decodeOnly)
        CHECK(BASS_ChannelPlay(handle, false), MSE_Object::Err::operationFailed);
    setState(mse_scsPlaying);
    return true;
}

/*!
 * If a playback is paused, then this function unpauses it.
 * If a playback is stopped, then this function starts it.
 */
bool MSE_Sound::playOrUnpause()
{
    if(channelState == mse_scsPaused)
        return unpause();
    else
        return play();
}

/*!
 * Restarts a playback from a beginning of a current audio source.
 */
bool MSE_Sound::restart()
{
    stop();
    return play();
}

/*!
 * Starts a playback of a current audio source.
 */
bool MSE_Sound::play()
{
    if(!currentSource)
        return false;

    int pos;
    if(currentSource->cueSheetTrack)
        pos = BASS_ChannelSeconds2Bytes(handle, currentSource->cueSheetTrack->startPos);
    else
        pos = 0;
    BASS_ChannelSetPosition(handle, pos, BASS_POS_BYTE);

    if(!initParams.decodeOnly)
    {
        BASS_ChannelUpdate(handle, 0);
        if(!BASS_ChannelPlay(handle, pos == 0))
        {
            stop();
            return false;
        }
    }

    if(channelState != mse_scsPlaying)
        setState(mse_scsPlaying);

    return true;
}

/*!
 * Retrieves the immediate sample data.
 *
 * \param buffer
 * Pointer to a buffer to receive the data.
 *
 * \param length
 * Number of bytes wanted.
 */
int MSE_Sound::getData(void *buffer, int length)
{
    if(channelState != mse_scsPlaying)
        return -1;
    if(endBytePos)
    {
        int decodingPos = BASS_ChannelGetPosition(handle, BASS_POS_DECODE);
        if(decodingPos < 0)
            return -1;
        length = std::min(length, endBytePos - decodingPos);
    }
    return BASS_ChannelGetData(handle, buffer, length);
}

/*!
 * Returns the position of a current track in seconds.
 * Returns negative value, if no source loaded.
 * Returns 0 if track is stopped.
 *
 * This function returns an actual position in stream
 * which may not match a current track position
 * (this is a case for CUE-splitted files and Internet-radio streams).
 */
double MSE_Sound::getRealPosition()
{
    if(!currentSource)
        return -1;
    if(channelState == mse_scsIdle)
        return 0;
    QWORD bytes = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);
    if(bytes == 0xFFFFFFFFFFFFFFFF)
        return -1;
    double secs = BASS_ChannelBytes2Seconds(handle, bytes);
    if(secs < 0)
        return -1;
    return secs;
}

/*!
 * Same as getRealPosition(),
 * but returns a track position instead of a stream position.
 * If a stream contains more than one track (CUE-splitted files, Internet-radio),
 * then those position values may differ.
 */
double MSE_Sound::getPosition()
{
    double secs = getRealPosition();
    if(secs <= 0)
        return secs;
    if(currentSource->cueSheetTrack)
    {
        secs = secs - currentSource->cueSheetTrack->startPos;
    }
    else
    {
        if(channelType == mse_sctRemote)
            secs = secs - remoteStreamShift;
    }
    return secs;
}

bool MSE_Sound::setPosition(double secs)
{
    if(secs < 0)
    {
        secs = 0;
    }
    else
    {
        if(secs > getTrackDuration())
            secs = getTrackDuration();
    }
    if(currentSource->cueSheetTrack)
    {
        secs = secs + currentSource->cueSheetTrack->startPos;
    }
    else
    {
        if(channelType == mse_sctRemote)
            secs = secs + remoteStreamShift;
    }
    QWORD bytes = BASS_ChannelSeconds2Bytes(handle, secs);
    if(bytes == 0xFFFFFFFFFFFFFFFF)
        return false;
    if(!BASS_ChannelSetPosition(handle, bytes, BASS_POS_BYTE))
        return false;
    emit onPositionChange();
    return true;
}

/*!
 * Sets the volume for a sound object.
 * Valid values are in range [0;1].
 */
bool MSE_Sound::setVolume(float value)
{
    if(value < 0)
    {
        value = 0;
    }
    else
    {
        if(value > 1)
            value = 1;
    }
    volume = value;
    if(handle)
    {
        if(!BASS_ChannelSetAttribute(handle, BASS_ATTRIB_VOL, volume))
            return false;
        refreshVolume();
        emit onVolumeChange();
        return true;
    }
    else
    {
        emit onVolumeChange();
        return true;
    }
}

/*!
 * Returns a MSE_SoundSampleType value by its name.
 * If a return value cannot be determined then the function returns mse_sstFloat32
 * and sets *ok* (if it's not nullptr) to false, otherwise - sets ok to true.
 *
 *  *str*   | result
 * --------------------------
 *  normal  | mse_sstNormal
 *  8bits   | mse_sst8Bits
 *  float32 | mse_sstFloat32
 */
MSE_SoundSampleType MSE_Sound::sampleTypeFromString(const QString &str, bool *ok)
{
    if(str == "normal")
    {
        if(ok)
            *ok = true;
        return mse_sstNormal;
    }
    if(str == "8bits")
    {
        if(ok)
            *ok = true;
        return mse_sst8Bits;
    }
    if(str == "float32")
    {
        if(ok)
            *ok = true;
        return mse_sstFloat32;
    }
    if(ok)
        *ok = false;
    return mse_sstFloat32;
}

/*!
 * Returns a MSE_SoundSampleInterpolation value by its name.
 * If a return value cannot be determined then the function returns mse_ssiSinc
 * and sets *ok* (if it's not nullptr) to false, otherwise - sets ok to true.
 *
 *  *str*  | result
 * ------------------------
 *  linear | mse_ssiLinear
 *  none   | mse_ssiNone
 *  sinc   | mse_ssiSinc
 */
MSE_SoundSampleInterpolation MSE_Sound::sampleInterpolationFromString(const QString &str, bool *ok)
{
    if(str == "linear")
    {
        if(ok)
            *ok = true;
        return mse_ssiLinear;
    }
    if(str == "none")
    {
        if(ok)
            *ok = true;
        return mse_ssiNone;
    }
    if(str == "sinc")
    {
        if(ok)
            *ok = true;
        return mse_ssiSinc;
    }
    if(ok)
        *ok = false;
    return mse_ssiSinc;
}

/*!
 * Returns a MSE_SoundSampleRamping value by its name.
 * If a return value cannot be determined then the function returns mse_ssrSensitive
 * and sets *ok* (if it's not nullptr) to false, otherwise - sets ok to true.
 *
 *  *str*     | result
 * ------------------------------
 *  none      | mse_ssrNone
 *  normal    | mse_ssrNormal
 *  sensitive | mse_ssrSensitive
 */
MSE_SoundSampleRamping MSE_Sound::sampleRampingFromString(const QString &str, bool *ok)
{
    if(str == "none")
    {
        if(ok)
            *ok = true;
        return mse_ssrNone;
    }
    if(str == "normal")
    {
        if(ok)
            *ok = true;
        return mse_ssrNormal;
    }
    if(str == "sensitive")
    {
        if(ok)
            *ok = true;
        return mse_ssrSensitive;
    }
    if(ok)
        *ok = false;
    return mse_ssrSensitive;
}

/*!
 * Returns a MSE_SoundSurroundMode value by its name.
 * If a return value cannot be determined then the function returns mse_ssmNone
 * and sets *ok* (if it's not nullptr) to false, otherwise - sets ok to true.
 *
 *  *str* | result
 * ----------------------
 *  none  | mse_ssmNone
 *  mode1 | mse_ssmMode1
 *  mode2 | mse_ssmMode2
 */
MSE_SoundSurroundMode MSE_Sound::surroundModeFromString(const QString &str, bool *ok)
{
    if(str == "none")
    {
        if(ok)
            *ok = true;
        return mse_ssmNone;
    }
    if(str == "mode1")
    {
        if(ok)
            *ok = true;
        return mse_ssmMode1;
    }
    if(str == "mode2")
    {
        if(ok)
            *ok = true;
        return mse_ssmMode2;
    }
    if(ok)
        *ok = false;
    return mse_ssmNone;
}

/*!
 * Returns a MSE_SoundTrackerEmulation value by its name.
 * If a return value cannot be determined then the function returns mse_steNone
 * and sets *ok* (if it's not nullptr) to false, otherwise - sets ok to true.
 *
 *  *str*        | result
 * ------------------------------------
 *  none         | mse_steNone
 *  fastTracker2 | mse_steFastTracker2
 *  proTracker1  | mse_steProTracker1
 */
MSE_SoundTrackerEmulation MSE_Sound::trackerEmulationFromString(const QString &str, bool *ok)
{
    if(str == "none")
    {
        if(ok)
            *ok = true;
        return mse_steNone;
    }
    if(str == "fastTracker2")
    {
        if(ok)
            *ok = true;
        return mse_steFastTracker2;
    }
    if(str == "proTracker1")
    {
        if(ok)
            *ok = true;
        return mse_steProTracker1;
    }
    if(ok)
        *ok = false;
    return mse_steNone;
}

/*!
 * Returns a sample rate conversion ratio that will be used for a specified number of sinc points.
 * A sample rate conversion ratio is used when setting BASS_ATTRIB_SRC parameter of a sound channel.
 *
 *  *count*          | result
 * ---------------------------
 *  count <= 0       | 0
 *  1 <= count <= 8  | 1
 *  9 <= count <= 16 | 2
 *  count > 16       | 3
 *
 * \sa sincPointsFromSampleRateConversion
 */
int MSE_Sound::sampleRateConversionFromSincPoints(int count)
{
    if(count <= 0)
        return 0;
    if(count <= 8)
        return 1;
    if(count <= 16)
        return 2;
    return 3;
}

/*!
 * Returns a number of sinc points used for a specified sample rate converison ratio.
 *
 *  *value* | result
 * -------------------------
 *  <= 0    | 0
 *  1       | 8
 *  2       | 16
 *  > 2     | 32
 *
 * \sa sampleRateConversionFromSincPoints
 */
int MSE_Sound::sincPointsFromSampleRateConversion(int value)
{
    switch(value)
    {
        case 1: return 8;
        case 2: return 16;
    }
    if(value <= 0)
        return 0;
    else
        return 32;
}

/*!
 * Returns a MSE_SoundChannelState value by its name.
 * If a return value cannot be determined then the function returns mse_scsIdle
 * and sets *ok* (if it's not nullptr) to false, otherwise - sets ok to true.
 *
 *  *str*   | result
 * --------------------------
 *  idle    | mse_scsIdle
 *  paused  | mse_scsPaused
 *  playing | mse_scsPlaying
 */
MSE_SoundChannelState MSE_Sound::channelStateFromString(const QString &str, bool *ok)
{
    if(str == "idle")
    {
        if(ok)
            *ok = true;
        return mse_scsIdle;
    }
    if(str == "paused")
    {
        if(ok)
            *ok = true;
        return mse_scsPaused;
    }
    if(str == "playing")
    {
        if(ok)
            *ok = true;
        return mse_scsPlaying;
    }
    if(ok)
        *ok = false;
    return mse_scsIdle;
}

/*!
 * Returns a string representaion of a given MSE_SoundChannelState value.
 * Returns an empty string for an invalid value.
 *
 * \sa playbackModeFromString
 */
QString MSE_Sound::channelStateToString(MSE_SoundChannelState state)
{
    switch(state)
    {
        case mse_scsIdle: return QStringLiteral("idle");
        case mse_scsPaused: return QStringLiteral("paused");
        case mse_scsPlaying: return QStringLiteral("playing");
        default: return QString();
    }
}

/*!
 * Returns a directory name for a specified filename.
 * If *base* is true, the the function returns only basename of a directory,
 * not a full directory path.
 */
QString MSE_Sound::getDirName(const QString& source, bool base) const
{
    MSE_SoundChannelType sourceType = engine->typeByUri(source);
    switch(sourceType)
    {
        case mse_sctUnknown:
        case mse_sctRemote:
            return source;

        default:
            QFileInfo fInfo(source);
            if(!base)
                return fInfo.absolutePath();
            QFileInfo dInfo(fInfo.absolutePath());
            return dInfo.completeBaseName();
    }
}

/*!
 * Returns the sample frequency of a currently opened audio.
 * Returns zero if an audio channel is not open or a frequency information cannot be retrieved.
 */
int MSE_Sound::getFrequency()
{
    float freq;
    if(!BASS_ChannelGetAttribute(handle, BASS_ATTRIB_FREQ, &freq))
        return 0;
    return freq;
}

/*!
 * Returns the number of channels of a currently opened audio.
 * Returns zero if an audio channel is not open or a frequency information cannot be retrieved.
 */
int MSE_Sound::getChannelsCount()
{
    BASS_CHANNELINFO info;
    if(!BASS_ChannelGetInfo(handle, &info))
        return 0;
    return info.chans;
}

/*!
 * Installs a new callback which will be called when the track reaches the specified position.
 *
 * \param pos
 * Position in stream in seconds when a callback should be called.
 * A negative value means that the callback should be invoked
 * when *pos* seconds of a playback is left.
 *
 * If the resulting position is out of a track length,
 * then the callback will be fired at the end of a track if *pos* &gt; 0,
 * or at the start of a track if *pos &lt; 0.
 *
 * \param receiver
 * The owner of the callback.
 *
 * \param member
 * SLOT to call back.
 *
 * \return This function returns a callback information object.
 * Delete this object to remove a callback.
 */
MSE_SoundPositionCallback *MSE_Sound::installPositionCallback(int pos, MSE_SoundPositionCallbackFunc func, void *data)
{
    MSE_SoundPositionCallback* callback = new MSE_SoundPositionCallback(&positionCallbacks, pos, func, data);
    if(currentSource && handle)
        setPosSync(currentSource, callback, trackDuration);
    return callback;
}

/*!
 * Plays a file at a specifeid playlist index.
 *
 * \sa openFromList
 */
bool MSE_Sound::playFromList(int index)
{
    if(!openFromList(index))
        return false;
    if(!play())
        return false;
    return true;
}

bool MSE_Sound::openNextValid()
{
    errCount = 0;
    return _openNextValid();
}

bool MSE_Sound::openPrevValid()
{
    errCount = 0;
    return _openPrevValid();
}

bool MSE_Sound::openFirstValidInPrevDir()
{
    errCount = 0;
    return _openFirstValidInPrevDir();
}

bool MSE_Sound::openFirstValidInNextDir()
{
    errCount = 0;
    return _openFirstValidInNextDir();
}

bool MSE_Sound::openFirstValidInDir()
{
    errCount = 0;
    return _openFirstValidInDir();
}

bool MSE_Sound::_openNextValid()
{
    int curIndex = playlist->getIndex();
    stop();

    while(!openNext())
    {
        if(!incErrCount())
        {
            if(curIndex >= 0)
                playlist->setIndex(curIndex);
            open();
            close();
            return false;
        }
    }

    return true;
}

bool MSE_Sound::_openPrevValid()
{
    int curIndex = playlist->getIndex();
    stop();

    while(!openPrev())
    {
        if(!incErrCount())
        {
            if(curIndex >= 0)
                playlist->setIndex(curIndex);
            open();
            close();
            return false;
        }
    }

    return true;
}

bool MSE_Sound::openNext()
{
    playlist->moveToNext();
    return open();
}

bool MSE_Sound::openPrev()
{
    playlist->moveToPrev();
    return open();
}

bool MSE_Sound::openValid(int index)
{
    if(!playlist->setIndex(index))
        return false;
    if(!open())
        return openNextValid();
    return true;
}

bool MSE_Sound::playValid(int index)
{
    if(openValid(index))
        return play();
    else
        return false;
}

bool MSE_Sound::_playNextValid()
{
    if(!_openNextValid())
        return false;
    return play();
}

bool MSE_Sound::_playPrevValid()
{
    if(!_openPrevValid())
        return false;
    return play();
}

bool MSE_Sound::_openFirstValidInPrevDir()
{
    stop();
    int tmpIndex = playlist->getIndex();
    if(!playlist->moveToFirstInPrevDir())
    {
        playlist->setIndex(tmpIndex);
        return false;
    }
    if(open())
        return true;
    if(!incErrCount())
        return false;
    if(!playlist->getCurrentSource())
        return false;

    forever
    {
        if(playlist->isLastInDir())
        {
            if(!playlist->moveToFirstInPrevDir())
            {
                playlist->setIndex(tmpIndex);
                return false;
            }
            if(open())
                return true;
            if(!incErrCount())
                return false;
            if(!playlist->getCurrentSource())
                return false;
            continue;
        }
        if(!playlist->moveToNext())
        {
            playlist->setIndex(tmpIndex);
            return false;
        }
        if(open())
            return true;
        if(!incErrCount())
            return false;
        if(!playlist->getCurrentSource())
            return false;
    }

    return true;
}

bool MSE_Sound::_openFirstValidInNextDir()
{
    stop();
    int tmpIndex = playlist->getIndex();

    if(!playlist->moveToFirstInNextDir())
    {
        playlist->setIndex(tmpIndex);
        return false;
    }

    if(open())
        return true;
    if(!incErrCount())
        return false;
    return _openNextValid();
}

bool MSE_Sound::_openFirstValidInDir()
{
    stop();
    int tmpIndex = playlist->getIndex();

    if(!playlist->moveToFirstInDir())
    {
        playlist->setIndex(tmpIndex);
        return false;
    }

    if(open())
        return true;
    if(!incErrCount())
        return false;
    return _openNextValid();
}


bool MSE_Sound::setEndSync(HCHANNEL theHandle, const MSE_Source *source)
{
    if(theHandle == handle)
        if(hSyncEnd)
            BASS_ChannelRemoveSync(theHandle, hSyncEnd);

    if(source->cueSheetTrack)
    {
        if(source->cueSheetTrack->endPos)
        {
            endBytePos = BASS_ChannelSeconds2Bytes(theHandle, source->cueSheetTrack->endPos);
            hSyncEnd = BASS_ChannelSetSync(
                        theHandle,
                        BASS_SYNC_POS,
                        endBytePos,
                        &MSE_Sound::syncEnd, this);
            return hSyncEnd != 0;
        }
    }
    endBytePos = 0;
    hSyncEnd = BASS_ChannelSetSync(theHandle, BASS_SYNC_END, 0, &MSE_Sound::syncEnd, this);
    return hSyncEnd != 0;
}

bool MSE_Sound::setPosSyncs(const MSE_Source *source)
{
    if(positionCallbacks.children().isEmpty())
        return true;
    double duration, fullDuration;
    getChannelDurations(handle, source, duration, fullDuration);
    bool result = true;
    foreach(QObject* callback, positionCallbacks.children())
        if(!setPosSync(source, static_cast<MSE_SoundPositionCallback*>(callback), duration))
            result = false;
    return result;
}

bool MSE_Sound::setPosSync(
        const MSE_Source *source,
        MSE_SoundPositionCallback *callback, double duration)
{
    if(callback->sync && callback->channel)
        BASS_ChannelRemoveSync(callback->channel, callback->sync);

    double pos = callback->getPos();
    if(pos > 0)
    {
        if(duration >= 0)
        {
            if(pos > duration)
                pos = duration;
        }
    }
    else
    {
        if(duration >= 0)
        {
            pos = duration + pos;
            if(pos < 0)
                pos = 0;
        }
        else
        {
            // trying to set a negative position on a stream without a known duration
            return false;
        }
    }
    if(source->cueSheetTrack)
    {
        pos = pos + source->cueSheetTrack->startPos;
    }
    else
    {
        if(source->type == mse_sctRemote)
            pos = pos + remoteStreamShift;
    }
    pos = BASS_ChannelSeconds2Bytes(handle, pos);
    callback->sync = BASS_ChannelSetSync(handle, BASS_SYNC_POS, pos, &MSE_Sound::syncPos, callback);
    callback->sound = this;
    if(callback->sync)
    {
        callback->channel = handle;
        return true;
    }
    else
    {
        callback->channel = 0;
        return false;
    }
}

void MSE_Sound::setState(MSE_SoundChannelState newState)
{
    channelState = newState;
    emit onStateChange();
    contStateTimer.start();
}

void MSE_Sound::setContinuousState(MSE_SoundChannelState newState)
{
    contStateTimer.stop();
    if(newState != channelContState)
    {
        channelContState = newState;
        emit onContinuousStateChange();
    }
}

bool MSE_Sound::playNextValid()
{
    if(!openNextValid())
        return false;
    return play();
}

bool MSE_Sound::playPrevValid()
{
    if(!openPrevValid())
        return false;
    return play();
}

bool MSE_Sound::playFirstValidInNextDir()
{
    if(!openFirstValidInNextDir())
        return false;
    return play();
}

bool MSE_Sound::playFirstValidInPrevDir()
{
    if(!openFirstValidInPrevDir())
        return false;
    return play();
}

bool MSE_Sound::playFirstValidInDir()
{
    if(!openFirstValidInDir())
        return false;
    return play();
}

void MSE_Sound::fillTrackInfo()
{
    trackFilename = currentSource->entry.filename;

    QFileInfo info;

    if(channelType != mse_sctRemote)
    {
        info.setFile(trackFilename);
        getChannelDurations(handle, currentSource, trackDuration, fullTrackDuration);
    }
    else
    {
        trackDuration = -1;
        fullTrackDuration = -1;
    }

    currentSource->fillTags(sourceTags);

    trackArtistFromTags = !sourceTags.trackArtist.isEmpty();
    trackTitleFromTags = !sourceTags.trackTitle.isEmpty();

    if(!trackTitleFromTags)
    {
        if(channelType == mse_sctRemote)
            sourceTags.trackTitle = trackFilename;
        else
            sourceTags.trackTitle = info.completeBaseName();
    }

    if(!trackArtistFromTags)
        trackFormattedTitle = sourceTags.trackTitle;
    else
        trackFormattedTitle = sourceTags.trackArtist+" - "+sourceTags.trackTitle;

    emit onInfoChange();
    if(channelType == mse_sctRemote)
        remoteStreamShift = getRealPosition();
    setPosSyncs(currentSource);
}

void MSE_Sound::getChannelDurations(HCHANNEL theHandle, const MSE_Source* source, double &duration, double &fullDuration)
{
    QWORD channelLength = BASS_ChannelGetLength(theHandle, BASS_POS_BYTE);
    if(channelLength != 0xFFFFFFFFFFFFFFFF)
    {
        double channelLengthSecs = BASS_ChannelBytes2Seconds(handle, channelLength);
        if(channelLengthSecs >= 0)
        {
            fullDuration = channelLengthSecs;
            if(source->cueSheetTrack)
            {
                if(source->cueSheetTrack->endPos)
                    duration = source->cueSheetTrack->endPos - source->cueSheetTrack->startPos;
                else
                    duration = fullDuration - source->cueSheetTrack->startPos;
            }
            else
            {
                duration = fullDuration;
            }
        }
        else
        {
            fullDuration = -1;
            duration = -1;
        }
    }
    else
    {
        fullDuration = -1;
        duration = -1;
    }
}

void MSE_Sound::refreshVolume()
{
    if(handle)
    {
        if(!BASS_ChannelGetAttribute(handle, BASS_ATTRIB_VOL, &volume))
            volume = 0;
    }
}

/*!
 * Increases/decreases the sound volume level.
 * Returns true on success.
 *
 * \sa snapVolumeToGrid
 */
bool MSE_Sound::changeVolume(float diff, bool snapToGrid)
{
    refreshVolume();
    float val = volume + diff;
    if(snapToGrid)
        val = engine->snapVolumeToGrid(val, diff);
    return setVolume(val);
}

bool MSE_Sound::open(MSE_Source* source)
{
    // if source is in the same file as currentSource (.cue-splitted files)
    // then there's no need to load the file again
    if(currentSource && source->cueSheetTrack)
    {
        if(currentSource->cueSheetTrack)
        {
            if(source->cueSheetTrack->sheet == currentSource->cueSheetTrack->sheet)
            {
                setEndSync(handle, source);
                currentSource = source;
                fillTrackInfo();
                if(channelState == mse_scsPlaying)
                    return play();
                else
                    return true;
            }
        }
    }

    if(!close())
        return false;

    HCHANNEL newHandle = source->open();
    CHECK(newHandle, MSE_Object::Err::cannotLoadSound, source->entry.filename);

    if(currentSource != source)
        if(!close())
            return false;

    handle = newHandle;

    switch(source->type)
    {
        case mse_sctRemote:
        case mse_sctModule:
        case mse_sctStream:
        case mse_sctPlugin:
            if(!setEndSync(handle, source))
            {
                close();
                return false;
            }

            if(initParams.enableDSP)
            {
                if(!BASS_ChannelSetDSP(handle, &MSE_Sound::DSPProc, this, 0))
                {
                    close();
                    return false;
                }
            }
            break;

        default:
            break;
    }

    channelType = source->type;
    currentSource = source;

    fillTrackInfo();
    BASS_ChannelSetAttribute(handle, BASS_ATTRIB_VOL, volume);
    BASS_ChannelSetAttribute(handle, BASS_ATTRIB_SRC, sampleRateConversion);
    connect(currentSource, SIGNAL(onMeta()), this, SLOT(onMeta()));

    emit onOpen();

    errCount = 0;

    return true;
}

bool MSE_Sound::incErrCount()
{
    if(playlist->getIndex() == -1)
    {
        SETERROR(MSE_Object::Err::noValidFilesFound);
        return false;
    }
    errCount++;
    int n = playlist->getList()->size();
    CHECK(errCount < n, MSE_Object::Err::noValidFilesFound);
    return true;
}

void MSE_Sound::invokePlayNextValid()
{
    // only valid if called from onSyncEnd()
    if(currentSource)
    {
        if(currentSource->cueSheetTrack)
        {
            MSE_Source* nextSource = playlist->getNextSource();
            if(nextSource)
            {
                if(nextSource->cueSheetTrack)
                {
                    if(nextSource->cueSheetTrack->sheet == currentSource->cueSheetTrack->sheet)
                    {
                        if(nextSource->cueSheetTrack->index == (currentSource->cueSheetTrack->index+1))
                        {
                            // if the next source is the next track in .cue sheet
                            // then there's no need to call open() at all
                            playlist->moveToNext();
                            setEndSync(handle, nextSource);
                            setPosSyncs(nextSource);
                            currentSource = nextSource;
                            fillTrackInfo();
                            return;
                        }
                    }
                }
            }
        }
    }

    _playNextValid();
}

#ifdef QT_NETWORK_LIB
void MSE_Sound::onSockReadyRead()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if(!reply)
        return;
    if(reply->bytesAvailable() >= 256)
        QTimer::singleShot(0, &sockSync, SLOT(quit()));
}
#endif

void MSE_Sound::onContStateTimer()
{
    setContinuousState(channelState);
}

void MSE_Sound::onMeta()
{
    if(qobject_cast<MSE_Source*>(sender()) == currentSource)
        fillTrackInfo();
}

bool MSE_Sound::startRecord()
{
    HCHANNEL newHandle = BASS_RecordStart(
                engine->getInitParams().outputFrequency,
                2,
                BASS_SAMPLE_FLOAT,
                &MSE_Sound::recordProc,
                this);
    if(!newHandle)
        return false;

    if(!close())
    {
        BASS_StreamFree(newHandle);
        return false;
    }

    handle = newHandle;
    channelType = mse_sctRecord;
    setState(mse_scsPlaying);
    return true;
}
