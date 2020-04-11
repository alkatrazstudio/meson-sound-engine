/****************************************************************************}
{ mpris.cpp - MPRIS interface                                                }
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

#include "mse/utils/mpris.h"
#include <QGuiApplication>

static constexpr int TIME_MULTIPLIER = 1000000;

MSE_Mpris::MSE_Mpris(MSE_Sound* sound, const QString& serviceName, const QString& serviceTitle) : MSE_Object(sound)
    ,sound(sound)
    ,playlist(sound->getPlaylist())
    ,playbackModeBeforeRandom(getPlaybackMode() == mse_ppmRandom
                              ? mse_ppmAllLoop
                              : getPlaybackMode())
{
    if(serviceName.isEmpty())
        mpris.setServiceName(qApp->applicationName());
    else
        mpris.setServiceName(serviceName);
    if(serviceTitle.isEmpty())
    {
        QGuiApplication* app = qobject_cast<QGuiApplication*>(qApp);
        if(app)
            mpris.setIdentity(app->applicationDisplayName());
    }
    else
    {
        mpris.setIdentity(serviceTitle);
    }

    mpris.setCanControl(true);
    mpris.setCanGoNext(true);
    mpris.setCanGoPrevious(true);
    mpris.setCanPause(true);
    mpris.setCanPlay(true);
    mpris.setCanQuit(true);
    mpris.setCanRaise(false);
    mpris.setCanSeek(true);
    mpris.setCanSetFullscreen(false);

    mpris.setHasTrackList(false);
    mpris.setMaximumRate(1);
    mpris.setMinimumRate(1);
    mpris.setRate(1);
    updatePlaybackMode();
    updateMetadata();
    updateVolume();

    connect(&mpris, &MprisPlayer::playRequested, this, &MSE_Mpris::playRequested);
    connect(&mpris, &MprisPlayer::quitRequested, this, &MSE_Mpris::quitRequested);
    connect(&mpris, &MprisPlayer::nextRequested, this, &MSE_Mpris::nextRequested);
    connect(&mpris, &MprisPlayer::pauseRequested, this, &MSE_Mpris::pauseRequested);
    connect(&mpris, &MprisPlayer::playPauseRequested, this, &MSE_Mpris::playPauseRequested);
    connect(&mpris, &MprisPlayer::previousRequested, this, &MSE_Mpris::previousRequested);
    connect(&mpris, &MprisPlayer::stopRequested, this, &MSE_Mpris::stopRequested);
    connect(&mpris, &MprisPlayer::volumeRequested, this, &MSE_Mpris::volumeRequested);
    connect(&mpris, &MprisPlayer::openUriRequested, this, &MSE_Mpris::openUriRequested);

    connect(&mpris, &MprisPlayer::seekRequested, this, [this](qlonglong offset){
        double secsOffset = static_cast<double>(offset) / TIME_MULTIPLIER;
        double curPos = getPosition();
        double secs = secsOffset + curPos;
        emit seekRequested(secs);
    });

    connect(&mpris, &MprisPlayer::loopStatusRequested, [this](Mpris::LoopStatus loopStatus){
        MSE_PlaylistPlaybackMode playbackMode;
        switch(loopStatus)
        {
            case Mpris::LoopStatus::None:
                switch(getPlaybackMode())
                {
                    case mse_ppmTrackOnce:
                    case mse_ppmAllOnce:
                        playbackMode = getPlaybackMode();
                        break;

                    case mse_ppmTrackLoop:
                        playbackMode = mse_ppmTrackOnce;
                        break;

                    default:
                        playbackMode = mse_ppmAllOnce;
                }
                break;

            case Mpris::LoopStatus::Track:
                playbackMode = mse_ppmTrackOnce;
                break;

            case Mpris::LoopStatus::Playlist:
                playbackMode = mse_ppmAllOnce;
                break;

            default:
                return;
        }
        emit playbackModeRequested(playbackMode);
    });

    connect(&mpris, &MprisPlayer::rateRequested, [this](double rate){
        if(rate == 0)
            emit pauseRequested();
        mpris.setRate(1);
    });

    connect(&mpris, &MprisPlayer::shuffleRequested, [this](bool shuffle){
        if(shuffle)
        {
            emit playbackModeRequested(mse_ppmRandom);
        }
        else
        {
            if(getPlaybackMode() == mse_ppmRandom)
            {
                emit playbackModeRequested(playbackModeBeforeRandom);
            }
            else
            {
                emit playbackModeRequested(getPlaybackMode());
            }
        }
    });

    connect(sound, &MSE_Sound::onVolumeChange, [this]{
        updateVolume();
        updatePlaybackStatus();
    });

    connect(sound, &MSE_Sound::onPositionChange, [this]{
        updatePlaybackStatus();
    });

    connect(playlist, &MSE_Playlist::onPlaybackModeChange, [this]{
        updatePlaybackMode();
        updatePlaybackStatus();
    });

    connect(sound, &MSE_Sound::onContinuousStateChange, [this]{
        updatePlaybackStatus();
    });

    connect(sound, &MSE_Sound::onInfoChange, [this]{
        updateMetadata();
        updatePlaybackStatus();
    });

    posTimer.setInterval(100);
    posTimer.setSingleShot(false);
    connect(&posTimer, &QTimer::timeout, this, [this]{
        qlonglong position = getPosition() * TIME_MULTIPLIER;
        mpris.setPosition(position);
    });
    posTimer.start();
}

MSE_Mpris::~MSE_Mpris()
{
    posTimer.stop();
    mpris.setPlaybackStatus(Mpris::Stopped);
}

void MSE_Mpris::updateVolume()
{
    float vol = getVolume();
    mpris.setVolume(vol);
}

void MSE_Mpris::updateMetadata()
{
    MSE_Source* src = playlist->getCurrentSource();
    if(src == nullptr)
        return;

    QVariantMap map;
    const MSE_SourceTags& tags = this->sound->getTags();
    QDateTime dt = QDateTime::fromString(tags.trackDate, "YYYY");
    int i;
    bool ok;
    QUrl url(this->sound->getTrackFilename());
    if(url.scheme().isEmpty())
        url.setScheme("file");

    map[Mpris::metadataToString(Mpris::Metadata::TrackId)] = QString("/mesonplayer/" + QString::number(playlist->getIndex()));
    map[Mpris::metadataToString(Mpris::Metadata::Length)] = this->sound->getTrackDuration() * TIME_MULTIPLIER;
    map[Mpris::metadataToString(Mpris::Metadata::Album)] = tags.trackAlbum;
    map[Mpris::metadataToString(Mpris::Metadata::Artist)] = this->sound->getTrackArtist();
    if(dt.isValid())
        map[Mpris::metadataToString(Mpris::Metadata::ContentCreated)] = dt;
    i = tags.discIndex.toInt(&ok);
    if(ok)
        map[Mpris::metadataToString(Mpris::Metadata::DiscNumber)] = i;
    map[Mpris::metadataToString(Mpris::Metadata::Genre)] = tags.genre;
    map[Mpris::metadataToString(Mpris::Metadata::Title)] = this->sound->getTrackTitle();
    i = tags.trackIndex.toInt(&ok);
    if(ok)
        map[Mpris::metadataToString(Mpris::Metadata::TrackNumber)] = i;
    map[Mpris::metadataToString(Mpris::Metadata::Url)] = url;

    mpris.setMetadata(map);
    updatePlaybackStatus();
}

void MSE_Mpris::updatePlaybackMode()
{
    switch(getPlaybackMode())
    {
        case mse_ppmAllLoop:
            mpris.setLoopStatus(Mpris::LoopStatus::Playlist);
            break;

        case mse_ppmTrackLoop:
            mpris.setLoopStatus(Mpris::LoopStatus::Track);
            break;

        default:
            mpris.setLoopStatus(Mpris::LoopStatus::None);
    }
    mpris.setShuffle(getPlaybackMode() == mse_ppmRandom);
    updatePlaybackStatus();
}

void MSE_Mpris::updatePlaybackStatus()
{
    qlonglong position;
    switch(getState())
    {
        case mse_scsPaused:
            mpris.setPlaybackStatus(Mpris::Paused);
            position = getPosition() * TIME_MULTIPLIER;
            mpris.setPosition(position);
            mpris.seeked(position);
            break;

        case mse_scsPlaying:
            mpris.setPlaybackStatus(Mpris::Playing);
            position = getPosition() * TIME_MULTIPLIER;
            mpris.setPosition(position);
            mpris.seeked(position);
            break;

        default:
            mpris.setPlaybackStatus(Mpris::Stopped);
            mpris.setPosition(0);
            mpris.seeked(0);
    }
}

double MSE_Mpris::getPosition()
{
    return sound->getPosition();
}

MSE_PlaylistPlaybackMode MSE_Mpris::getPlaybackMode()
{
    return sound->getPlaylist()->getPlaybackMode();
}

MSE_SoundChannelState MSE_Mpris::getState()
{
    return sound->getContinuousState();
}

float MSE_Mpris::getVolume()
{
    return sound->getVolume();
}
