/****************************************************************************}
{ source_url.h - network file                                                }
{                                                                            }
{ Copyright (c) 2013 Alexey Parfenov <zxed@alkatrazstudio.net>               }
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

#include "mse/sources/source.h"
#include "mse/bass/bassmix.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

class MSE_SourceUrl : public MSE_Source
{
    Q_OBJECT
public:
    explicit MSE_SourceUrl(MSE_Playlist* parent);
    ~MSE_SourceUrl();
    virtual HCHANNEL open();
    virtual bool close();

protected:
    virtual bool getTags(MSE_SourceTags &tags);
    bool openUrl(const QString& url, int redirectsLeft);

    QNetworkAccessManager* netMan;
    QNetworkReply *netReply;
    QTimer* retryTimer;
    QTimer* timeoutTimer;
    int retriesLeft;

    HSTREAM mixerStream;
    HSTREAM urlStream;

    QString curTrackArtist;
    QString curTrackTitle;

    enum {
        mse_sus_Idle,
        mse_sus_Ready,
        mse_sus_WaitingForStart,
        mse_sus_WaitingPlaylistHeader,
        mse_sus_ReceivingPlaylist,
        mse_sus_WaitingStreamHeader,
        mse_sus_ReceivingStream
    } state;
    int chunkLen;
    int chunkPos;
    int metaLen;
    int preloadLength;
    bool isMono;

    QString _url;
    int _redirectsLeft;

    static const int maxRedirects;
    static const int preloadSecs;
    static const int timeoutInterval;
    static const int maxBufferCapacity;
    static const int retryInterval;
    static const int maxRetries;

    static void CALLBACK fileCloseProc(void *user);
    static QWORD CALLBACK fileLenProc(void *user);
    static DWORD CALLBACK fileReadProc(void *buffer, DWORD length, void *user);
    static DWORD CALLBACK fileReadProcNoMeta(void *buffer, DWORD length, void *user);
    static BOOL CALLBACK fileSeekProc(QWORD offset, void *user);
    static void CALLBACK startProc(HSYNC handle, DWORD channel, DWORD data, void *user);

    BASS_FILEPROCS fileProcTable;

    void onFileClose();
    DWORD onFileRead(void *buffer, DWORD length);
    DWORD onFileReadNoMeta(void *buffer, DWORD length);
    void onMixerStart(DWORD channel);

    void parseMeta(QByteArray &data);

    void onReadError();
    void tryRestartUrl(bool initialStart = false);

protected slots:
    void onSockHeaders();
    void onSockData();
    void onSockDone();
    void onSockError(QNetworkReply::NetworkError err);
    void closeSock();
    void openUrl();
    void retryUrl();
};
