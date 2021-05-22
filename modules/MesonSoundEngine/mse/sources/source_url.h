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
#include "mse/playlist.h"
#include "mse/bass/bassmix.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

class UrlStreamCreator : public QObject
{
    Q_OBJECT

public:
    UrlStreamCreator()
    {
        qRegisterMetaType<DWORD>("DWORD");
        qRegisterMetaType<HSTREAM>("HSTREAM");
    }

public slots:
    void create(DWORD flags, BASS_FILEPROCS *procs, void *user)
    {
        HSTREAM urlStream = BASS_StreamCreateFileUser(STREAMFILE_BUFFER, flags, procs, user);
        emit resultReady(urlStream);
    }

signals:
    void resultReady(HSTREAM urlStream);
};


class UrlStreamBuffer : public QObject
{
    Q_OBJECT
    QByteArray data;
    mutable QMutex mutex;

public:
    qint64 bytesAvailable() const
    {
        QMutexLocker locker(&mutex);
        return data.size();
    }

    qint64 read(char *dest, qint64 maxSize)
    {
        QMutexLocker locker(&mutex);
        qint64 size = qMin(maxSize, static_cast<qint64>(data.size()));
        memcpy(dest, data.constData(), size);
        data.remove(0, size);
        return size;
    }

    void write(QByteArray newData)
    {
        QMutexLocker locker(&mutex);
        data.append(newData);
    }

    void clear()
    {
        QMutexLocker locker(&mutex);
        data.clear();
    }
};


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
    bool openUrl(const MSE_PlaylistEntry& urlEntry, int redirectsLeft);

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

    MSE_PlaylistEntry _url;
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

    void tryRestartUrl(bool initialStart = false);

    bool urlStreamIsClosed = true;
    QThread urlStreamCreatorThread;
    UrlStreamCreator *urlStreamCreator;

    UrlStreamBuffer urlStreamBuffer;

protected slots:
    void onSockHeaders();
    void onSockData();
    void onSockDone();
    void onSockError(QNetworkReply::NetworkError err);
    void closeSock();
    void openUrl();
    void retryUrl();
    void onUrlStreamReady(HSTREAM urlStream);

signals:
    void onUrlStreamCreate(DWORD flags, BASS_FILEPROCS *procs, void *user);
};
