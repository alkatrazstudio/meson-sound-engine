#include "source_tags.h"

void MSE_SourceTags::clear()
{
    trackArtist.clear();
    trackTitle.clear();
    trackAlbum.clear();
    trackDate.clear();
    nTracks.clear();
    trackIndex.clear();
    nDiscs.clear();
    discIndex.clear();
    genre.clear();
}

QString MSE_SourceTags::clean(const QString& s)
{
    return QString::fromUtf8(s.toUtf8().data()).trimmed();
}

void MSE_SourceTags::clean()
{
    trackArtist = clean(trackArtist);
    trackTitle = clean(trackTitle);
    trackAlbum = clean(trackAlbum);
    trackDate = clean(trackDate);
    nTracks = clean(nTracks);
    trackIndex = clean(trackIndex);
    nDiscs = clean(nDiscs);
    discIndex = clean(discIndex);
    genre = clean(genre);
}
