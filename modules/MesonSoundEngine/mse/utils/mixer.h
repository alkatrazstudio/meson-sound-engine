/****************************************************************************}
{ mixer.h - channels mixer and resampler                                     }
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

#include "mse/bass/bassmix.h"

/*!
 * Parameters for MSE_Mixer initialization.
 */
struct MSE_MixerInitParams {
    MSE_SoundSampleType sampleType = mse_sstFloat32; /*!<
    Sets the type of samples of the mixer output.

    **Default**: ::mse_sstFloat32;

    \note If you set <tt>MSE_EngineInitParams.use8Bits = true</tt> when initializing a sound engine,
    then you must set this param to ::mse_sst8Bits.

    \sa MSE_SoundSampleType
*/
    int nChannels = 2; /*!<
    Number of channels to use for output.

    **Default**: 2

    \note If you set <tt>MSE_EngineInitParams.useMono = false</tt> when initializing a sound engine,
    then you must set this param to false.
*/
    bool useSoftware = false; /*!<
    Use software for sound mixing, else use hardware.

    **Default**: false
*/
    bool use3D = false; /*!<
    Enable support for 3D effects (EAX).

    **Default**: false
*/
    bool useOldFx = false; /*!<
    Enable the old implementation of DirectX 8 effects for the mixer output.

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
    bool decodeOnly = false; /*!<
    Mix the sample data, without playing it.

    **Default**: false
*/
    quint32 outputFrequency = 44100; /*!<
    Output sample rate in Hz.

    **Valid values**: 192000, 96000, 48000, 44100, 22050, 16000, 11025, 8000

    **Default**: 44100
*/
};

/*!
 * A single input source for MSE_Mixer instance.
 */
struct MSE_MixerInput {
    MSE_Sound* sound; /*!<
    MSE_Sound instance. Must have MSE_SoundInitParams::decodeOnly flag set to true.
*/
    HSTREAM bridge; /*!<
    A bridge stream between the MSE_MixerInput::sound and the mixer. Only used internally. Must not be changed by user.
*/

    MSE_MixerInput(MSE_Sound* sound):
        sound(sound),
        bridge(0){}
};

/*!
 * List of MSE_MixerInput instances.
 */
typedef QList<MSE_MixerInput*> MSE_MixerInputs;

class MSE_Mixer : public MSE_Object
{
    Q_OBJECT

public:
    explicit MSE_Mixer();
    ~MSE_Mixer();

    /*!
     * Returns the used engine.
     */
    inline MSE_Engine* getEngine() const {return engine;}

    bool init(const MSE_MixerInitParams& params = MSE_MixerInitParams());

    /*!
     * Get current initialization parameters.
     */
    inline const MSE_MixerInitParams& getInitParams() const {return initParams;}

    bool addInput(MSE_Sound* sound);
    bool removeInput(int index);
    bool removeInput(MSE_Sound* sound);
    inline int getInputsCount() const {return inputs.size();}
    inline MSE_MixerInput* getInput(int index) const {return inputs.at(index);}
    MSE_MixerInput *getInput(MSE_Sound* sound) const;

    int getData(char *buffer, int length);

    bool play();
    bool pause();
    bool unpause();
    bool stop();

    inline float getVolume() const {return volume;}
    void refreshVolume();
    bool setVolume(float value);
    bool changeVolume(float diff, bool snapToGrid = false);

protected:
    MSE_Engine* engine;
    HSTREAM handle;
    MSE_MixerInputs inputs;
    MSE_MixerInitParams initParams;
    DWORD defaultBridgeFlags;
    float volume;

    int getFrequency(MSE_Sound* sound);

protected slots:
    void onSoundDestroyed(QObject* obj);
    void onSoundOpen();
};
