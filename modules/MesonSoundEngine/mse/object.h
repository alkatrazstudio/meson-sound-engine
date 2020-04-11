/****************************************************************************}
{ object.h - base class                                                      }
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

/*!
 * This dummy class is a parent for all MSE classes.
 */
class MSE_Object : public QObject
{
    Q_OBJECT

public:
    /*!
     * Possible error codes.
     */
    enum class Err {
        unknown = 1, /*!< Unknown error */
        defaultDeviceNotAvail, /*!< Default device is not available */
        initFail, /*!< Initialization failed */
        recordInitFail, /*!< Recording device initialization failed */
        no3dSupport, /*!< 3D effects are either not supported or not enabled */
        stereoNotEnabled, /*!< Stereo mode is not enabled */
        unknownFileType, /*!< Unknown file type */
        notLocalFile, /*!< Not a local file */
        pathNotFound, /*!< Path not found */
        cannotGetCanonicalPath, /*!< Cannot resolve path */
        cannotGetAbsolutePath, /*!< Cannot expand path */
        notURL, /*!< Not URL */
        playlistIsEmpty, /*!< Playlist is empty */
        noValidFilesFound, /*!< No valid files found */
        openFail, /*!< Fail at open */
        openWriteFail, /*!< Open for write fail */
        cannotGetAttribute, /*!< Cannot get a channel attribute */
        cannotSetAttribute, /*!< Cannot set a channel attribute */
        cannotGetInfo, /*!< Cannot get a channel info */
        unsupportedFormat, /*!< Unsupported channel format */
        cannotInitStream, /*!< Cannot initialize stream */
        cannotAddSync, /*!< Cannot add sync to the channel */
        noRetriesLeft, /*!< No retries left */
        readError, /*!< Fail at read */
        writeError, /*!< Write Error */
        invalidFormat, /*!< Invalid Format */
        invalidVersion, /*!< Invalid version */
        processNotStarted, /*!< Process has not been started */
        processNotRunning, /*!< Process is either not running or not responding */
        memoryError, /*!< Memory Error */
        accurateIntervalsNotEnabled, /*!< Accurate intervals were not enabled */
        cannotBindAddress, /*!< Cannot bind address and/or port */
        cannotCreateVirtFile, /*!< Cannot create a virtual file for using accurate intervals */
        cannotInitVirtFile, /*!< Cannot initialize a virtual file for using accurate intervals */
        cannotStartVirtFile, /*!< Cannot start a virtual file for using accurate intervals */
        virtFileClosed, /*!< Buffer underflow! Filling the buffer... */
        alreadyDone, /*!< Already done */
        cannotFetchPluginInfo, /*!< Cannot fetch a plugin info */
        outOfRange, /*!< Out of range */
        cueIndexLost, /*!< CUE index error */
        cueIndexOutOfRange, /*!< CUE index out of range */
        cueSourceNotFound, /*!< Cannot find source for CUE sheet */
        urlInvalid, /*!< Invalid URL */
        cannotLoadSound, /*!< Cannot load the sound file */
        bridgeCreationFail, /*!< Mixer bridge creation failed */
        cannotAddBridge, /*!< Cannot add a bridge to the mixer */
        mixerInputNotFound, /*!< Mixer input not found */
        noMixerInputs, /*!< No mixer inputs */
        invalidState, /*!< Invalid state */
        operationFailed, /*!< Operation failed */
        invalidRedirect, /*!< Invalid redirect */
        cannotInitializeCOM, /*!< Cannot initialize COM interface */
        unableCreateGuid, /*!< Unable to create a new GUID object */
        unableGetEnumerator, /*!< Unable to get an audio enumerator */
        unableGetEndpoint, /*!< Unable to get a default audio endpoint */
        unableFindChannelController, /*!< Unable to find any sound channel controller */
        unableActivateEndpoint, /*!< Unable to activate the audio endpoint */
        unableGetMasterVolume, /*!< Unable to retreive a master volume */
        unableUpdateMasterState, /*!< Unable to get the current state of the audio endpoint */
        unableSetMasterVolume, /*!< Unable to set a master volume */
        unableRestoreMasterVolumeSwitch, /*!< Unable to restore a master volume switch */
        masterVolumeNotAvailable, /*!< Master volume control is not available */
        openMixer, /*!< Cannot open the audio mixer */
        mixerAttach, /*!< Unable to attach source channel to the audio mixer */
        registerMixerElement, /*!< Cannot register the audio mixer  */
        loadMixer, /*!< Cannot load the audio mixer */
        masterVolumeElementNotFound, /*!< Master volume element not found */
        masterVolumeRange, /*!< Unable to retreive a msater volume range */
        dynLoadFailed, /*!< Unable to load a shared library */
        apiRequest, /*!< API call failed */
        cannotOpenBrowser, /*!< Cannot open the URL in a user's browser */
        decryptError, /*!< Decryption error */
        encryptError, /*!< Decryption error */
        pushData, /*!< Cannot push all the required data to stream */
        networkTimeout, /*! Network timeout. */
        tooManyRedirects /*! Too much redirects. */
    };
    Q_ENUM(Err)

    explicit MSE_Object(QObject *parent = nullptr);

    static QString errorCodeToString(Err errorCode);
};
