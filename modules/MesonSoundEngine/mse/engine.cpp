/****************************************************************************}
{ engine.cpp - sound engine core                                             }
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

#include "mse/engine.h"
#include "mse/sound.h"

#include "coreapp.h"

#if __has_include(<quazip5/quazipfile.h>)
    #include <quazip5/quazipfile.h>
#else
    #include <quazip/quazipfile.h>
#endif

static MSE_Engine* instance = nullptr;

/*!
 * Constructs MSE_Engine instance.
 *
 * MSE_Engine is a singleton class and cannot be created directly via this constructor.
 * You should call getInstance to create a MSE engine.
 */
MSE_Engine::MSE_Engine(QObject *parent) : MSE_Object(parent)
{
#ifdef Q_OS_WIN
    mvCoInited = false;
#endif
#ifdef Q_OS_LINUX
    mvMixerHandle = nullptr;
    mvSelemId = nullptr;
#endif
}

/*!
 * Returns a global instance of MSE_Engine class.
 * If an engine wasn't created yet, this function calls the constructor, then returns an instance.
 */
MSE_Engine* MSE_Engine::getInstance()
{
    if(!instance)
        instance = new MSE_Engine();
    return instance;
}

/*!
 * Destroys the sound engine instance and unloads all loaded plugins.
 *
 * \note All other instances of MSE_* classes will be invalid.
 *
 * \sa loadPlugin, unloadAllPlugins
 */
MSE_Engine::~MSE_Engine()
{
    unloadAllPlugins();
#ifdef Q_OS_WIN
    if(mvCoInited)
        CoUninitialize();
#endif
#ifdef Q_OS_LINUX
    if(mvMixerHandle)
        snd_mixer_close(mvMixerHandle);
#endif
    instance = nullptr;
}

/*!
 * Initializes the engine.
 *
 * If params not specified, then the object will be initialized with a default parameters.
 *
 * Refer to MSE_EngineInitParams for the information about initialization parameters.
 *
 * \sa getDefaultInitParams
 */
bool MSE_Engine::init(const MSE_EngineInitParams& params)
{
    initParams = params;

    if(initParams.userAgent.isEmpty())
        initParams.userAgent = getDefaultUA();
    else
        initParams.userAgent = initParams.userAgent.simplified();

    if(!postInit())
        return false;

    initParams.useDefaultDevice = (BASS_GetConfig(BASS_CONFIG_DEV_DEFAULT) != 0);
    refreshVolume();
    masterVolumeAvailable = initMasterVolumeControl();
    return true;
}

/*!
 * Snap a given sound volume value to a grid with a *gridStep* step.
 * If the volume is greater than or equals the maximum then return this maximum.
 * If *gridStep* equals zero then return *val* unmodified.
 * Negative *gridStep* is allowed and is the same as a positive one.
 */
float MSE_Engine::snapVolumeToGrid(float val, float gridStep)
{
    if(val <= 0)
        return 0;
    if(val >= 1)
        return 1;
    if(gridStep == 0)
        return val;
    return gridStep*qRound(val/gridStep);
}

/*!
 * Actual initialization.
 *
 * Do not call this directly. Use init function instead.
 *
 * \sa init
 */
bool MSE_Engine::postInit()
{
    libVersion.setDword(BASS_GetVersion());
    CHECK(
        (libVersion.major == MSE_LIBVERSION_MAJOR)
            &&
        (libVersion.minor == MSE_LIBVERSION_MINOR),
        MSE_Object::Err::invalidVersion
    );
    if(initParams.useDefaultDevice)
        BASS_SetConfig(BASS_CONFIG_DEV_DEFAULT, 1);
    DWORD flags = BASS_DEVICE_LATENCY | BASS_DEVICE_FREQ;
    if(initParams.use3D)
        flags |= BASS_DEVICE_3D;
    if(initParams.use8Bits)
        flags |= BASS_DEVICE_8BITS;
    if(initParams.nChannels == 1)
        flags |= BASS_DEVICE_MONO;

    //BASS_SetConfig(BASS_CONFIG_FLOATDSP, 1);

    CHECK(BASS_Init(initParams.device, initParams.outputFrequency, flags, nullptr, nullptr), MSE_Object::Err::initFail);

    if(initParams.recordingDevice >= -1)
        CHECK(!BASS_RecordInit(initParams.recordingDevice), MSE_Object::Err::recordInitFail);

    isFloatSupported = checkForFeature(BASS_SAMPLE_FLOAT, initParams);
    is3DSupported = checkForFeature(BASS_DEVICE_3D, initParams);

    uaString = initParams.userAgent.toUtf8();
    BASS_SetConfigPtr(BASS_CONFIG_NET_AGENT, uaString.constData());

    BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, initParams.updatePeriod);
    BASS_SetConfig(BASS_CONFIG_BUFFER, initParams.bufferLength);
    int nThreads = initParams.updateThreads;
    if(nThreads < 0)
        nThreads = QThread::idealThreadCount();
    if(nThreads <= 0)
        nThreads = 1;
    BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, nThreads);

    return true;
}

/*!
 * Check whether a certain flag combination is acceptable for creating a sound stream.
 */
bool MSE_Engine::checkForFeature(DWORD flags, const MSE_EngineInitParams &params) const
{
    HCHANNEL channel = BASS_StreamCreate(params.outputFrequency, params.nChannels, flags, nullptr, nullptr);
    if(!channel)
        return false;
    BASS_StreamFree(channel);
    return true;
}

/*!
 * Tries to initialize the master volume controls.
 *
 * Returns true on success.
 * Then it will mean that the global volume level can be controlled via MSE.
 */
bool MSE_Engine::initMasterVolumeControl()
{
#ifdef Q_OS_WIN
    CHECK(SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)), MSE_Object::Err::cannotInitializeCOM);
    mvCoInited = true;
    CHECK(SUCCEEDED(CoCreateGuid(&mvGuid)), MSE_Object::Err::unableCreateGuid);
    CHECK(SUCCEEDED(
        CoCreateInstance(
                  __uuidof(MMDeviceEnumerator),
                  nullptr, CLSCTX_INPROC_SERVER,
                  __uuidof(IMMDeviceEnumerator),
                  (void**)&mvEnumerator)
              ), MSE_Object::Err::unableGetEnumerator);
    CHECK(SUCCEEDED(mvEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &mvDevice)), MSE_Object::Err::unableGetEndpoint);
    CHECK(SUCCEEDED(
              mvDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&mvEndpoint)
              ), MSE_Object::Err::unableActivateEndpoint);
    return true;
#endif
#ifdef Q_OS_OSX
    UInt32 devIdSize = sizeof(mvDevice);
    AudioObjectPropertyAddress defDevProp = {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMaster
    };
    CHECK(noErr == AudioObjectGetPropertyData(kAudioObjectSystemObject, &defDevProp, 0, nullptr, &devIdSize, &mvDevice), MSE_Object::Err::unableGetEndpoint);

    Float32 vol;
    UInt32 volPropSize = sizeof(Float32);
    mvMasterProp = {
        .mSelector = kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
        .mScope = kAudioDevicePropertyScopeOutput,
        .mElement = kAudioObjectPropertyElementMaster
    };
    CHECK(noErr == AudioObjectGetPropertyData(mvDevice, &mvMasterProp, 0, nullptr, &volPropSize, &vol), MSE_Object::Err::unableFindChannelController);

    return true;
#endif
#ifdef Q_OS_LINUX
    long mvMax;
    const char* card = "default";
    const char* selemName = "Master";

    CHECK(!snd_mixer_open(&mvMixerHandle, 0), MSE_Object::Err::openMixer);
    CHECK(!snd_mixer_attach(mvMixerHandle, card), MSE_Object::Err::mixerAttach);
    CHECK(!snd_mixer_selem_register(mvMixerHandle, nullptr, nullptr), MSE_Object::Err::registerMixerElement);
    CHECK(!snd_mixer_load(mvMixerHandle), MSE_Object::Err::loadMixer);

    snd_mixer_selem_id_alloca(&mvSelemId);
    snd_mixer_selem_id_set_index(mvSelemId, 0);
    snd_mixer_selem_id_set_name(mvSelemId, selemName);

    CHECK(mvMixerElem = snd_mixer_find_selem(mvMixerHandle, mvSelemId), MSE_Object::Err::masterVolumeElementNotFound);

    CHECK(!snd_mixer_selem_get_playback_volume_range(mvMixerElem, &mvMin, &mvMax), MSE_Object::Err::masterVolumeRange);
    mvRange = mvMax - mvMin;
    mvHasSwitch = snd_mixer_selem_has_playback_switch(mvMixerElem);

    return true;
#endif
    return false;
}


/*!
 * Sets the volume for a whole MSE system.
 * Valid values are in range [0;1].
 * Returns false if a sound level cannot be set.
 */
bool MSE_Engine::setVolume(float value)
{
    if(value < 0)
        value = 0;
    else
    {
        if(value > 1)
            value = 1;
    }
    bool result = BASS_SetVolume(value);
    if(result)
        volume = value;
    return result;
}

/*!
 * Loads a BASS-compatible plugin. Returns true on success.
 *
 * \note OSX libraries must be located in the application directory.
 */
bool MSE_Engine::loadPlugin(const QString &filename)
{
    CHECK(QFile::exists(filename), MSE_Object::Err::pathNotFound, filename);
    QFileInfo finfo;
    finfo.setFile(filename);
    QString fullFilename = finfo.canonicalFilePath();
    CHECK(!fullFilename.isEmpty(), MSE_Object::Err::cannotGetCanonicalPath, filename);

    HPLUGIN plug;

#ifdef Q_OS_WIN
    plug = BASS_PluginLoad((char*)(fullFilename.utf16()), BASS_UNICODE);
#else
    plug = BASS_PluginLoad(fullFilename.toUtf8().constData(), 0);
#endif

    if(!plug)
    {
        int err = BASS_ErrorGetCode();
        switch(err)
        {
            case BASS_ERROR_FILEOPEN:
                SETERROR(MSE_Object::Err::openFail, filename);
                break;

            case BASS_ERROR_FILEFORM:
                SETERROR(MSE_Object::Err::invalidFormat, filename);
                break;

            case BASS_ERROR_VERSION:
                SETERROR(MSE_Object::Err::alreadyDone, filename);
                break;

            default:
                SETERROR(MSE_Object::Err::unknown, filename);
        }
        return false;
    }

    const BASS_PLUGININFO* plugInfo = BASS_PluginGetInfo(plug);
    if(!plugInfo)
    {
        BASS_PluginFree(plug);
        SETERROR(MSE_Object::Err::cannotFetchPluginInfo, filename);
        return false;
    }

    MSE_EnginePluginFormat fmt;
    MSE_EnginePluginInfo info;
    const BASS_PLUGINFORM* format;
    QString exts;
    QStringList extList;

    info.filename = fullFilename;
    info.version.setDword(plugInfo->version);
    quint32 n = plugInfo->formatc;

    for(quint32 a=0; a<n; a++)
    {
        format = &plugInfo->formats[a];
        fmt.description = QString::fromUtf8(format->name);
        exts = QString::fromUtf8(format->exts);
        extList = exts.split(";", QString::SkipEmptyParts);
        fmt.extensions.clear();
        foreach(QString ext, extList)
            fmt.extensions.append(ext.mid(2).toLower()); // don't include "*." part
        info.formats.append(fmt);
    }

    pluginHandles.append(plug);
    plugins.append(info);

    return true;
}

/*!
 * Loads all BASS-compatible plugins from a specified directory.
 * Loading order is undefined.
 *
 * Plugins to be loaded must be properly named:
 *
 *  OS      | filename mask
 * ---------|-----------------
 *  Windows | bass?*.dll
 *  Linux   | libbass?*.so
 *  OSX     | libbass?*.dylib
 *
 * Returns true if ALL plugins were successfully loaded.
 *
 * \note OSX libraries must be located in the application directory.
 */
bool MSE_Engine::loadPluginsFromDirectory(const QString &dirname)
{
    QDir dir;
    dir.setPath(dirname);
    CHECK(dir.exists(), MSE_Object::Err::pathNotFound, dirname)
    QString fullDirname = dir.canonicalPath();
    CHECK(!fullDirname.isEmpty(), MSE_Object::Err::cannotGetCanonicalPath, dirname)
    fullDirname += "/";
    QStringList nameFilters;
#ifdef Q_OS_WIN
    nameFilters.append("bass?*.dll");
#endif
#ifdef Q_OS_LINUX
    nameFilters.append("libbass?*.so");
#endif
#ifdef Q_OS_OSX
    nameFilters.append("libbass?*.dylib");
#endif
    QStringList entries = dir.entryList(nameFilters, QDir::Files, {});
    bool ok = true;
    foreach(QString entry, entries)
        if(!loadPlugin(fullDirname + entry))
            ok = false;
    return ok;
}

/*!
 * Unloads a plugin with a specified index.
 * To get an index of a particular plugin you may use getPluginsCount and getPluginInfo.
 *
 * This function returns true if a plugin had been successfully unloaded from memory.
 *
 * \sa getPluginsCount, getPluginInfo
 */
bool MSE_Engine::unloadPlugin(int index)
{
    CHECK((index>=0) && (index<plugins.size()), MSE_Object::Err::outOfRange);
    if(!BASS_PluginFree(pluginHandles[index]))
        return false;
    plugins.removeAt(index);
    pluginHandles.removeAt(index);
    return true;
}

/*!
 * Unloads all loaded plugins.
 *
 * Returns true if ALL plugins were unloaded successfully
 * or there were no loaded plugins in the first place.
 */
bool MSE_Engine::unloadAllPlugins()
{
    int n = plugins.size();
    for(int a=n-1; a>=0; a--)
        unloadPlugin(a);
    return plugins.isEmpty();
}

bool MSE_Engine::unzipFile(const QString &filename, QByteArray& arr) const
{
    QuaZip zip(filename);
    if(!zip.open(QuaZip::mdUnzip))
        return false;
    QuaZipFile f(&zip);
    if(!zip.goToFirstFile())
    {
        zip.close();
        return false;
    }
    if(!f.open(QIODevice::ReadOnly))
    {
        zip.close();
        return false;
    }
    arr = f.readAll();
    f.close();
    zip.close();
    return !arr.isEmpty();
}

/*!
 * Returns a type of a sound file by its URI.
 */
MSE_SoundChannelType MSE_Engine::typeByUri(const QString &uri) const
{
    QString fName = MSE_Utils::normalizeUri(uri);
    if(fName.contains("://"))
    {
        QUrl url(uri);
        if(url.isValid())
            return mse_sctRemote;
        else
            return mse_sctUnknown;
    }

    QFileInfo fi(fName);
    QString ext = fi.suffix().toLower();
    int n = getPluginsCount();
    for(int a=0; a<n; a++)
    {
        MSE_EnginePluginInfo pluginInfo = getPluginInfo(a);
        foreach(MSE_EnginePluginFormat format, pluginInfo.formats)
            if(format.extensions.contains(ext))
                return mse_sctPlugin;
    }

    if((ext == "mp3")||(ext == "mp2")||(ext == "mp1")||(ext == "ogg")||(ext == "wav")||(ext == "aiff"))
        return mse_sctStream;
    if((ext == "mo3")||(ext == "it")||(ext == "xm")||(ext == "s3m")||(ext == "mtm")||(ext == "mod")||(ext == "umx")||
            (ext == "mdz")||(ext == "s3z")||(ext == "xmz")||(ext == "itz"))
        return mse_sctModule;
    if(uri.lastIndexOf(".cue:", -1, Qt::CaseInsensitive) > 0)
        return mse_sctStream;

    return mse_sctUnknown;
}

/*!
 * Returns the first real sound output device on this system.
 */
int MSE_Engine::getRealOutputDeviceIndex()
{
    BASS_DEVICEINFO info;
    int a = 1;
    while(BASS_GetDeviceInfo(a, &info))
    {
        if(info.flags & BASS_DEVICE_ENABLED)
            return a; // on Linux it will be always 1 apparently
        a++;
    }
    return 0;
}

/*!
 * Retreives a User Agent string in the following format:
 *
 * &ltappName&gt;/&lt;appVersion&gt; (&lt;OS name&gt; &ltOS version&gt;) MesonSoundEngine/&lt;yyyymmdd&gt;
 *
 * placeholder | value
 * -------------------------------------------------------------------------------
 * appName     | Application's name provided in *appName*.
 *             | If *appName* is not provided or empty, then use qApp->applicationDisplayName().
 *             | If qApp->applicationDisplayName is not available or empty, then use APP_TITLE macro.
 *             | If the APP_TITLE macro is not declared, then use qApp->applicationName()
 *             | If qApp->applicationName() is empty, then use "MSE".
 * appVersion  | Application's version provided in *appVersion*.
 *             | If *appVersion* is not provided or empty, then use qApp->applicationVersion()
 *             | If qApp->applicationVersion() is empty, then use "Generic".
 * OS name     | The name of the OS.
 *             | May be unavailable on some Linux distributions (will become "Unknown OS").
 * OS version  | The version of the OS.
 *             | May be unavailable on some Linux distributions.
 *             | Only included when OS name is resolved.
 * yyyymmdd    | The build date of the MesonSoundEngine (read as "the application").
 *
 * Examples:
 * \li Meson-Player/0.5 (Windows NT 6.1) MesonSoundEngine/20130512
 * \li MSE-Powered-App/Generic (OSX 10.8) MesonSoundEngine/20130401
 * \li MesonCast/Generic (Debian 7.0) MesonSoundEngine/20130420
 */
QString MSE_Engine::getDefaultUA(const QString &appName, const QString &appVersion)
{
    QString _appName(appName);
    if(_appName.isEmpty())
    {
#ifdef QT_GUI_LIB
        _appName = qApp->applicationDisplayName();
        if(_appName.isEmpty())
        {
#endif
#ifdef APP_TITLE
            _appName = APP_TITLE;
#else
            _appName = qApp->applicationName();
            if(_appName.isEmpty())
                _appName = "MSE Powered App";
#endif
#ifdef QT_GUI_LIB
        }
#endif
    }

    _appName.replace(" ", "-");

    QString _appVersion(appVersion);
    if(_appVersion.isEmpty())
    {
        _appVersion = CoreApp::majMinVersion().toString();
        if(_appVersion.isEmpty())
            _appVersion = "Generic";
    }

    QString _osName = QSysInfo::prettyProductName();
    if(_osName.isEmpty())
        _osName = "Unknown OS";
    QString _mseVersion(CoreApp::buildDate().toString("yyyyMMdd"));

    QString result =
            _appName % QStringLiteral("/") % _appVersion %
            QStringLiteral(" (") % _osName % QStringLiteral(") MesonSoundEngine/") %
            _mseVersion;
    result = result.simplified();
    return result;
}

/*!
 * Increases the sound volume level for a whole MSE system.
 * Returns true on success.
 * Pass a negative value to decrease a sound volume level.
 *
 * \sa snapVolumeToGrid
 */
bool MSE_Engine::changeVolume(float diff, bool snapToGrid)
{
    refreshVolume();
    float val = volume + diff;
    if(snapToGrid)
        val = snapVolumeToGrid(val, diff);
    return setVolume(val);
}

/*!
 * Returns the master volume in range [0;1] or -1 if a sound level cannot be retrieved.
 *
 * \sa setMasterVolume, isMasterVolumeAvailable
 */
float MSE_Engine::getMasterVolume()
{
    CHECKN(masterVolumeAvailable, MSE_Object::Err::masterVolumeNotAvailable);
    float vol;

#ifdef Q_OS_WIN
    CHECKN(SUCCEEDED(mvEndpoint->GetMasterVolumeLevelScalar(&vol)), MSE_Object::Err::unableGetMasterVolume);
#endif

#ifdef Q_OS_OSX
    Float32 _vol;
    UInt32 volPropSize = sizeof(_vol);
    CHECKN(noErr == AudioObjectGetPropertyData(mvDevice, &mvMasterProp, 0, nullptr, &volPropSize, &_vol), MSE_Object::Err::unableGetMasterVolume);
    vol = _vol;
#endif

#ifdef Q_OS_LINUX
    long _vol;
    CHECKN(snd_mixer_handle_events(mvMixerHandle) >= 0, MSE_Object::Err::unableUpdateMasterState);
    CHECKN(!snd_mixer_selem_get_playback_volume(
               mvMixerElem,
               SND_MIXER_SCHN_MONO,
               &_vol), MSE_Object::Err::unableGetMasterVolume);
    vol = (_vol+mvMin)/mvRange;
#endif

    if(vol < 0)
        return 0;
    if(vol > 1)
        return 1;
    return vol;
}

/*!
 * Sets the master volume in range [0;1].
 * Returns false if a sound level cannot be set.
 */
bool MSE_Engine::setMasterVolume(float val)
{
    CHECK(masterVolumeAvailable, MSE_Object::Err::masterVolumeNotAvailable);

    if(val > 1)
        val = 1;
    else
        if(val < 0)
            val = 0;

#ifdef Q_OS_WIN
    CHECK(SUCCEEDED(mvEndpoint->SetMasterVolumeLevelScalar(val, &mvGuid)), MSE_Object::Err::unableSetMasterVolume);
    return true;
#endif
#ifdef Q_OS_OSX
    UInt32 volPropSize = sizeof(Float32);
    CHECK(noErr == AudioObjectSetPropertyData(mvDevice, &mvMasterProp, 0, nullptr, volPropSize, &val), MSE_Object::Err::unableSetMasterVolume);
    return true;
#endif
#ifdef Q_OS_LINUX
    QHash<int, int> switches;
    if(mvHasSwitch)
    {
        for(int a=0; a<SND_MIXER_SCHN_LAST; a++)
        {
            int switchVal;
            if(!snd_mixer_selem_get_playback_switch(mvMixerElem, static_cast<snd_mixer_selem_channel_id_t>(a), &switchVal))
                switches.insert(a, switchVal);
        }
        snd_mixer_selem_set_playback_switch_all(mvMixerElem, 1);
    }

    int setVolResult = snd_mixer_selem_set_playback_volume_all(mvMixerElem, mvMin + mvRange*val);

    if(mvHasSwitch)
    {
        bool ok = true;
        QHashIterator<int, int> i(switches);
        while(i.hasNext())
        {
            i.next();
            if(snd_mixer_selem_set_playback_switch(
                mvMixerElem,
                static_cast<snd_mixer_selem_channel_id_t>(i.key()),
                i.value()
            )){
                ok = false;
            }
        }
        CHECK(ok, MSE_Object::Err::unableRestoreMasterVolumeSwitch);
    }

    CHECK(!setVolResult, MSE_Object::Err::unableSetMasterVolume);

    return true;
#endif
    return false;
}

/*!
 * Increases the global sound volume level.
 * Returns true on success.
 * Pass a negative value to decrease a sound volume level.
 *
 * \sa snapVolumeToGrid
 */
bool MSE_Engine::changeMasterVolume(float diff, bool snapToGrid)
{
    float vol = getMasterVolume();
    if(vol < 0)
        return false;

    float val = vol + diff;
    if(snapToGrid)
        val = snapVolumeToGrid(val, diff);
    return setMasterVolume(val);
}
