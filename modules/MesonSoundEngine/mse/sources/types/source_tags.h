#pragma once

#include <QString>

struct MSE_SourceTags {
    QString trackArtist;
    QString trackTitle;
    QString trackAlbum;
    QString trackDate;
    QString nTracks;
    QString trackIndex;
    QString nDiscs;
    QString discIndex;
    QString genre;

    void clear();
    static QString clean(const QString& s);
    void clean();
};
