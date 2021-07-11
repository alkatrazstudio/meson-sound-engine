/****************************************************************************}
{ engine.h - sound engine core                                               }
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

#include "mse/object.h"
#include "mse/sound.h"

#ifdef QT_NETWORK_LIB
    #include <QtNetwork/QNetworkProxy>
#endif

/*!
 * Parameters for MSE_Engine initialization.
 */
struct MSE_EngineInitParams {
    quint32 outputFrequency = 44100; /*!<
    Output sample rate in Hz. Only for Linux.

    **Valid values**: 192000, 96000, 48000, 44100, 22050, 16000, 11025, 8000

    **Default**: 44100
*/
    bool use8Bits = false; /*!<
    Use 8-bit resolution, else 16-bit.

    **Default**: false
*/
    int nChannels = 2; /*!<
    Number of channels.

    **Default**: 2
*/
    bool use3D = false; /*!<
    Enable support for 3D effects (EAX).

    **Default**: false
*/
    int device = -1; /*!<
    Use a specific device for a sound output.

    **Valid values**:

    value | description
    ------|------------
    -1 | default device
    0 | no sound
    1 | first real output device
    2 | second real output device
    ... | ...
    N | real output device \#N

    **Default**: -1
*/
    int recordingDevice = -2; /*!<
    Use a specific device for a sound recording.

    **Valid values**:

    value | description
    ------|------------
    -2    | do not use a recording device
    -1    | default device
    0     | first real recording device
    1     | second real recording device
    ...   | ...
    N | real recording device \#(N-1)

    **Default**: -2
*/
    bool useDefaultDevice = true; /*!<
    Automatically switch the default output to the new default device whenever it changes.
    Only works on Windows.

    **Default**: true
*/

#ifdef QT_NETWORK_LIB
    QNetworkProxy proxy; /*!<
    Use a proxy for all network requests.

    **Default**: &lt;application proxy&gt;

    \note If you set application-wise proxy,
    then some network requests will be proxied regardless of useAppProxy.
*/
#endif

    QString userAgent; /*!<
    UA string used in all network requests.

    **Default**: &lt;empty&gt;

    \note If MSE_Engine is initialized with an empty userAgent,
    then the result of MSE_Engine::getDefaultUA() will be used as a UA string.
*/
    int updatePeriod = 100; /*!<
    The amount of time (in milliseconds) between updates of the playback buffers.

    **Valid values**: 5..100, but not greater than MSE_EngineInitParams::bufferLength.

    **Default**: 100
*/
    int bufferLength = 500;/*!<
    The playback buffer length in milliseconds.

    **Valid values**: 10..5000, but not lower than MSE_EngineInitParams::updatePeriod.

    **Default**: 500
*/
    int updateThreads = -1;/*!<
    How many channel playback buffers can be updated in parallel.

    **Valid values**: any positive integer or -1 for a system default.

    **Default**: -1
*/
};

class MSE_Engine : public MSE_Object
{
    Q_OBJECT

public:
    static MSE_Engine* getInstance();
    ~MSE_Engine();

    bool init(const MSE_EngineInitParams &params = MSE_EngineInitParams());

    /*!
     * Returns BASS library version information.
     */
    inline const MSE_VersionInfo& getLibVersion() const {return libVersion;}

    /*!
     * Returns true if floating point samples are supported.
     */
    inline bool getIsFloatSupported() const {return isFloatSupported;}

    /*!
     * Returns true if a 3D sound positioning and effects are supported.
     */
    inline bool getIs3DSupported() const {return is3DSupported;}

    /*!
     * Returns parameters the engine was initialized with.
     *
     * \note These parameters can be slightly different to those passed to init() function.
     */
    inline const MSE_EngineInitParams& getInitParams() const {return initParams;}

    static float snapVolumeToGrid(float val, float gridStep);

    /*!
     * Returns a current volume for a MSE system in range [0;1].
     *
     * \note If a sound level was changed somewhere outside MSE,
     * then this function still return the same value.
     * Use refreshVolume() to update volume information.
     */
    inline float getVolume() const {return volume;}

    bool setVolume(float value);

    /*!
     * Updates an information about a current MSE system volume.
     * Use it in cases when a MSE volume could be changed by some external function.
     */
    inline void refreshVolume(){volume = BASS_GetVolume();}

    bool changeVolume(float diff, bool snapToGrid = false);

    /*!
     * Returns true, if a master sound volume can be changed via MSE functions.
     *
     * \sa getMasterVolume, setMasterVolume, changeMasterVolume
     */
    inline bool isMasterVolumeAvailable() const {return masterVolumeAvailable;}

    float getMasterVolume();
    bool setMasterVolume(float val);
    bool changeMasterVolume(float diff, bool snapToGrid = false);

    bool loadPlugin(const QString& filename);
    bool loadPluginsFromDirectory(const QString& dirname);
    bool unloadPlugin(int index);
    bool unloadAllPlugins();

    bool unzipFile(const QString &filename, QByteArray& arr) const;

    MSE_SoundChannelType typeByUri(const QString &filename) const;

    /*!
     * Returns a number of loaded plugins.
     *
     * \sa getPluginInfo
     */
    inline int getPluginsCount() const {return plugins.size();}

    /*!
     * Returns a plugin information.
     */
    inline const MSE_EnginePluginInfo& getPluginInfo(int index) const {return plugins.at(index);}

    static int getRealOutputDeviceIndex();

    static QString getDefaultUA(const QString& appName = "", const QString& appVersion = "");

protected:
    MSE_VersionInfo libVersion; /*!< BASS library version information. */
    bool isFloatSupported; /*!< True, if floating point samples are supported. */
    bool is3DSupported; /*!< True, if 3D functionality can be applied to streams. */
    MSE_EngineInitParams initParams; /*!< Initialization parameters. */
    QList<MSE_EnginePluginInfo> plugins; /*!< Information about loaded plugin. */
    QList<HPLUGIN> pluginHandles; /*!< List of plugin handles. It matches plugins list. */
    float volume; /*!< Current MSE volume in range [0;1]. */
    QByteArray uaString; /*!< UA string in UTF-8. */

    bool masterVolumeAvailable; /*!< True if OS master volume can be controlled by MSE. */
#ifdef Q_OS_WIN
    bool mvCoInited;
    GUID mvGuid;
    IAudioEndpointVolume *mvEndpoint;
    IMMDeviceEnumerator *mvEnumerator;
    IMMDevice *mvDevice;
#endif
#ifdef Q_OS_OSX
    AudioDeviceID mvDevice;
    AudioObjectPropertyAddress mvMasterProp;
#endif
#ifdef Q_OS_LINUX
    float mvRange;
    long mvMin;
    bool mvHasSwitch;
    snd_mixer_selem_id_t* mvSelemId;
    snd_mixer_t* mvMixerHandle;
    snd_mixer_elem_t* mvMixerElem;
#endif

    explicit MSE_Engine(QObject *parent = 0);
    bool postInit();
    bool checkForFeature(DWORD flags, const MSE_EngineInitParams &params) const;
    bool initMasterVolumeControl();
};
