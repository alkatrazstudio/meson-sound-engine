/****************************************************************************}
{ object.cpp - base class                                                    }
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

#include "mse/object.h"

MSE_Object::MSE_Object(QObject *parent) : QObject(parent)
{

}

/*!
 * Returns a human-readable string for a specific error code.
 */
QString MSE_Object::errorCodeToString(Err errorCode)
{
    switch(errorCode)
    {
        case Err::unknown: return QStringLiteral("Unknown error");
        case Err::defaultDeviceNotAvail: return QStringLiteral("Default device is not available");
        case Err::initFail: return QStringLiteral("Initialization failed");
        case Err::recordInitFail: return QStringLiteral("Recording device initialization failed");
        case Err::no3dSupport: return QStringLiteral("3D effects are either not supported or not enabled");
        case Err::stereoNotEnabled: return QStringLiteral("Stereo mode is not enabled");
        case Err::unknownFileType: return QStringLiteral("Unknown file type");
        case Err::notLocalFile: return QStringLiteral("Not a local file");
        case Err::pathNotFound: return QStringLiteral("Path not found");
        case Err::cannotGetCanonicalPath: return QStringLiteral("Cannot resolve path");
        case Err::cannotGetAbsolutePath: return QStringLiteral("Cannot expand path");
        case Err::notURL: return QStringLiteral("Not URL");
        case Err::playlistIsEmpty: return QStringLiteral("Playlist is empty");
        case Err::noValidFilesFound: return QStringLiteral("No valid files found");
        case Err::openFail: return QStringLiteral("Fail at open");
        case Err::openWriteFail: return QStringLiteral("Open for write fail");
        case Err::readError: return QStringLiteral("Fail at read");
        case Err::writeError: return QStringLiteral("Write Error");
        case Err::invalidFormat: return QStringLiteral("Invalid Format");
        case Err::invalidVersion: return QStringLiteral("Invalid library version");
        case Err::processNotStarted: return QStringLiteral("Process has not been started");
        case Err::processNotRunning: return QStringLiteral("Process is either not running or not responding");
        case Err::memoryError: return QStringLiteral("Memory Error");
        case Err::accurateIntervalsNotEnabled: return QStringLiteral("Accurate intervals were not enabled");
        case Err::cannotBindAddress: return QStringLiteral("Cannot bind address and/or port");
        case Err::cannotCreateVirtFile: return QStringLiteral("Cannot create a virtual file for using accurate intervals");
        case Err::cannotInitVirtFile: return QStringLiteral("Cannot initialize a virtual file for using accurate intervals");
        case Err::cannotStartVirtFile: return QStringLiteral("Cannot start a virtual file for using accurate intervals");
        case Err::virtFileClosed: return QStringLiteral("Buffer underflow! Filling the buffer...");
        case Err::alreadyDone: return QStringLiteral("Already done");
        case Err::cannotFetchPluginInfo: return QStringLiteral("Cannot fetch a plugin info");
        case Err::outOfRange: return QStringLiteral("Out of range");
        case Err::cueIndexLost: return QStringLiteral("CUE index error");
        case Err::cueIndexOutOfRange: return QStringLiteral("CUE index out of range");
        case Err::cueSourceNotFound: return QStringLiteral("Cannot find source for CUE sheet");
        case Err::urlInvalid: return QStringLiteral("Invalid URL");
        case Err::cannotLoadSound: return QStringLiteral("Cannot load the sound file");
        case Err::bridgeCreationFail: return QStringLiteral("Mixer bridge creation failed");
        case Err::cannotAddBridge: return QStringLiteral("Cannot add a bridge to the mixer");
        case Err::mixerInputNotFound: return QStringLiteral("Mixer input not found");
        case Err::noMixerInputs: return QStringLiteral("No mixer inputs");
        case Err::invalidState: return QStringLiteral("Invalid state");
        case Err::operationFailed: return QStringLiteral("Operation failed");
        case Err::invalidRedirect: return QStringLiteral("Invalid redirect");
        case Err::cannotInitializeCOM: return QStringLiteral("Cannot initialize COM interface");
        case Err::unableCreateGuid: return QStringLiteral("Unable to create a new GUID object");
        case Err::unableGetEnumerator: return QStringLiteral("Unable to get an audio enumerator");
        case Err::unableGetEndpoint: return QStringLiteral("Unable to get a default audio endpoint");
        case Err::unableFindChannelController: return QStringLiteral("Unable to find any sound channel controller");
        case Err::unableActivateEndpoint: return QStringLiteral("Unable to activate the audio endpoint");
        case Err::unableGetMasterVolume: return QStringLiteral("Unable to retrieve a master volume");
        case Err::unableUpdateMasterState: return QStringLiteral("Unable to get the current state of the audio endpoint");
        case Err::unableSetMasterVolume: return QStringLiteral("Unable to set a master volume");
        case Err::unableRestoreMasterVolumeSwitch: return QStringLiteral("Unable to restore a master volume switch");
        case Err::masterVolumeNotAvailable: return QStringLiteral("Master volume control is not available");
        case Err::openMixer: return QStringLiteral("Cannot open the audio mixer");
        case Err::mixerAttach: return QStringLiteral("Unable to attach source channel to the audio mixer");
        case Err::registerMixerElement: return QStringLiteral("Cannot register the audio mixer");
        case Err::loadMixer: return QStringLiteral("Cannot load the audio mixer");
        case Err::masterVolumeElementNotFound: return QStringLiteral("Master volume element not found");
        case Err::masterVolumeRange: return QStringLiteral("Unable to retreive a msater volume range");
        case Err::dynLoadFailed: return QStringLiteral("Unable to load a shared library");
        case Err::apiRequest: return QStringLiteral("API call failed");
        case Err::cannotOpenBrowser: return QStringLiteral("Cannot open the URL in a user's browser");
        case Err::decryptError: return QStringLiteral("Decryption error");
        case Err::encryptError: return QStringLiteral("Encryption error");
        case Err::pushData: return QStringLiteral("Cannot push all the required data to stream");
        case Err::networkTimeout: return QStringLiteral("Network timeout");
        case Err::tooManyRedirects: return QStringLiteral("Too much redirects");
        case Err::cannotGetAttribute: return QStringLiteral("Cannot get a channel attribute");
        case Err::cannotSetAttribute: return QStringLiteral("Cannot set a channel attribute");
        case Err::cannotGetInfo: return QStringLiteral("Cannot get a channel info");
        case Err::unsupportedFormat: return QStringLiteral("Unsupported channel format");
        case Err::cannotInitStream: return QStringLiteral("Cannot initialize stream");
        case Err::cannotAddSync: return QStringLiteral("Cannot add sync to the channel");
        case Err::noRetriesLeft: return QStringLiteral("No retries left");
        default: return QStringLiteral("Undefined error");
    }
}
