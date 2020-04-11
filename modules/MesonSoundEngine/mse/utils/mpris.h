/****************************************************************************}
{ mpris.h - MPRIS interface                                                  }
{                                                                            }
{ Copyright (c) 2017 Alexey Parfenov <zxed@alkatrazstudio.net>               }
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

#include "mse/sound.h"
#include <MprisQt/MprisPlayer>

class MSE_Mpris : public MSE_Object
{
    Q_OBJECT
public:
    MSE_Mpris(MSE_Sound* sound, const QString &serviceName = QString(), const QString &serviceTitle = QString());
    virtual ~MSE_Mpris();

    void updateVolume();

protected:
    MprisPlayer mpris;
    MSE_Sound* sound;
    MSE_Playlist* playlist;
    MSE_PlaylistPlaybackMode playbackModeBeforeRandom;
    QTimer posTimer;

    void updateMetadata();
    void updatePlaybackMode();
    void updatePlaybackStatus();

    virtual double getPosition();
    virtual MSE_PlaylistPlaybackMode getPlaybackMode();
    virtual MSE_SoundChannelState getState();
    virtual float getVolume();

signals:
    void playRequested();
    void quitRequested();
    void nextRequested();
    void pauseRequested();
    void playPauseRequested();
    void previousRequested();
    void stopRequested();
    void playbackModeRequested(MSE_PlaylistPlaybackMode playbackMode);
    void volumeRequested(double volume);
    void openUriRequested(const QUrl &url);
    void seekRequested(double secs);
};

/******* HELPER TEMPLATE *******

    MSE_Mpris *mpris = new MSE_Mpris(sound);

    connect(mpris, &MSE_Mpris::playRequested, [this]{

    });

    connect(mpris, &MSE_Mpris::pauseRequested, [this]{

    });

    connect(mpris, &MSE_Mpris::playPauseRequested, [this]{

    });

    connect(mpris, &MSE_Mpris::stopRequested, [this]{

    });

    connect(mpris, &MSE_Mpris::nextRequested, [this]{

    });

    connect(mpris, &MSE_Mpris::previousRequested, [this]{

    });

    connect(mpris, &MSE_Mpris::seekRequested, [this](double secs){

    });

    connect(mpris, &MSE_Mpris::playbackModeRequested, [this](MSE_PlaylistPlaybackMode playbackMode){

    });

    connect(mpris, &MSE_Mpris::volumeRequested, [this](double volume){

    });

    connect(mpris, &MSE_Mpris::openUriRequested, [this](const QUrl &url){

    });

    connect(mpris, &MSE_Mpris::quitRequested, [this]{

    });
*/
