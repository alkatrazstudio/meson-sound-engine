/****************************************************************************}
{ types.h - general includes & definitions                                   }
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

#include "errormanager.h"

#ifdef Q_OS_WIN
    #include <windows.h>
    #include <ks.h>
    #include <ksmedia.h>
    #include <mmdeviceapi.h>
    #include <endpointvolume.h>
#endif

#ifdef Q_OS_OSX
    #include <CoreAudio/CoreAudio.h>
    #include <AudioToolbox/AudioServices.h>
#endif

#ifdef Q_OS_LINUX
    #include <alsa/asoundlib.h>
#endif

#include "mse/bass/bass.h"

#define MSE_TRANSMITTER_METAMULTIPLIER 16
#define MSE_TRANSMITTER_MAXMETALENGTH (MSE_TRANSMITTER_METAMULTIPLIER * 255)
#define MSE_TRANSMITTER_MAXTITLELENGTH (MSE_TRANSMITTER_MAXMETALENGTH - 255)
#define MSE_TRANSMITTER_MAXCLIENTREQUESTLENGTH 65535
#define MSE_LIBVERSION_MAJOR 2
#define MSE_LIBVERSION_MINOR 4

#if BASSVERSION	!= MSE_LIBVERSION_MAJOR*256+MSE_LIBVERSION_MINOR
    #error \
The current version of MesonSoundEngine library \
is incompatible with an included BASS library header
#endif

class MSE_Engine;
class MSE_Sound;
class MSE_Playlist;
class MSE_Source;

/*!
 * Version information with a string formatter routines.
 */
struct MSE_VersionInfo {
    quint8 build; /*!< Build version number (x.x.x.<b>x</b>).*/
    quint8 revision; /*!< Revision version number (x.x.<b>x</b>.x). */
    quint8 minor; /*!< Minor version number (x.<b>x</b>.x.x). */
    quint8 major; /*!< Major version number (<b>x</b>.x.x.x). */
    /*!
     * Initializes the structure from a DWORD.
     *
     * \example For example, 0x02040103 (hex), would be version 2.4.1.3
     */
    inline void setDword(DWORD x)
    {
        memcpy(reinterpret_cast<DWORD*>(this), &x, sizeof(x));
    }

    /*!
     * Returns a string reprentation including major and minor version numbers (i.e. "3.2").
     */
    inline QString asShortString() const
    {
        return QString::number(major)+"."+
               QString::number(minor);
    }

    /*!
     * Returns a full string reprentation of a version information (i.e. "3.2.41.89").
     */
    inline QString asString() const
    {
        return asShortString()+"."+
               QString::number(revision)+"."+
               QString::number(build);
    }
};

/*!
 * Information about a single format supported by a plugin
 */
struct MSE_EnginePluginFormat {
    QString description; /*!< Format description. */
    QStringList extensions; /*!< List of supported file extensions for this format (without a leading dot). */
};

/*!
 * Plugin information
 */
struct MSE_EnginePluginInfo {
    QString filename; /*!< Full canonical path to a plugin file. */
    MSE_VersionInfo version; /*! Version information. */
    QList<MSE_EnginePluginFormat> formats; /*! Supported formats. */
};

/*!
 * The type of sound samples
 */
enum MSE_SoundSampleType {
    mse_sstNormal, /*!< Use 16-bit resolution. */
    mse_sst8Bits, /*!< Use 8-bit resolution. */
    mse_sstFloat32 /*!< Use 32-bit floating-point sample data. */
} ;

/*!
 * Sample interpolation mode.
 */
enum MSE_SoundSampleInterpolation {
    mse_ssiLinear, /*!<
    Linear interpolation.
*/
    mse_ssiNone, /*!<
    Use non-interpolated sample mixing.
    This generally reduces the sound quality, but can be good for chip-tunes.
*/
    mse_ssiSinc /*!<
    Use sinc interpolated sample mixing.
    This increases the sound quality, but also requires more processing.
*/
};

/*!
 * Sample ramping mode.
 */
enum MSE_SoundSampleRamping {
    mse_ssrNone, /*!<
    No ramping is applied.
*/
    mse_ssrNormal, /*!<
    Smoothen volume/pan changes to avoid "clicks".
*/
    mse_ssrSensitive /*!<
    Same as mse_ssrNormal, but will only ramp the start of a sample
    if it thinks that it would "click" otherwise.
    This keeps percussive sounds sharp,
    whereas normal ramping can slightly deaden their impact.
*/
};

/*!
 * Surround mode.
 */
enum MSE_SoundSurroundMode {
    mse_ssmNone, /*!<
    Do not apply surround effects
*/
    mse_ssmMode1, /*!<
    Apply XMPlay's surround sound to the music.
*/
    mse_ssmMode2 /*!<
    Same as mse_ssmMode1, but ignores panning,
    which is useful if the music's channels are all centered.
*/
};

/*!
 * Tracker emulation mode.
 */
enum MSE_SoundTrackerEmulation {
    mse_steNone, /*!< Play .MOD file as usual. */
    mse_steFastTracker2, /*!< Play .MOD file as FastTracker 2 would. */
    mse_steProTracker1 /*!< Play .MOD file as ProTracker 1 would. */
};

/*!
 * Parameters for MSE_Sound initialization.
 */
struct MSE_SoundInitParams {
    MSE_SoundSampleType sampleType = mse_sstFloat32; /*!<
    Sets the type of samples.

    **Default**: ::mse_sstFloat32 if <tt>MSE_EngineInitParams.use8Bits == false</tt>, otherwise - ::mse_sst8Bits;

    \note If you set <tt>MSE_EngineInitParams.use8Bits = true</tt> when initializing a sound engine,
    then you must set this param to ::mse_sst8Bits.

    \sa MSE_SoundSampleType
*/
    bool useSoftware = false; /*!<
    Use software for sound processing, else use hardware.

    **Default**: false
*/
    bool use3D = false; /*!<
    Enable support for 3D effects (EAX).

    **Default**: false
*/
    bool useOldFx = false; /*!<
    Enable the old implementation of DirectX 8 effects.

    **When enabled**.
    This is the standard way of using DX8 effects.
    The main advantage of this method is that effect parameter changes are audible instantaneously.
    The main disadvantages are that the channel's sample rate cannot be changed (can with DX9),
    and it cannot be used with decoding channels or speaker assignment.

    **When disabled**.
    The advantages/disadvantages of this method are basically the opposite of the other method;
    the channel's sample rate can be changed, but there's a delay in effect parameter changes being audible.
    The reason being that, using this method, the effects are applied at the same stage as user DSP functions.

    **Default**: false
*/
    bool doPrescan = false; /*!<
    Enable pin-point accurate seeking (to the exact byte) on the MP3/MP2/MP1 stream.
    This also increases the time taken to create the stream,
    due to the entire file being pre-scanned for the seek points.

    **Default**: false
*/
    bool decodeOnly = false; /*!<
    Mix the sample data, without playing it. Needed if the stream will be used in MSE_Mixer.

    **Default**: false
*/
    MSE_SoundSampleInterpolation sampleInterpolation = mse_ssiSinc; /*!<
    Sets a sample interpolation mode. Only applies to tracker music.

    **Default**: ::mse_ssiSinc

    \sa MSE_SoundSampleInterpolation
*/
    MSE_SoundSampleRamping sampleRamping = mse_ssrSensitive; /*!<
    Sets a sample ramping mode. Only applies to tracker music.

    **Default**: ::mse_ssrSensitive

    \sa MSE_SoundSampleRamping
*/
    MSE_SoundSurroundMode surroundMode = mse_ssmNone; /*!<
    Set surround mode. Only applies to tracker music.

    **Default**: ::mse_ssmNone

    \note This param will be set to ::mse_ssmNone
    if you set <tt>#useMono = true</tt>.

    \sa MSE_SoundSurroundMode
*/
    MSE_SoundTrackerEmulation trackerEmulation = mse_steNone; /*!<
    Sets tracker emulation mode. Only applies to tracker music.

    **Default**: ::mse_steNone

    \sa MSE_SoundTrackerEmulation
*/
    bool enableDSP = false; /*!<
    Enables MSE_Sound::onDSP signal.

    **Default**: false
*/
    int sincPoints = 32; /*!<
    When a channel has a different sample rate to what the output device is using,
    the channel's sample data will need to be converted to match the output device's rate during playback.
    This parameter determines how many points will be used for a sample rate conversion.

    Set this param to zero to use linear interpolation.
    The linear interpolation option uses less CPU,
    but the sinc interpolation gives better sound quality (less aliasing),
    with the quality and CPU usage increasing with the number of points.

    **Valid values**: 0, 8, 16, 32

    **Default**: 32
*/

    bool useICU = false; /*!<
    Use ICU library to detect tag encodings if they're not UTF.
    If false, then fallback to ISO-8859-1 encoding.

    WARNING: may produce incorrect tags!

    **Default**: false
*/

    int icuMinConfidence = 0; /*!<
    Only try to convert the tag value to UTF-8
    if ICU has the confidence score at least this big.

    **Valid values**: 0 - 100

    **Default**: 0
*/
};

/*!
 * This enum determines how the sound sources should be loaded
 * when specifying a source containing another sources
 * (e. g. directory or playlist)
 */
enum MSE_SourceLoadFlag {
    mse_slfDefault = 0x0, /*!<
    Do not apply any special rules
*/
    mse_slfRecurseSubdirs = 0x1, /*!<
    Load all underlying directories recursively
*/
    mse_slfLoadPlaylists = 0x2, /*!<
    Parse all encountered playlists
*/
    mse_slfSkipDirs = 0x4, /*!<
    Skip directories when loading files from a playlist.
*/
    mse_slfSkipPlaylists = 0x8 /*!<
    Skip playlists when loading files from a playlist.
*/
};

/*!
 * \class MSE_SourceLoadFlags
 * A combination of MSE_SourceLoadFlag flags.
 */
Q_DECLARE_FLAGS(MSE_SourceLoadFlags, MSE_SourceLoadFlag)

/*!
 * Playlist playback behavior.
 */
enum MSE_PlaylistPlaybackMode {
    mse_ppmTrackOnce, /*!<
    When a current track ends, stop the playback
    (do not move to a next track in a playlist)
*/
    mse_ppmTrackLoop, /*!<
    When a current track ends, play that track once more
    (indefinetely loop a current track)
*/
    mse_ppmRandom, /*!<
    When a current track ends, play a random track from a playlist
    (the playlist won't stop after it plays all tracks)
*/
    mse_ppmAllOnce, /*!<
    When a current track ends, play the next one
    unless there are no tracks in playlist left
*/
    mse_ppmAllLoop /*!<
    When a current track ends, play the next one.
    If a current track was the last one in a playlist,
    then play a first track.
    (indefinetely loop a playlist)
*/
} ;

/*!
 * The type of the sound channel.
 */
enum MSE_SoundChannelType {
    mse_sctUnknown, /*!<
    The type of the sound cannot be/was not determined
*/
    mse_sctStream, /*!<
    BASS supported local stream file (e. g. *.mp3, *.ogg, ...)
*/
    mse_sctModule, /*!<
    BASS supported local module/tracker file (e. g. *.mod, *.xm, ...)
*/
    mse_sctRemote, /*!<
    BASS/plugin supported remote stream
*/
    mse_sctRecord, /*!<
    The channel is used to record, not to play sound
*/
    mse_sctPlugin /*!<
    Channel decoding is handled by one of loaded plugins
*/
};

/*!
 * The current state of the sound channel.
 */
enum MSE_SoundChannelState {
    mse_scsIdle, /*!< The channel is stopped */
    mse_scsPlaying, /*!< The channel is played */
    mse_scsPaused /*!< The channel is paused */
};

/*!
 * Process priority for MSE_Encoder
 */
enum MSE_EncoderProcessPriority {
    mse_eppLowest, /*!< Lowest priority */
    mse_eppLower, /*!< Lower than normal priority */
    mse_eppNormal, /*!< Normal priority */
    mse_eppHigher, /*!< Higher than normal priority */
    mse_eppHighest /*!< Highest priority */
};

/*!
 * Parameters for MSE_Encoder initialization.
 *
 * \note The actual encoder might not be able to support some of the parameters.
 * Refer to the documentation of the used encoder class for details.
 */
struct MSE_EncoderInitParams {
    quint16 outputBitrate; /*!<
    Output bitrate in kbps.
*/
    quint32 outputFrequency; /*!<
    Output sample rate in Hz.
    To eleminate possible resampling, this value should be equal to
    MSE_Mixer::outputFrequency from the corresponding mixer object (default).

    **Valid values**: depends on encoder

    **Default**: MSE_Mixer::outputFrequency
    (from the MSE_Mixer object which is used in constructor)
*/
    MSE_EncoderProcessPriority processPriority; /*!<
    Encoder process priority.

    **Default**: ::mse_eppNormal

    \sa MSE_EncoderProcessPriority
*/
    quint8 outputQuality; /*!<
    Encoding quality in percents.

    **Valid values**: 0..100

    **Default**: 100
*/
    quint8 chunkDuration; /*!<
    Number of seconds for encoder to process at a time.

    **Valid values**: 1..255

    **Default**: 5
*/
};

/*!
 * Parameters for MSE_Encoder initialization.
 */
struct MSE_TransmitterInitParams {
    quint8 pollInterval; /*!<
    Polling interval in seconds after which the encoder will process each new chunk.
    Discarded when MSE_TransmitterInitParams::useAccurateSendIntervals = true.

    **Valid values**: 1..255

    **Default**: Default value is calculated from a corresponding MSE_Encoder object
    as MSE_EncoderInitParams::chunkDuration minus 1
*/
    quint16 port; /*!<
    Station port. You must specify different ports for a different stations.

    **Valid values**: any valid port number in range of 1..65535

    **Default**: 8000
*/
    quint16 maxListeners; /*!<
    Maximum number of simultaneously connected clients allowed.
    Set to zero for no limit.

    **Valid values**: 0..65535

    **Default**: 0
*/
    qint64 dataBlockLength; /*!<
    Number of bytes in each ShoutCast data block (icy-metaint).

    **Valid values**: any positive integer (preferably 2^N)

    **Default**: 16384
*/
    quint8 bufferLength; /*!<
    Internal output buffer in seconds.

    **Valid values**: 1..255

    **Default**: 15
*/
    QString name; /*!<
    Station name. This will appear in icy-name header.

    **Valid values**: any non-empty string

    **Default**: Unnamed
*/
    QString genre; /*!<
    Station music genre. This will appear in icy-genre header.

    **Valid values**: any string

    **Default**: Unknown
*/
    bool isPublic; /*!<
    This will appear in icy-pub response (1 if true, 0 otherwise).
    This param doesn't mean anything special in the current server implementation.

    **Default**: true
*/
    QString url; /*!<
    Station's URL. This will appear in icy-url header.

    **Default**: \<empty\>
*/
    QString irc; /*!<
    IRC contact number. This will appear in icy-irc header.

    **Default**: \<empty\>
*/
    QString icq; /*!<
    ICQ contact number. This will appear in icy-icq header.

    **Default**: \<empty\>
*/
    QString aim; /*!<
    AIM contact number. This will appear in icy-aim header.

    **Default**: \<empty\>
*/
    QString notice1; /*!<
    This will appear in icy-notice1 header.

    **Default**: \<empty\>

*/
    QString notice2; /*!<
    This will appear in icy-notice2 header.

    **Default**: \<empty\>

*/
    bool useAccurateSendIntervals; /*!<
    By default, MSE_Transmitter sends data to clients using approximate intervals of time
    based on bitrate and other parameters.
    In other words, the tranmission speed from server not exactly equals
    to a playback speed on client side.

    Such behavior can lead to buffer underflow/overflow on client side and,
    as a result, playback corruption.

    When MSE_TransmitterInitParams::useAccurateSendIntervals = true, then MSE_Transmitter will use upload speed
    that exactly matches the playback speed of the audio stream,
    minimizing chances of above mentioned client-side problems.

    To use this option, you must set initialize MSE_Engine with
    non-zero MSE_EngineInitParams::device value.

    **Default**: false

    \sa MSE_Transmitter, MSE_EngineInitParams
*/
    QString titleFormat; /*!<
    Title format for a current track.

    \li **%artist%** will be replaced with an artist's name;
    \li **%title%** will be replaced with a track title;
    \li **%full_title%** will be replaced with "%artist% - %title%"
    if artist's title is present,
    otherwise it will be replaced with %title%.

    If a title cannot be determined
    then it will be the current filename without an extension.

    **Default**: %full_title%
*/
    QString controlPassword; /*!<
    If set, then you can control the station via HTTP requests.

    **Default**: \<empty\>
*/
    bool enableMixing; /*!<
    To be documented. Do not use!

    **Default**: false
*/
};

/*!
 * Playlist format
 */
enum MSE_PlaylistFormatType {
    mse_pftUnknown, /*!< undefined/unsupported */
    mse_pftASX, /*!< [Advanced Stream Rediretor](https://en.wikipedia.org/wiki/Advanced_Stream_Redirector) */
    mse_pftM3U, /*!< [M3U](https://en.wikipedia.org/wiki/M3U) (in UTF-8) */
    mse_pftXSPF, /*!< [XML Shareable Playlist Format](https://en.wikipedia.org/wiki/XML_Shareable_Playlist_Format) */
    mse_pftPLS, /*!< [PLS](https://en.wikipedia.org/wiki/PLS_\(file_format\)) */
    mse_pftWPL, /*!< [Windows Media Player Playlist](https://en.wikipedia.org/wiki/Windows_Media_Player_Playlist) */
    mse_pftCUE /*!< [CUE Sheet](https://en.wikipedia.org/wiki/Cue_sheet_\(computing\)) */
};

/*!
 * ID3v2.x tag header structure.
 * More info at http://id3.org/id3v2.4.0-structure.
 */
struct MSE_TagInfoID3v2Header {
    quint8 id[3]; /*!< Tag identification. Must contain 'ID3' if tag exists and is correct. */
    quint8 version; /*!< Major version number. */
    quint8 revision; /*!< Revision version number. */
    quint8 flags; /*!< Tag flags. */
    quint8 size[4]; /*!< Tag size in big endian syncsafe integer (only last 7 bits of each byte are effective). */

    /*!
     * Returns tag size in bytes
     */
    quint32 byteSize() const
    {
        return ((size[0] & 0b1111111) << 21)
             + ((size[1] & 0b1111111) << 14)
             + ((size[2] & 0b1111111) <<  7)
             + ((size[3] & 0b1111111) <<  0);
    }
};


/*!
 * ID3 v2.3 and v2.4 tag structure.
 * More info at https://id3.org/d3v2.3.0, http://id3.org/id3v2.4.0-structure and http://id3.org/id3v2.4.0-frames.
 */
struct MSE_TagInfoID3v2 {
    char name[4]; /*!< Tag name (e.g. TIT1, TPE1, ...). */
    quint8 len[4]; /*!<
    Tag length.

    version | format
    --------|-------
     2.3    | 4-byte big endian integer.
     2.4    | 4-byte big endian syncsafe integer (only last 7 bits of each byte are effective).
*/
    quint8 flags[2]; /*!< Tag flags. */
    quint8 encoding; /*!<
    Tag encoding.

    value | encoding
    ------|---------
     $00  | ISO-8859-1. Terminated with $00.
     $01  | UTF-16 encoded Unicode with BOM.
     $03  | UTF-16BE encoded Unicode without BOM (only v2.4).
     $04  | UTF-8 encoded Unicode. Terminated with $00 (only v2.4).
*/

    /*!
     * Returns tag size in bytes
     */
    quint32 byteSize(const quint8 version) const
    {
        return (version == 3)
             ? (len[0] << 24)
             + (len[1] << 16)
             + (len[2] << 8)
             + len[3]
             : ((len[0] & 0b1111111) << 21)
             + ((len[1] & 0b1111111) << 14)
             + ((len[2] & 0b1111111) <<  7)
             + ((len[3] & 0b1111111) <<  0);
    }
};

/*!
 * ID3 v2.2 tag structure.
 * More info at: http://id3.org/id3v2-00
 */
struct MSE_TagInfoID3v22 {
    char name[3]; /*!< Tag name (e.g. TT2, TP1, ...). */
    quint8 len[3]; /*!< Tag length in 3-byte big-endian integer */
    quint8 encoding; /*!<
    Tag encoding.

    value | encoding
    ------|---------
     $00  | ISO-8859-1. Terminated with $00.
     $01  | UCS-2 encoded Unicode with BOM.
*/

    /*!
     * Returns tag value size in bytes
     */
    quint32 byteSize() const
    {
        return (len[0]<<16) + (len[1]<<8) + len[2];
    }
};

#ifndef BASS_TAG_WMA
    #define BASS_TAG_WMA 8
#endif
