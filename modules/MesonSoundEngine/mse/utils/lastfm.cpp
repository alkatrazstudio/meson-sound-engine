/****************************************************************************}
{ lastfm.cpp - Last.FM scrobbler                                             }
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

#include "mse/utils/lastfm.h"
#include "simplecrypt.h"

#include <QDesktopServices>

const int MSE_Lastfm::errNet = -1;
const int MSE_Lastfm::errParse = -2;
const int MSE_Lastfm::errNotLoggedIn = -3;
const int MSE_Lastfm::maxScrobbles = 50;
const int MSE_Lastfm::maxQueue = 5000;

const QString& MSE_Lastfm::apiURL("https://ws.audioscrobbler.com/2.0/");
const QString& MSE_Lastfm::authURL("https://www.last.fm/api/auth/");

/*!
 * Creates a scrobbler for a specified sound object.
 */
MSE_Lastfm::MSE_Lastfm(MSE_Sound *parent) : MSE_Object(parent)
  ,sound(parent)
  ,state(mse_lfmIdle)
  ,isScrobbleAllowed(true)
  ,crypt(nullptr)
{
    skTimer.setSingleShot(false);
    connect(&skTimer, SIGNAL(timeout()), SLOT(retrieveWebSessionKey()));

    connect(sound, &MSE_Sound::onPositionChange, [this]{
        isScrobbleAllowed = false;
    });
    connect(sound, &MSE_Sound::onInfoChange, [this]{
        isScrobbleAllowed =
            sound->isTrackArtistFromTags() &&
            sound->isTrackTitleFromTags() &&
            (sound->getType() != mse_sctModule) &&
            (sound->getType() != mse_sctRecord) &&
            (
                (sound->getTrackDuration() >= initParams.minTrackDuration) ||
                (sound->getTrackDuration() < 0)
            );
    });
}

MSE_Lastfm::~MSE_Lastfm()
{
    delete crypt;
}

/*!
 * Gets the default initialization parameters.
 *
 * Refer to MSE_LastfmInitParams for the information about initialization parameters.
 *
 * \sa getDefaultInitParams
 */
void MSE_Lastfm::getDefaultInitParams(MSE_LastfmInitParams &params)
{
    params.altScrobblePos = 30;
    params.apiKey.clear();
    params.cacheFile.clear();
    params.cacheKey = 0;
    params.minTrackDuration = 30;
    params.nowPlayingPos = 10;
    params.scrobblePos = -10;
    params.sessionRetries = 30;
    params.sessionRetryInterval = 10;
    params.sharedSecret.clear();
}

/*!
 * Initializes the scrobbler.
 *
 * If params not specified, then the object will be initialized with a default parameters.
 *
 * Refer to MSE_LastfmInitParams for the information about initialization parameters.
 *
 * \sa getDefaultInitParams
 */
bool MSE_Lastfm::init(MSE_LastfmInitParams *params)
{
    MSE_LastfmInitParams p;
    if(params)
    {
        p = *params;
    }
    else
    {
        getDefaultInitParams(p);
    }

    initParams = p;

    scrobbleCallback = sound->installPositionCallback(initParams.scrobblePos, MSE_Lastfm::onPosSync, this);
    altScrobbleCallback = sound->installPositionCallback(initParams.altScrobblePos, MSE_Lastfm::onPosSync, this);
    nowPlayingCallback = sound->installPositionCallback(initParams.nowPlayingPos, MSE_Lastfm::onPosSync, this);

    skTimer.setInterval(initParams.sessionRetryInterval * 1000);

    if(initParams.cacheKey)
    {
        crypt = new SimpleCrypt(initParams.cacheKey);
        crypt->setCompressionMode(SimpleCrypt::CompressionNever);
        crypt->setIntegrityProtectionMode(SimpleCrypt::ProtectionHash);
    }

    loadCache();

    return true;
}

void MSE_Lastfm::onPosSync(MSE_SoundPositionCallback *callback)
{
    static_cast<MSE_Lastfm*>(callback->getData())->_onPosSync(callback);
}

void MSE_Lastfm::_onPosSync(MSE_SoundPositionCallback *callback)
{
    MSE_Sound* sound = callback->getSound();
    if(isScrobbleAllowed)
    {
        const char* member;
        int trackPos = qMax(0, static_cast<int>(sound->getPosition()));
        if(callback == scrobbleCallback)
        {
            member = "onScrobble";
        }
        else
        {
            if(callback == altScrobbleCallback)
            {
                // the alternative callback is only for streams with an undefined length
                // and is only called when a common scrobble callback is negative
                if((sound->getTrackDuration() < 0) && (scrobbleCallback->getPos() < 0))
                {
                    member = "onScrobble";
                }
                else
                {
                    return;
                }
            }
            else
            {
                member = "onNowPlaying";
            }
        }

        // the current function is running in a different thread,
        // but processing needs to be done in the object's thread
        QMetaObject::invokeMethod(this, member, Qt::QueuedConnection,
                                  Q_ARG(const QString&, sound->getTrackArtist()),
                                  Q_ARG(const QString&, sound->getTrackTitle()),
                                  Q_ARG(const QString&, sound->getTags().trackAlbum),
                                  Q_ARG(int, trackPos));
    }
}

/*!
 * Make an asynchronous API call.
 */
void MSE_Lastfm::sendRequest(const QString& method, MSE_LastfmRequestParams &params, MSE_LastfmRequestCallback callback, bool isPost)
{
    params["method"] = method;
    params["api_key"] = initParams.apiKey;

    QList<QString> keys = params.keys();
    std::sort(keys.begin(), keys.end());
    QString sig;
    foreach(const QString& key, keys)
        sig.append(key).append(params.value(key));
    sig.append(initParams.sharedSecret);
    sig = QCryptographicHash::hash(sig.toUtf8(), QCryptographicHash::Md5).toHex();
    params["api_sig"] = sig;
    params["format"] = "json";

    QString query = constructQuery(params);

    QUrl url(apiURL);
    if(!isPost)
        url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, sound->getEngine()->getInitParams().userAgent);
    request.setRawHeader("Connection", "close");

    QNetworkReply* reply;
    if(isPost)
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        QByteArray postData = query.toUtf8();
        request.setHeader(QNetworkRequest::ContentLengthHeader, postData.size());
        reply = netMan.post(request, postData);
    }
    else
    {
        reply = netMan.get(request);
    }
    currentReply = reply;
    connect(reply, &QNetworkReply::finished, [this, callback, reply](){
        reply->deleteLater();

        QNetworkReply::NetworkError err = reply->error();
        if(err != QNetworkReply::NoError)
        {
            QString err = reply->errorString();
            SETERROR(MSE_Object::Err::apiRequest, err);
            callback(errNet, QJsonObject());
            return;
        }
#ifdef DEBUG_MODE
        QString s = QString::fromUtf8(reply->readAll());
        QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8());
#else
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
#endif
        QJsonObject obj = doc.object();
        if(obj.isEmpty())
        {
            SETERROR(MSE_Object::Err::apiRequest);
            callback(errParse, obj);
            return;
        }

        bool ok;

        int lfmErr = obj.value("error").toVariant().toInt(&ok);
        if(lfmErr && ok)
        {
            QString err = obj.value("message").toString();
            SETERROR(MSE_Object::Err::apiRequest, err);
            callback(lfmErr, obj);
            return;
        }

        callback(0, obj);
    });
}

bool MSE_Lastfm::sendWriteRequest(const QString &method, MSE_LastfmRequestParams &params, MSE_LastfmRequestCallback callback)
{
    if(sessionKey.isEmpty())
        return false;
    params["sk"] = sessionKey;
    sendRequest(method, params, callback, true);
    return true;
}

void MSE_Lastfm::closeCurrentRequest()
{
    if(currentReply)
    {
        currentReply->disconnect();
        currentReply->close();
        currentReply->deleteLater();
    }
}

void MSE_Lastfm::moveToIdleState()
{
    skTimer.stop();
    closeCurrentRequest();
    requestToken.clear();
    sessionKey.clear();
    //userName = "";
    setState(mse_lfmIdle);
}

void MSE_Lastfm::setState(MSE_LastfmState newState)
{
    if(newState == state)
        return;
    state = newState;
    emit onStateChange();
}

bool MSE_Lastfm::saveCache()
{
    if(initParams.cacheFile.isEmpty())
        return true;

    QSaveFile f;
    f.setFileName(initParams.cacheFile);
    CHECK(f.open(QIODevice::WriteOnly), MSE_Object::Err::openWriteFail, initParams.cacheFile);

    QJsonObject obj;
    obj.insert("username", userName);
    obj.insert("session_key", sessionKey);
    QJsonArray arr;
    foreach(const MSE_LastfmTrackEntry& entry, queue)
    {
        QJsonObject item;
        item.insert("artist", entry.artist);
        item.insert("track", entry.track);
        item.insert("album", entry.album);
        item.insert("timestamp", entry.timestamp);
        arr.append(item);
    }
    obj.insert("tracks", arr);
    QJsonDocument doc(obj);
    QByteArray data = doc.toBinaryData();

    if(crypt)
    {
        data = crypt->encryptToByteArray(data);
        if(crypt->lastError() != SimpleCrypt::ErrorNoError)
        {
            f.cancelWriting();
            f.commit();
            SETERROR(MSE_Object::Err::encryptError);
            return false;
        }
    }

    f.write(data);
    CHECK(f.commit(), MSE_Object::Err::writeError, initParams.cacheFile);
    return true;
}

bool MSE_Lastfm::loadCache()
{
    if(initParams.cacheFile.isEmpty())
        return true;

    QFile f(initParams.cacheFile);
    if(!f.exists())
        return true;
    CHECK(f.open(QIODevice::ReadOnly), MSE_Object::Err::readError, initParams.cacheFile);
    QByteArray data = f.readAll();
    f.close();
    CHECK(!data.isEmpty(), MSE_Object::Err::readError, initParams.cacheFile);
    if(crypt)
    {
        data = crypt->decryptToByteArray(data);
        CHECK(crypt->lastError() == SimpleCrypt::ErrorNoError, MSE_Object::Err::decryptError, initParams.cacheFile);
    }

    QJsonDocument doc = QJsonDocument::fromBinaryData(data);
    QJsonObject obj = doc.object();
    CHECK(!obj.isEmpty(), MSE_Object::Err::readError, initParams.cacheFile);

    QString _sessionKey = obj.value("session_key").toString();
    QString _userName = obj.value("username").toString();
    if(!_sessionKey.isEmpty() && !_userName.isEmpty())
    {
        sessionKey = _sessionKey;
        if(!userName.isEmpty() && userName != _userName)
            queue.clear();
        userName = _userName;
        QJsonArray arr = obj.value("tracks").toArray();
        foreach(const QJsonValue& val, arr)
        {
            MSE_LastfmTrackEntry entry;
            QJsonObject item = val.toObject();
            entry.artist = item.value("artist").toString();
            entry.track = item.value("track").toString();
            entry.album = item.value("album").toString();
            entry.timestamp = item.value("timestamp").toString();
            queue.append(entry);
        }
        setState(mse_lfmLoggedIn);
    }
    return true;
}

QString MSE_Lastfm::constructQuery(const MSE_LastfmRequestParams &params)
{
    QUrlQuery query;
    foreach(const QString& key, params.keys())
        query.addQueryItem(key, params.value(key));
    QString result(query.toString(QUrl::FullyEncoded));
    result.replace("+", "%2B"); // QTBUG-10146
    return result;
}

void MSE_Lastfm::retrieveSessionKeyCommon(bool isMobile, MSE_LastfmRequestParams &request)
{
    closeCurrentRequest();
    sessionKey.clear();
    setState(mse_lfmGetSession);
    QString method;

    if(isMobile)
    {
        method = "auth.getMobileSession";
    }
    else
    {
        request["token"] = requestToken;
        method = "auth.getSession";
    }

    sendRequest(method, request, [this, isMobile](int err, const QJsonObject& obj){
        if(isMobile)
        {
            // no errors allowed in mobile version
            if(err)
            {
                moveToIdleState();
                return;
            }
        }
        else
        {
            switch(err)
            {
                case 4: // Invalid authentication token supplied
                case 15: // This token has expired
                    moveToIdleState();
                    return; // these errors will prevent the app from trying any further

                case 0:
                    break;

                default:
                    skAttemptsLeft--;
                    if(!skAttemptsLeft)
                        moveToIdleState();
                    return; // the function will be called again, unless no attempts left
            }
        }

        QJsonObject objSession = obj.value("session").toObject();
        if(objSession.isEmpty())
        {
            SETERROR(MSE_Object::Err::apiRequest);
            moveToIdleState();
            return;
        }

        sessionKey = objSession.value("key").toString().trimmed();
        if(sessionKey.isEmpty())
        {
            SETERROR(MSE_Object::Err::apiRequest);
            moveToIdleState();
            return;
        }

        QString _userName = objSession.value("name").toString().trimmed();
        if(!userName.isEmpty() && userName != _userName)
            queue.clear();
        userName = _userName;

        if(!isMobile)
            skTimer.stop(); // we got the sessionKey, so stop trying to retreive web session
        saveCache();
        setState(mse_lfmLoggedIn);
    }
    ,isMobile // mobile auth requires POST
    );
}

/*!
 * Start the authentication process (web version).
 * This will launch the system browser.
 * User has a limited time to authorize the application.
 * If a user is logged in, then they will be logged off first.
 */
void MSE_Lastfm::startWebAuth()
{
    logout();
    setState(mse_lfmGetToken);

    MSE_LastfmRequestParams request;
    sendRequest("auth.getToken", request, [this](int err, const QJsonObject& obj){
        if(err)
        {
            moveToIdleState();
            return;
        }

        requestToken = obj.value("token").toString().trimmed();
        if(requestToken.isEmpty())
        {
            SETERROR(MSE_Object::Err::apiRequest);
            moveToIdleState();
            return;
        }

        QUrl url(authURL);
        QUrlQuery query;
        query.addQueryItem("api_key", initParams.apiKey);
        query.addQueryItem("token", requestToken);
        url.setQuery(query);
        if(!QDesktopServices::openUrl(url))
        {
            SETERROR(MSE_Object::Err::cannotOpenBrowser);
            moveToIdleState();
            return;
        }

        skAttemptsLeft = initParams.sessionRetries;
        skTimer.start();
    });
}

/*!
 * Start the authentication process (mobile version).
 * If a user is logged in, then they will be logged off first.
 */
void MSE_Lastfm::startMobileAuth(const QString &username, const QString &password)
{
    logout();
    retrieveMobileSessionKey(username, password);
}

/*!
 * Logs the user out.
 * Scrobbling data won't be transmitted until user logs in again.
 */
void MSE_Lastfm::logout()
{
    moveToIdleState();
    //queue.clear();
    saveCache();
}

void MSE_Lastfm::onScrobble(
        const QString& artist,
        const QString& title,
        const QString& album,
        int trackPos)
{
    if(sessionKey.isEmpty())
        return;

    // add a track to a queue;
    MSE_LastfmTrackEntry entry;
    entry.artist = artist;
    entry.track = title;
    entry.album = album;
    entry.timestamp = QString::number(QDateTime::currentDateTimeUtc().toTime_t() - trackPos);
    queue.append(entry);

    while(queue.size() > maxQueue)
        queue.removeFirst();

    MSE_LastfmRequestParams params;
    int n = qMin(maxScrobbles, queue.size());
    for(int a=0; a<n; a++)
    {
        QString index = QStringLiteral("[")+QString::number(a)+"]";
        const MSE_LastfmTrackEntry& entry = queue.at(a);
        params["artist"+index] = entry.artist;
        params["track"+index] = entry.track;
        params["album"+index] = entry.album;
        params["timestamp"+index] = entry.timestamp;
    }

    sendWriteRequest("track.scrobble", params, [this, n](int err, const QJsonObject& obj){
        Q_UNUSED(obj);

        switch(err)
        {
            case 4: // Authentication Failed
            case 9: // Invalid session key - Please re-authenticate
                logout();
                return; // these errors will log the user off

            case 13: // Invalid method signature supplied
                // This error can signify url encoding failure.
                if(n == queue.size())
                {
                    // If the whole queue was submitted,
                    // then the last track is what causes the problem.
                    // So remove it.
                    queue.removeLast();
                    if(n > 1)
                        saveCache();
                }
                else
                {
                    // Otherwise there's no reliable way to determine which track causes it,
                    // so the whole bunch needs to be cleared to prevent further consequent errors.
                    // The error is ignored then as if all tracks were processed (see below).
                }
                break;

            case 0:
                break; // no error

            default: // other errors, including network problems
                // save queue (which is now holding the current track as well)
                saveCache();
                return;
        }

        // remove scrobbled tracks from cache
        queue = queue.mid(n);
        if(n > 1)
            saveCache();
    });
}

void MSE_Lastfm::onNowPlaying(
        const QString& artist,
        const QString& title,
        const QString& album,
        int trackPos)
{
    Q_UNUSED(trackPos);
    MSE_LastfmRequestParams params;
    params.insert("artist", artist);
    params.insert("track", title);
    params.insert("album", album);

    sendWriteRequest("track.updateNowPlaying", params, [this](int err, const QJsonObject& obj){
        Q_UNUSED(err);
        Q_UNUSED(obj);
        switch(err)
        {
            case 4: // Authentication Failed
            case 9: // Invalid session key - Please re-authenticate
                logout();
                return; // these errors will log the user off

            default:
                return; // ignore all other errors
        }
    });
}

/*!
 * This function tries to retreive a web session key.
 * The session key can only be retreived if a user authorized this application in browser.
 */
void MSE_Lastfm::retrieveWebSessionKey()
{
    MSE_LastfmRequestParams request;
    retrieveSessionKeyCommon(false, request);
}

/*!
 * This function tries to retreive a mobile session key.
 */
void MSE_Lastfm::retrieveMobileSessionKey(const QString &username, const QString &password)
{
    MSE_LastfmRequestParams request;
    request["username"] = username;
    request["password"] = password;
    retrieveSessionKeyCommon(true, request);
}
