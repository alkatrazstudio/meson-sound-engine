/****************************************************************************}
{ lastfm.h - Last.FM scrobbler                                               }
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

#include "mse/types.h"
#include "mse/sound.h"
#include "simplecrypt.h"

#include <QNetworkReply>

/*!
 * Query parameters for a LastFM API call.
 */
typedef QHash<QString, QString> MSE_LastfmRequestParams;

typedef std::function<void(int, const QJsonObject&)> MSE_LastfmRequestCallback;

/*!
 * A status of MSE_Lastfm object.
 */
enum MSE_LastfmState {
    mse_lfmIdle, /*!< User is not logged in. */
    mse_lfmGetToken, /*!< getToken request is sent, but the response is not received yet. */
    mse_lfmGetSession, /*!<
    Trying to fetch a service session.
    Retries will be aborted after a certain amount of time.

    \sa MSE_LastfmInitParams::sessionRetryInterval, MSE_LastfmInitParams::sessionRetries
*/
    mse_lfmLoggedIn /*!< The user is logged in. */
};

/*!
 * Parameters for MSE_Lastfm initialization.
 */
struct MSE_LastfmInitParams {
    QString apiKey; /*!<
    The API key.

    **Default**: &lt;empty&gt;
*/
    QString sharedSecret; /*!<
    The shared secret.

    **Default**: &lt;empty&gt;
*/
    double scrobblePos; /*!<
    Defines a track position at which a scrobbling request should be executed.
    See MSE_SoundPositionCallback for more info.

    **Default**: -10
*/
    double altScrobblePos; /*!<
    Same as ::scrobblePos, but only applies when a stream length cannot be determined
    and ::scroblePos is negative.

    This value MUST be a positive number.

    **Default**: 30
*/
    double nowPlayingPos; /*!<
    Defines a track position at which a nowPlaying request should be executed.
    See MSE_SoundPositionCallback for more info.

    **Default**: 10
*/
    quint8 sessionRetryInterval; /*!<
    After running MSE_Lastfm::startAuth(), the browser will be launched to authenticate the user.
    This way the MSE_Lastfm object won't be able to determine
    if the user has finished the authentication process or not.
    So the scrobbler will be trying to get a service session every *sessionRetryInterval* seconds.

    **Default**: 10

    \sa sessionRetries
*/
    quint32 sessionRetries; /*!<
    The number of retries the scrobbler will make in attempt to retreive a service session.

    **Default**: 30

    \sa sessionRetryInterval
*/
    QString cacheFile; /*!<
    This directory will store an information about the current service session
    and tracks to be scrobbled (if they weren't scrobbled from the first attempt).
    Leave empty to disable this cache.
    You can also encrypt this file using ::cacheKey.

    **Default**: &lt;empty&gt;
*/

    quint64 cacheKey; /*!<
    Use this key to encrypt ::cacheFile.
    Set to zero to disable encryption.

    **Default**: 0
*/

    int minTrackDuration; /*!<
    Tracks that are shorter than *minTrackDuration* seconds
    will not be scrobbled neither they update nowPlaying status.

    **Default**: 20
*/
};

struct MSE_LastfmTrackEntry {
    QString artist;
    QString track;
    QString album;
    QString timestamp;
};
typedef QList<MSE_LastfmTrackEntry> MSE_LastfmTrackEntries;

/*!
 * Last.fm scrobbler
 */
class MSE_Lastfm : public MSE_Object
{
    Q_OBJECT
public:
    explicit MSE_Lastfm(MSE_Sound *parent);
    ~MSE_Lastfm();

    void getDefaultInitParams(MSE_LastfmInitParams& params) const;

    bool init(MSE_LastfmInitParams* params = nullptr);

    /*!
     * Returns parameters the sound object was initialized with.
     *
     * \note These parameters can be slightly different to those passed to init() function.
     */
    inline const MSE_LastfmInitParams& getInitParams() const {return initParams;}

    /*!
     * Returns the corresponding MSE_SOund object.
     */
    inline MSE_Sound* getSound() const {return sound;}

    /*!
     * Returns a nickname of a currently logged in user.
     */
    inline const QString& getUserName() const {return userName;}

    /*!
     * Returns a current state of the object.
     */
    inline MSE_LastfmState getState() const {return state;}

    void startWebAuth();
    void startMobileAuth(const QString& username, const QString& password);
    void logout();

protected:
    static void onPosSync(MSE_SoundPositionCallback* callback);
    void _onPosSync(MSE_SoundPositionCallback* callback);
    void sendRequest(const QString &method, MSE_LastfmRequestParams& params, MSE_LastfmRequestCallback callback, bool isPost = false);
    bool sendWriteRequest(const QString &method, MSE_LastfmRequestParams& params, MSE_LastfmRequestCallback callback);
    void closeCurrentRequest();
    void moveToIdleState();
    void setState(MSE_LastfmState newState);
    bool saveCache();
    bool loadCache();
    QString constructQuery(const MSE_LastfmRequestParams& params);
    void retrieveSessionKeyCommon(bool isMobile, MSE_LastfmRequestParams &request);

    MSE_Sound* sound;
    MSE_LastfmInitParams initParams;
    MSE_LastfmState state;
    MSE_SoundPositionCallback* scrobbleCallback;
    MSE_SoundPositionCallback* altScrobbleCallback;
    MSE_SoundPositionCallback* nowPlayingCallback;
    QPointer<QNetworkReply> currentReply;
    QString requestToken;
    QString sessionKey;
    QNetworkAccessManager netMan;
    QTimer skTimer;
    bool isScrobbleAllowed;
    int skAttemptsLeft;
    QString userName;
    MSE_LastfmTrackEntries queue;
    SimpleCrypt* crypt;

    static const QString& apiURL;
    static const QString& authURL;

    static const int errNet;
    static const int errParse;
    static const int errNotLoggedIn;
    static const int maxScrobbles;
    static const int maxQueue;

protected slots:
    void onScrobble(const QString& artist, const QString& title, const QString& album, int trackPos);
    void onNowPlaying(const QString& artist, const QString& title, const QString& album, int trackPos);
    void retrieveWebSessionKey();
    void retrieveMobileSessionKey(const QString& username, const QString& password);

signals:
    void onStateChange();
};
