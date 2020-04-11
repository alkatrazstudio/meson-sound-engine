/****************************************************************************}
{ mixer.cpp - channels mixer and resampler                                   }
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

#include "mse/utils/mixer.h"

static DWORD CALLBACK streamProc(HSTREAM handle, void *buffer, DWORD length, void *user)
{
    Q_UNUSED(handle);
    MSE_MixerInput* input = static_cast<MSE_MixerInput*>(user);
    DWORD res = input->sound->getData(buffer, length);
    if(res == 0xFFFFFFFF)
        res = 0;
    return res;
}

MSE_Mixer::MSE_Mixer() : MSE_Object()
{
    engine = MSE_Engine::getInstance();
    handle = 0;
    defaultBridgeFlags = 0;
}

MSE_Mixer::~MSE_Mixer()
{
    for(int a=inputs.size()-1; a>=0; a--)
        removeInput(a);
}

/*!
 * Initializes te engine.
 *
 * If params not specified then the default params will be used.
 *
 * Refer to MSE_MixerInitParams for the information about initialization parameters.
 */
bool MSE_Mixer::init(const MSE_MixerInitParams &params)
{
    initParams = params;
    CHECK(!initParams.use3D || engine->getIs3DSupported(), MSE_Object::Err::no3dSupport);

    if(initParams.use3D)
        initParams.nChannels = 1;

    DWORD flags = 0;
    if(initParams.use3D)
        flags |= BASS_SAMPLE_3D;
    if(initParams.useOldFx)
        flags |= BASS_SAMPLE_FX;
    if(initParams.useSoftware)
        flags |= BASS_SAMPLE_SOFTWARE;
    defaultBridgeFlags = flags;
    switch(initParams.sampleType)
    {
        case mse_sst8Bits:
            flags |= BASS_SAMPLE_8BITS;
            break;
        case mse_sstFloat32:
            flags |= BASS_SAMPLE_FLOAT;
            break;
        default:
            break;
    }
    if(initParams.decodeOnly)
        flags |= BASS_STREAM_DECODE;

    //flags |= BASS_MIXER_BUFFER;

    CHECK(handle = BASS_Mixer_StreamCreate(initParams.outputFrequency, initParams.nChannels, flags), MSE_Object::Err::initFail);

    defaultBridgeFlags |= BASS_STREAM_DECODE;
    return true;
}

/**
 * Adds a MSE_Sound instance as one of the inputs for the mixer.
 *
 * Returns true on success.
 */
bool MSE_Mixer::addInput(MSE_Sound *sound)
{
    MSE_MixerInput* input = new MSE_MixerInput(sound);

    connect(sound, SIGNAL(onOpen()), SLOT(onSoundOpen()));
    if(input->sound->isOpen())
        onSoundOpen();

    connect(sound, &MSE_Sound::onVolumeChange, [this, sound]{
        foreach(MSE_MixerInput* input, inputs)
        {
            if(input->sound == sound)
            {
                if(input->bridge)
                    BASS_ChannelSetAttribute(input->bridge, BASS_ATTRIB_VOL, sound->getVolume());
                break;
            }
        }
    });

    inputs.append(input);
    return true;
}

/**
 * Remove one of the mixer's inputs by its index.
 *
 * Returns true on success.
 */
bool MSE_Mixer::removeInput(int index)
{
    CHECK((index >= 0) && (index < inputs.size()), MSE_Object::Err::outOfRange);
    const MSE_MixerInput* input = inputs.at(index);
    BASS_Mixer_ChannelRemove(input->bridge);
    BASS_StreamFree(input->bridge);
    input->sound->disconnect(this, SLOT(onSoundOpen()));
    delete inputs.takeAt(index);
    return true;
}

/**
 * Remove a MSE_Sound instance from the list of the mixer's inputs.
 *
 * Returns true on success.
 * Also returns true if the specified MSE_Sound is not amongst this mixer's inputs.
 */
bool MSE_Mixer::removeInput(MSE_Sound *sound)
{
    for(int a=inputs.size()-1; a>=0; a--)
        if(inputs.at(a)->sound == sound)
            return removeInput(a);
    return true;
}

/**
 * Returns MSE_MixerInput object that corresponds to the specified MSE_Sound object
 * (which should be one of the current inputs).
 *
 * Returns nullptr if the specified MSE_Sound object is not amongst this mixer's inputs.
 */
MSE_MixerInput *MSE_Mixer::getInput(MSE_Sound *sound) const
{
    for(int a=inputs.size()-1; a>=0; a--)
    {
        MSE_MixerInput* input = inputs.at(a);
        if(input->sound == sound)
            return input;
    }
    return nullptr;
}

/*!
 * Decode the next chunk of data of length bytes to buffer.
 * If the mixer is currently paused or stopped then this function returns silence.
 */
int MSE_Mixer::getData(char *buffer, int length)
{
    return BASS_ChannelGetData(handle, buffer, length);
}


/*!
 * Starts mixing.
 */
bool MSE_Mixer::play()
{
    return BASS_ChannelPlay(handle, true);
}

/*!
 * Pause mixing.
 *
 * When the mixing is paused the mixer will return empty data via MSE_Mixer::getData().
 */
bool MSE_Mixer::pause()
{
    return BASS_ChannelPause(handle);
}

/*!
 * Resume mixing.
 */
bool MSE_Mixer::unpause()
{
    return BASS_ChannelPlay(handle, false);
}

/*!
 * Stop mixing.
 *
 * When the mixing is stopped the mixer will return empty data via MSE_Mixer::getData().
 */
bool MSE_Mixer::stop()
{
    return BASS_ChannelStop(handle);
}

/*!
 * Updates current volume information.
 */
void MSE_Mixer::refreshVolume()
{
    if(handle)
    {
        if(!BASS_ChannelGetAttribute(handle, BASS_ATTRIB_VOL, &volume))
            volume = 0;
    }
}


/*!
 * Sets the volume of the mixer's output.
 */
bool MSE_Mixer::setVolume(float value)
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
        return BASS_ChannelSetAttribute(handle, BASS_ATTRIB_VOL, volume);
    else
        return true;
}

/*!
 * Change the volume of the mixer's output by diff.
 *
 * diff is in range from 0 to 1, where 1 is the maximum volume.
 *
 * snapToGrid - see MSE_Engine::snapVolumeToGrid
 */
bool MSE_Mixer::changeVolume(float diff, bool snapToGrid)
{
    refreshVolume();
    float val = volume + diff;
    if(snapToGrid)
        val = engine->snapVolumeToGrid(val, diff);
    return setVolume(val);
}

void MSE_Mixer::onSoundDestroyed(QObject *obj)
{
    MSE_Sound* sound = qobject_cast<MSE_Sound*>(obj);
    if(!sound)
        return;
    removeInput(sound);
}

void MSE_Mixer::onSoundOpen()
{
    MSE_Sound* sound = qobject_cast<MSE_Sound*>(sender());
    if(!sound)
        return;
    foreach(MSE_MixerInput* input, inputs)
    {
        if(input->sound == sound)
        {
            if(input->bridge)
            {
                BASS_Mixer_ChannelRemove(input->bridge);
                BASS_StreamFree(input->bridge);
            }

            DWORD flags = defaultBridgeFlags;

            switch(sound->getInitParams().sampleType)
            {
                case mse_sst8Bits:
                    flags |= BASS_SAMPLE_8BITS;
                    break;
                case mse_sstFloat32:
                    flags |= BASS_SAMPLE_FLOAT;
                    break;
                default:
                    break;
            }

            input->bridge = BASS_StreamCreate(
                        input->sound->getFrequency(),
                        input->sound->getChannelsCount(),
                        flags, &streamProc, input);
            CHECKV(input->bridge, MSE_Object::Err::bridgeCreationFail);

            BASS_ChannelSetAttribute(input->bridge, BASS_ATTRIB_SRC, input->sound->getSampleRateConversion());

            if(!BASS_Mixer_StreamAddChannel(handle, input->bridge, BASS_MIXER_DOWNMIX | BASS_MIXER_NORAMPIN))
            {
                SETERROR(MSE_Object::Err::cannotAddBridge);
                BASS_StreamFree(input->bridge);
                return;
            }

            BASS_ChannelSetAttribute(input->bridge, BASS_ATTRIB_VOL, sound->getVolume());
            break;
        }
    }
}
