/****************************************************************************}
{ source_url.cpp - network file                                              }
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

#include "mse/sources/source_url.h"
#include "mse/sound.h"
#include "qiodevicehelper.h"
#include "coreapp.h"

const int MSE_SourceUrl::maxRedirects = 5; // including redirects from playlists
const int MSE_SourceUrl::preloadSecs = 0; // create a stream when this amount of seconds available
const int MSE_SourceUrl::timeoutInterval = 10*1000; // timeout for every single network operation
const int MSE_SourceUrl::maxBufferCapacity = 10*1024*1024; // once network buffer is full, the connection is closed
const int MSE_SourceUrl::retryInterval = 10*1000; // wait for this amount of time before reconnect
const int MSE_SourceUrl::maxRetries = 5; // retry this many times

void CALLBACK MSE_SourceUrl::startProc(HSYNC handle, DWORD channel, DWORD data, void *user)
{
    Q_UNUSED(handle);
    Q_UNUSED(data);
    static_cast<MSE_SourceUrl*>(user)->onMixerStart(channel);
}

void MSE_SourceUrl::fileCloseProc(void *user)
{
    static_cast<MSE_SourceUrl*>(user)->urlStreamIsClosed = true;
}

QWORD MSE_SourceUrl::fileLenProc(void *user)
{
    Q_UNUSED(user);
    return 0;
}

DWORD MSE_SourceUrl::fileReadProc(void *buffer, DWORD length, void *user)
{
    return static_cast<MSE_SourceUrl*>(user)->onFileRead(buffer, length);
}

DWORD MSE_SourceUrl::fileReadProcNoMeta(void *buffer, DWORD length, void *user)
{
    return static_cast<MSE_SourceUrl*>(user)->onFileReadNoMeta(buffer, length);
}

BOOL MSE_SourceUrl::fileSeekProc(QWORD offset, void *user)
{
    Q_UNUSED(offset);
    Q_UNUSED(user);
    return false;
}

DWORD MSE_SourceUrl::onFileRead(void *buffer, DWORD length)
{
    char* p = static_cast<char*>(buffer);
    DWORD n = 0;

    forever
    {
        forever
        {
            if(urlStreamIsClosed)
                return n;

            if(metaLen == 0) // waiting for meta byte
            {
                if(urlStreamBuffer.bytesAvailable() < 1)
                    break;
                quint8 metaByte;
                urlStreamBuffer.read(reinterpret_cast<char*>(&metaByte), 1);
                if(!metaByte)
                {
                    metaLen = -1;
                    chunkPos = 0;
                }
                else
                {
                    metaLen = 16 * metaByte;
                }
            }

            if(metaLen > 0)
            {
                if(urlStreamBuffer.bytesAvailable() < metaLen)
                    break;
                QByteArray metaData;
                metaData.resize(metaLen);
                urlStreamBuffer.read(metaData.data(), metaLen);
                parseMeta(metaData);
                metaLen = -1;
                chunkPos = 0;
            }

            qint64 nToRead = qMin(urlStreamBuffer.bytesAvailable(), static_cast<qint64>(length - n));
            nToRead = qMin(static_cast<qint64>(chunkLen - chunkPos), nToRead);
            if(!nToRead)
                break;
            urlStreamBuffer.read(p, nToRead);
            p += nToRead;
            n += nToRead;
            chunkPos += nToRead;

            if(chunkPos == chunkLen)
                metaLen = 0;
        }

        if(n)
            return n;

        QThread::msleep(100);
    }
}

DWORD MSE_SourceUrl::onFileReadNoMeta(void *buffer, DWORD length)
{
    forever
    {
        if(urlStreamIsClosed)
            return 0;

        if(urlStreamBuffer.bytesAvailable())
            return urlStreamBuffer.read((char*)buffer, length);

        QThread::msleep(100);
    }
}

void MSE_SourceUrl::onMixerStart(DWORD channel)
{
    if(channel == mixerStream)
        tryRestartUrl(true);
}

void MSE_SourceUrl::parseMeta(QByteArray &data)
{
    if(sound->getInitParams().icuUseForRemoteSources)
    {
        cpTr.addEntry(data.constData(), data.size(), [&](const QString& icyString){
            setIcyString(icyString);
        });
        cpTr.processEntries(getTrReference());
        return;
    }

    QString icyString = QString::fromUtf8(data);
    setIcyString(icyString);
}

void MSE_SourceUrl::setIcyString(const QString& icyString)
{
    static QRegularExpression rx(
        "StreamTitle\\=\\'(.*?)\\'\\;",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = rx.match(icyString);
    QString trackArtist;
    QString trackTitle;
    if(match.hasMatch())
    {
        const QString& cap = match.captured(1);
        int p = cap.indexOf(QStringLiteral(" - "));
        if(p >= 0)
        {
            trackArtist = cap.left(p).trimmed();
            trackTitle = cap.mid(p+3).trimmed();
        }
        else
        {
            trackTitle = cap.trimmed();
        }
    }

    if((trackArtist != curTrackArtist) || (trackTitle != curTrackTitle))
    {
        curTrackArtist = trackArtist;
        curTrackTitle = trackTitle;
        emit onMeta();
    }
}

void MSE_SourceUrl::tryRestartUrl(bool initialStart)
{
    if(state != mse_sus_WaitingForStart)
    {
        state = mse_sus_WaitingForStart;
        if(initialStart)
            QTimer::singleShot(0, this, SLOT(openUrl()));
        else
            QTimer::singleShot(0, this, SLOT(retryUrl()));
    }
}

void MSE_SourceUrl::onSockHeaders()
{
    QString url = netReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();
    if(!url.isEmpty())
        openUrl(MSE_PlaylistEntry(url), _redirectsLeft-1);
}

void MSE_SourceUrl::closeSock()
{
    if(urlStreamCreatorThread.isRunning())
    {
        urlStreamIsClosed = true;
        urlStreamCreatorThread.quit();
        urlStreamCreatorThread.wait();
    }
    if(urlStream)
    {
        BASS_StreamFree(urlStream);
        urlStream = 0;
    }
    if(netReply)
    {
        netReply->disconnect();
        netReply->close();
        netReply->deleteLater();
        netReply = nullptr;
    }
    if(netMan)
    {
        netMan->disconnect();
        netMan->deleteLater();
        netMan = nullptr;
    }
    if(retryTimer)
    {
        retryTimer->disconnect();
        retryTimer->deleteLater();
        retryTimer = nullptr;
    }
    if(timeoutTimer)
    {
        timeoutTimer->disconnect();
        timeoutTimer->deleteLater();
        timeoutTimer = nullptr;
    }
    urlStreamBuffer.clear();
    state = mse_sus_Idle;
}

void MSE_SourceUrl::onSockDone()
{
    if(state != mse_sus_ReceivingPlaylist)
        return;

    QBufferEx buf;
    buf.open(QIODevice::ReadWrite);
    buf.write(netReply->readAll());
    buf.seek(0);

    // parse response as a playlist
    QList<MSE_PlaylistEntry> pList;
    if(!sound->getPlaylist()->parse(&buf, pList))
    {
        SETERROR(MSE_Object::Err::invalidFormat, _url.filename);
        state = mse_sus_Idle;
        return;
    }
    int n = pList.size();
    if(!n)
    {
        SETERROR(MSE_Object::Err::playlistIsEmpty, _url.filename);
        state = mse_sus_Idle;
        return;
    }

    // try random entry from the playlist (must be url)
    int curRedirects = _redirectsLeft;
    int i = 0;//rnd(n);
    if(openUrl(pList[i], curRedirects-1))
        return;

    // if it fails then try every other entry
    for(int a=0; a<n; a++)
        if(a != i)
            if(openUrl(pList[a], curRedirects-1))
                return;

    SETERROR(MSE_Object::Err::noValidFilesFound);
}

void MSE_SourceUrl::onSockError(QNetworkReply::NetworkError err)
{
    closeSock();
    switch(err)
    {
        case QNetworkReply::RemoteHostClosedError:
        case QNetworkReply::TimeoutError:
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::NetworkSessionFailedError:
        case QNetworkReply::ProxyConnectionClosedError:
        case QNetworkReply::ProxyTimeoutError:
        case QNetworkReply::ContentReSendError:
        case QNetworkReply::InternalServerError:
        case QNetworkReply::ServiceUnavailableError:
        case QNetworkReply::UnknownNetworkError:
        case QNetworkReply::UnknownProxyError:
        case QNetworkReply::UnknownContentError:
            tryRestartUrl();
            break;

        default:
            break;
    }
}

void MSE_SourceUrl::openUrl()
{
    openUrl(entry, maxRedirects);
}

void MSE_SourceUrl::retryUrl()
{
    closeSock();
    retriesLeft--;
    if(!retriesLeft)
    {
        SETERROR(MSE_Object::Err::noRetriesLeft);
        return;
    }

    retryTimer = new QTimer;
    retryTimer->setSingleShot(true);
    retryTimer->setInterval(retryInterval);
    connect(retryTimer, &QTimer::timeout, [this](){
        openUrl();
    });
    retryTimer->start();
}

void MSE_SourceUrl::onSockData()
{
    if(netReply->bytesAvailable() > maxBufferCapacity
        || urlStreamBuffer.bytesAvailable() > maxBufferCapacity)
    {
        if(sound->getState() != mse_scsPlaying)
            sound->close();
        else
            tryRestartUrl(true);
        return;
    }

    timeoutTimer->start(); // reset network timeout every time some data arrives

    qint64 nBytes;

    switch(state)
    {
        case mse_sus_WaitingPlaylistHeader:
            nBytes = netReply->bytesAvailable();
            if(nBytes < MSE_Playlist::detectLength)
                return;

            if(MSE_Playlist::typeByHeader(netReply) != mse_pftUnknown)
            {
                if(!_redirectsLeft)
                {
                    SETERROR(MSE_Object::Err::tooManyRedirects);
                    closeSock();
                    return;
                }

                state = mse_sus_ReceivingPlaylist;
            }
            else
            {
                bool ok;
                int bitrate = QString::fromUtf8(netReply->rawHeader("icy-br")).toInt(&ok);
                if(!ok)
                    bitrate = 256;
                preloadLength = bitrate / 8 * 1024 * preloadSecs;
                state = mse_sus_WaitingStreamHeader;
            }
            break;

        case mse_sus_WaitingStreamHeader:
            nBytes = netReply->bytesAvailable();
            if(nBytes < preloadLength)
                return;

            bool ok;
            chunkLen = QString::fromUtf8(netReply->rawHeader("icy-metaint")).toInt(&ok);
            if(!ok)
                chunkLen = 0;
            if(chunkLen)
                fileProcTable.read = fileReadProc;
            else
                fileProcTable.read = fileReadProcNoMeta;
            chunkPos = 0;
            metaLen = -1;

            if(urlStream)
                BASS_StreamFree(urlStream);

            retriesLeft = maxRetries;
            state = mse_sus_ReceivingStream;
            urlStreamIsClosed = false;

            urlStreamCreator = new UrlStreamCreator;
            urlStreamCreator->moveToThread(&urlStreamCreatorThread);
            connect(
                &urlStreamCreatorThread, &QThread::finished,
                urlStreamCreator, &QObject::deleteLater
            );
            connect(
                this, &MSE_SourceUrl::onUrlStreamCreate,
                urlStreamCreator, &UrlStreamCreator::create
            );
            connect(
                urlStreamCreator, &UrlStreamCreator::resultReady,
                this, &MSE_SourceUrl::onUrlStreamReady
            );
            urlStreamCreatorThread.start();
            emit onUrlStreamCreate(
                (sound->getDefaultStreamFlags() &~ BASS_SAMPLE_3D)
                    | BASS_STREAM_RESTRATE | BASS_STREAM_BLOCK | BASS_STREAM_DECODE,
                &fileProcTable,
                this
            );
            break;

        case mse_sus_ReceivingStream:
            urlStreamBuffer.write(netReply->readAll());
            break;

        default:
            return;
    }
}

void MSE_SourceUrl::onUrlStreamReady(HSTREAM newUrlStream)
{
    if(urlStreamCreatorThread.isRunning())
    {
        urlStreamCreatorThread.quit();
        urlStreamCreatorThread.wait();
    }

    if(urlStreamIsClosed)
        return;

    urlStream = newUrlStream;

    if(!urlStream)
    {
        SETERROR(MSE_Object::Err::cannotInitStream, _url.filename);
        closeSock();
        return;
    }
    if(!BASS_Mixer_StreamAddChannel(mixerStream, urlStream, BASS_MIXER_DOWNMIX))
    {
        SETERROR(MSE_Object::Err::mixerAttach, _url.filename);
        closeSock();
        return;
    }
}

MSE_SourceUrl::MSE_SourceUrl(MSE_Playlist *parent) : MSE_Source(parent)
  ,netMan(nullptr)
  ,netReply(nullptr)
  ,retryTimer(nullptr)
  ,timeoutTimer(nullptr)
  ,retriesLeft(0)
  ,mixerStream(0)
  ,urlStream(0)
{
    type = mse_sctRemote;
    fileProcTable.close = fileCloseProc;
    fileProcTable.length = fileLenProc;
    fileProcTable.seek = fileSeekProc;
}

MSE_SourceUrl::~MSE_SourceUrl()
{
    close();
}

HCHANNEL MSE_SourceUrl::open()
{
    curTrackArtist.clear();
    curTrackTitle.clear();
    close();

    state = mse_sus_Ready;
    mixerStream = BASS_Mixer_StreamCreate(
        sound->getEngine()->getInitParams().outputFrequency,
        sound->getEngine()->getInitParams().nChannels,
        sound->getDefaultStreamFlags() | BASS_MIXER_NONSTOP
    );
    if(!mixerStream)
    {
        state = mse_sus_Idle;
        return mixerStream;
    }

    if(!BASS_ChannelSetSync(mixerStream, BASS_SYNC_SETPOS, 0, &MSE_SourceUrl::startProc, this))
    {
        SETERROR(MSE_Object::Err::cannotAddSync);
        state = mse_sus_Idle;
        return mixerStream;
    }

    retriesLeft = maxRetries;
    return mixerStream;
}

bool MSE_SourceUrl::close()
{
    closeSock();
    if(mixerStream)
    {
        BASS_StreamFree(mixerStream);
        mixerStream = 0;
    }
    return true;
}

bool MSE_SourceUrl::getTags(MSE_SourceTags &tags)
{
    if(MSE_Source::getTags(tags))
        return true;

    tags.trackArtist = curTrackArtist;
    tags.trackTitle = curTrackTitle;

    return true;
}

bool MSE_SourceUrl::openUrl(const MSE_PlaylistEntry &urlEntry, int redirectsLeft)
{
    closeSock();
    _url = urlEntry;
    _redirectsLeft = redirectsLeft;
    if(!_redirectsLeft)
    {
        SETERROR(MSE_Object::Err::tooManyRedirects);
        return false;
    }
    QNetworkRequest req;
    QUrl urlObj(urlEntry.filename);
    if(!urlObj.isValid())
    {
        SETERROR(MSE_Object::Err::urlInvalid);
        return false;
    }
    req.setUrl(urlObj);
    req.setRawHeader("icy-metadata", "1");
    req.setRawHeader("Accept", "*/*");
    req.setRawHeader("User-Agent", sound->getEngine()->getInitParams().userAgent.toUtf8());
    state = mse_sus_WaitingPlaylistHeader;
    // need to create QNetworkAccessManager every time due to some unknown glitch
    // that may cause QNetworkAccessManager to stall
    netMan = new QNetworkAccessManager;
    netReply = netMan->get(req);
    connect(netReply, SIGNAL(readyRead()), SLOT(onSockData()));
    connect(netReply, SIGNAL(finished()), SLOT(onSockDone()));
    connect(netReply, SIGNAL(metaDataChanged()), SLOT(onSockHeaders()));
    connect(netReply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(onSockError(QNetworkReply::NetworkError)));

    timeoutTimer = new QTimer;
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(timeoutInterval);
    connect(timeoutTimer, &QTimer::timeout, [this](){
        tryRestartUrl();
    });
    timeoutTimer->start();

    return true;
}
