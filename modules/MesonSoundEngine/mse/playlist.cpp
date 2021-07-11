/****************************************************************************}
{ playlist.cpp - playlist management                                         }
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

#include "mse/playlist.h"
#include "mse/sources/source.h"
#ifdef MSE_MODULE_SOURCE_URL
#include "mse/sources/source_url.h"
#endif
#include "mse/sources/source_stream.h"
#include "mse/sources/source_module.h"
#include "mse/sources/source_plugin.h"

#include "qiodevicehelper.h"

const int MSE_Playlist::detectLength = 50;

/*!
 * Creates a MSE_Playlist instance.
 *
 * Normally you don't need to create MSE_Playlist object.
 * Instead, use MSE_Sound::getPlaylist() to fetch a reference to a playlist for a sound object.
 */
MSE_Playlist::MSE_Playlist(MSE_Sound* parent):MSE_Object(parent)
{
    engine = MSE_Engine::getInstance();
    sound = parent;
    history = new MSE_Sources();
    index = -1;
    historyIndex = -1;
    currentSource = nullptr;
    playbackMode = mse_ppmAllLoop;
}

/*!
 * Destroys a MSE_Playlist instance.
 */
MSE_Playlist::~MSE_Playlist()
{
    clear();
    delete history;
}

/*!
 * Retrieves a list of *count* sound sources that will be played next.
 * Note, that a result may contain multiple entries of a same sound source.
 * For example, if a playback mode is mse_ppmTrackLoop, then a result will hold
 * *count* identical items.
 *
 * \note If a playback mode is mse_ppmTrackOnce, then a result will be empty.
 *
 * \note *nextList* is cleared first.
 */
void MSE_Playlist::getNextSources(QList<const MSE_Source*> &nextList, int count)
{
    nextList.clear();
    int a;
    if(queue.isEmpty())
        a = index;
    else
        a = queue.last()->index;

    if(a < 0)
        return;
    int n = playlist.size();
    if(a >= n)
        return;

    MSE_Source* src;

    switch(playbackMode)
    {
        case mse_ppmTrackOnce:
            return;

        case mse_ppmTrackLoop:
            src = playlist.at(a);
            for(a=0; a<count; a++)
                nextList.append(src);
            return;

        case mse_ppmAllOnce:
            a++;
            n = n - a;
            if(count > n)
                count = n;
            while(count)
            {
                nextList.append(playlist.at(a));
                count--;
                a++;
            }
            return;

        case mse_ppmAllLoop:
            while(count)
            {
                a++;
                if(a == n)
                    a = 0;
                nextList.append(playlist.at(a));
                count--;
            }
            return;

        case mse_ppmRandom:
            a = historyIndex;
            while(count)
            {
                a++;
                if(a == history->size())
                    appendHistoryShuffle();
                nextList.append(history->at(a));
                count--;
            }
            return;
    }
}

/*!
 * Appends a sound source with a specified index into a playback queue.
 *
 * \sa insertIntoQueue, removeFromQueue
 */
bool MSE_Playlist::appendToQueue(int index)
{
    CHECK((index >= 0) && (index < playlist.size()), MSE_Object::Err::outOfRange);
    queue.append(playlist.at(index));
    return true;
}

/*!
 * Inserts a sound source with a specified index into a playback queue at a specified position.
 *
 * \sa appendToQueue, removeFromQueue
 */
bool MSE_Playlist::insertIntoQueue(int index, int pos)
{
    CHECK((index >= 0) && (index < playlist.size()), MSE_Object::Err::outOfRange);
    if(pos < 0)
    {
        pos = 0;
    }
    else
    {
        if(pos > queue.size())
            pos = queue.size();
    }
    queue.insert(pos, playlist.at(index));
    return true;
}

/*!
 * Removes a sound source form a queue.
 * Here, *index* specifies the index inside a queue.
 */
bool MSE_Playlist::removeFromQueue(int index)
{
    CHECK((index >= 0) && (index < playlist.size()), MSE_Object::Err::outOfRange);
    queue.removeAt(index);
    return true;
}

/*!
 * Removes a single entry of a sound source form a queue.
 * Here, *sourceIndex* specifies the index inside a playlist.
 */
bool MSE_Playlist::removeSourceFromQueue(int sourceIndex)
{
    CHECK((sourceIndex >= 0) && (sourceIndex < playlist.size()), MSE_Object::Err::outOfRange);
    return removeSourceFromQueue(playlist.at(sourceIndex));
}

/*!
 * Removes all entries of a sound source form a queue.
 * Here, *sourceIndex* specifies the index inside a playlist.
 */
bool MSE_Playlist::removeAllSourcesFromQueue(int sourceIndex)
{
    CHECK((sourceIndex >= 0) && (sourceIndex < playlist.size()), MSE_Object::Err::outOfRange);
    return removeAllSourcesFromQueue(playlist.at(sourceIndex));
}

/*!
 * Adds a file to a playlist.
 * Returns false if a file cannot be found or has an unsupported sound file format.
 */
bool MSE_Playlist::addFile(const MSE_PlaylistEntry &entry)
{
    MSE_Source* src = playlistEntryToSource(entry);
    if(!src)
        return false;

    addToPlaylistRaw(src);
    return true;
}

/*!
 * Adds a file to a playlist.
 * Returns false if a specified URL is invalid.
 */
bool MSE_Playlist::addUrl(const MSE_PlaylistEntry& urlEntry)
{
    MSE_Source* src = playlistEntryToSource(urlEntry);
    if(src->type != mse_sctRemote)
    {
        delete src;
        SETERROR(MSE_Object::Err::notURL, urlEntry.filename);
        return false;
    }
    addToPlaylistRaw(src);
    return true;
}

/*!
 * Adds files from a directory.
 * Returns the number of files successfully added.
 * Use sourceLoadFlags to change a traversal behavior.
 *
 * \note mse_slmSkipDirs flag is ignored here.
 *
 * \sa MSE_SourceLoadFlags
 */
int MSE_Playlist::addFromDirectory(const QString &dirname, MSE_SourceLoadFlags sourceLoadFlags)
{
    QString dName = MSE_Utils::normalizeUri(dirname);
    QDir dir;
    dir.setPath(dName);
    CHECK(dir.exists(), MSE_Object::Err::pathNotFound, dirname)
    QString fullDirname = dir.canonicalPath();
    CHECK(!fullDirname.isEmpty(), MSE_Object::Err::cannotGetCanonicalPath, dirname)
    fullDirname += "/";
    int result = 0;
    QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);

    QCollator collator;
    collator.setNumericMode(true);
    std::sort(entries.begin(), entries.end(), collator);

    QString entry;
    foreach(entry, entries)
        result += addFromDirectory(fullDirname + entry, sourceLoadFlags);
    entries = dir.entryList(QDir::Files | QDir::Readable);

    QStringList::iterator iBegin = entries.begin();
    QStringList::iterator iEnd = entries.end();
    QString cueBasename;

    for(QStringList::iterator i=iBegin; i!=iEnd; ++i)
    {
        if((*i).endsWith(".cue", Qt::CaseInsensitive))
        {
            cueBasename = (*i).left((*i).size()-4)+".";
            for(QStringList::iterator i2=iBegin; i2!=iEnd; ++i2)
            {
                if(i2 != i)
                    if((*i2).startsWith(cueBasename))
                        *i2 = "";
            }
        }
    }

    if(!sourceLoadFlags.testFlag(mse_slfLoadPlaylists))
        sourceLoadFlags |= mse_slfSkipPlaylists;

    std::sort(entries.begin(), entries.end(), collator);

    foreach(entry, entries)
        if(!entry.isEmpty())
            result += addAnything(MSE_PlaylistEntry(fullDirname + entry), sourceLoadFlags);
    return result;
}

/*!
 * Adds files from a multiple directories.
 * Returns the number of files successfully added.
 * Use sourceLoadFlags to change a traversal behavior.
 *
 * \note mse_slmSkipDirs flag is ignored here.
 *
 * \sa MSE_SourceLoadFlags
 */
int MSE_Playlist::addFromDirectory(const QStringList &dirnames, MSE_SourceLoadFlags sourceLoadFlags)
{
    int result = 0;
    foreach(QString dirname, dirnames)
        result += addFromDirectory(dirname, sourceLoadFlags);
    return result;
}

/*!
 * Adds files from a playlist.
 * Returns the number of files successfully added.
 *
 * Note, that by default all directories specified in the playlist will be added too
 * (unless mse_slfSkipDirs is specified in sourceLoadFlags)
 *
 * \sa MSE_SourceLoadFlags
 */
int MSE_Playlist::addFromPlaylist(const QString &filename, MSE_SourceLoadFlags sourceLoadFlags)
{
    QFileEx f;
    f.setFileName(filename);
    CHECK(f.exists(), MSE_Object::Err::pathNotFound, filename);

    QFileInfo fInfo(filename);
    if(fInfo.suffix().toLower() == "cue")
    {
        MSE_CueSheet* cueSheet = getCueSheet(filename);
        if(!cueSheet)
            return 0;
        MSE_Source* src;
        foreach(MSE_CueSheetTrack* track, cueSheet->tracks)
        {
            src = createSourceFromType(cueSheet->sourceType);
            src->cueSheetTrack = track;
            src->type = cueSheet->sourceType;
            src->entry = MSE_PlaylistEntry(track->sheet->cueFilename+":"+QString::number(track->index));
            addToPlaylistRaw(src);
        }
        return cueSheet->tracks.size();
    }

    CHECK(f.open(QIODevice::ReadOnly), MSE_Object::Err::openFail, filename);

    int result = 0;
    QList<MSE_PlaylistEntry> entries;
    if(!parse(&f, entries))
    {
        f.close();
        SETERROR(MSE_Object::Err::invalidFormat, filename);
        return result;
    }

    QFileInfo info(filename);
    QString curDir = QDir::currentPath();
    QDir::setCurrent(info.canonicalPath()+"/");

    foreach(MSE_PlaylistEntry entry, entries)
        result += addAnything(entry, sourceLoadFlags);
    f.close();

    QDir::setCurrent(curDir+"/");

    return result;
}

/*!
 * Adds files from a multiple playlists.
 * Returns the number of files successfully added.
 *
 * Note, that by default all directories specified in the playlist will be added too
 * (unless mse_slfSkipDirs is specified in sourceLoadFlags)
 *
 * \sa MSE_SourceLoadFlags
 */
int MSE_Playlist::addFromPlaylist(const QStringList &filenames, MSE_SourceLoadFlags sourceLoadFlags)
{
    int result = 0;
    foreach(QString filename, filenames)
        result += addFromPlaylist(filename, sourceLoadFlags);
    return result;
}

/*!
 * Adds a specified file/URL/directory/playlist to the playlist.
 * This function tries to determine the type of specified source,
 * then uses an appropriate function to find and add files.
 *
 * \sa addFile, addURL, addFromDirectory, addFromPlaylist
 */
int MSE_Playlist::addAnything(const MSE_PlaylistEntry& entry, MSE_SourceLoadFlags sourceLoadFlags)
{
    MSE_SoundChannelType type = engine->typeByUri(entry.uri);
    if(type == mse_sctRemote)
        if(addUrl(entry))
            return 1;

    QDir dir;
    dir.setPath(entry.uri);
    if(dir.exists())
    {
        if(sourceLoadFlags.testFlag(mse_slfSkipDirs))
            return 0;
        else
            return addFromDirectory(entry.uri, sourceLoadFlags);
    }

    bool isCue;
    if(hasSupportedExtension(entry.uri, isCue))
    {
        if(sourceLoadFlags.testFlag(mse_slfSkipPlaylists) && !isCue)
            return 0;
        else
            return addFromPlaylist(entry.uri, sourceLoadFlags);
    }

    if(type != mse_sctUnknown)
        if(addFile(entry))
           return 1;

    return 0;
}

/*!
 * Adds a specified files/URLs/directories/playlists to the playlist.
 * This function tries to determine the type of each specified source,
 * then uses an appropriate function to find and add files.
 *
 * \sa addFile, addURL, addFromDirectory, addFromPlaylist
 */
int MSE_Playlist::addAnything(const QList<MSE_PlaylistEntry> &entries, MSE_SourceLoadFlags sourceLoadFlags)
{
    int result = 0;
    foreach(auto entry, entries)
        result += addAnything(entry, sourceLoadFlags);
    return result;
}

/*!
 * Removes all entries from the playlist.
 *
 * \note Calling this function will also close the currently opened sound source if any.
 */
void MSE_Playlist::clear()
{
    sound->close();
    qDeleteAll(playlist.begin(), playlist.end());
    playlist.clear();
    history->clear();
    index = -1;
    currentSource = nullptr;
    historyIndex = -1;
}

/*!
 * Determines a playlist format using the first detectLength bytes from a provided stream.
 * The stream should contain text data in UTF-8 format with or without a BOM.
 * The stream also need to be positioned at the start of a text data.
 * The function will move a read position past BOM if a BOM is found.
 */
MSE_PlaylistFormatType MSE_Playlist::typeByHeader(QIODevice* dev)
{
    if(!skipBOM(dev))
        return mse_pftUnknown;
    QByteArray bytes = dev->peek(detectLength);
    QString s = QString::fromUtf8(bytes.constData());
    if(s.startsWith("#EXTM3U\r") || s.startsWith("#EXTM3U\n") || s.startsWith("#EXTM3U "))
        return mse_pftM3U;
    if(s.startsWith("[playlist]\r") || s.startsWith("[playlist]\n"))
        return mse_pftPLS;
    QRegularExpression rx;
    rx.setPattern("(\\<\\?xml[^\\?]+\\?\\>)?[\\r\\n\\s]*\\<asx[\\s\\>]");
    if(rx.match(s).hasMatch())
        return mse_pftASX;
    rx.setPattern("<?wpl[\\s\\>]");
    if(rx.match(s).hasMatch())
        return mse_pftWPL;
    rx.setPattern("(\\<\\?xml[^\\?]+\\?\\>)?[\\r\\n\\s]*\\<playlist[\\s\\>]");
    if(rx.match(s).hasMatch())
        return mse_pftXSPF;
    return mse_pftUnknown;
}

/*!
 * The function will move a read position past BOM if a BOM is found.
 */
bool MSE_Playlist::skipBOM(QIODevice* dev)
{
    unsigned char bom[3];
    bom[0] = 0;
    bom[1] = 0;
    bom[2] = 0;
    dev->peek(reinterpret_cast<char*>(&bom[0]), 3);
    if(
        (bom[0] == 0xEF)
            &&
        (bom[1] == 0xBB)
            &&
        (bom[2] == 0xBF)
    )
    {
        if(dev->read(reinterpret_cast<char*>(&bom[0]), 3) != 3)
            return false;
    }
    return true;
}

/*!
 * Parses the specified text stream into a *list*.
 *
 * \note All parsed items will be appended to a *list*.
 */
bool MSE_Playlist::parse(QIODevice* dev, QList<MSE_PlaylistEntry> &list)
{
    MSE_PlaylistFormatType pType = typeByHeader(dev);
    switch(pType)
    {
        case mse_pftASX:
            return parseASX(dev, list);

        case mse_pftM3U:
            return parseM3U(dev, list);

        case mse_pftXSPF:
            return parseXSPF(dev, list);

        case mse_pftPLS:
            return parsePLS(dev, list);

        case mse_pftWPL:
            return parseWPL(dev, list);

        default:
            return false;
    }
}

/*!
 * Parses a playlist with a specified filename into a *list*.
 *
 * \note All parsed items will be appended to a *list*.
 */
bool MSE_Playlist::parse(const QString &filename, QList<MSE_PlaylistEntry>& playlist)
{
    QFileEx f;
    f.setFileName(filename);
    if(!f.open(QIODevice::ReadOnly))
        return false;
    bool result = parse(&f, playlist);
    f.close();
    return result;
}

/*!
 * Returns a single sound source entry for a specified playlist entry.
 * Note, that you can only pass entries of sound files to this function.
 * The filenames however can be in format &lt;filename&gt;.cue:&lt;index&gt;
 * which means that a sound source is a part of a CUE sheet.
 */
MSE_Source *MSE_Playlist::playlistEntryToSource(const MSE_PlaylistEntry &entry)
{
    MSE_Source* source;
    MSE_SoundChannelType chType;

    if(entry.cueIndex < 0)
    {
        chType = engine->typeByUri(entry.uri);
        source = createSourceFromType(chType);
        if(!source)
            return source;
        source->entry = entry;
        source->cueSheetTrack = nullptr;
        source->type = chType;
        return source;
    }

    MSE_CueSheet* cueSheet = getCueSheet(entry.filename);

    if(!cueSheet)
        return nullptr;
    CHECKP(entry.cueIndex < cueSheet->tracks.size(), MSE_Object::Err::cueIndexOutOfRange);

    chType = engine->typeByUri(cueSheet->dataSourceFilename);
    source = createSourceFromType(chType);
    if(!source)
        return source;
    source->entry = entry;
    source->cueSheetTrack = cueSheet->tracks.at(entry.cueIndex);
    source->type = chType;

    return source;
}

/*!
 * Creates an empty MSE_Source descendant object for a specified channel type.
 */
MSE_Source *MSE_Playlist::createSourceFromType(MSE_SoundChannelType type)
{
    switch(type)
    {
        case mse_sctStream:
            return new MSE_SourceStream(this);

        case mse_sctModule:
            return new MSE_SourceModule(this);

        case mse_sctRemote:
#ifdef MSE_MODULE_SOURCE_URL
            return new MSE_SourceUrl(this);
#else
            return nullptr;
#endif

        case mse_sctPlugin:
            return new MSE_SourcePlugin(this);

        default:
            return nullptr;
    }
}

/*!
 * Returns a corresponding MSE_CueSheet for a specified filename.
 * This function uses the internal cache of CUE sheets data,
 * so one file won't be parsed multiple times.
 *
 * Returns nullptr if a CUE sheet cannot be parsed.
 */
MSE_CueSheet* MSE_Playlist::getCueSheet(const QString &filename)
{
    foreach(MSE_CueSheet* cueSheet, cueSheetsCache)
    {
        if(cueSheet->cueFilename == filename)
        {
            if(cueSheet->isValid)
                return cueSheet;
            return nullptr;
        }
    }

    QFileEx f;
    f.setFileName(filename);
    CHECKP(f.open(QIODevice::ReadOnly), MSE_Object::Err::openFail, filename);
    skipBOM(&f);

    QRegularExpression rxPerformer("^\\s*PERFORMER\\s*\\\"([^\\\"]*)\\\"\\s*$");
    QRegularExpression rxTitle("^\\s*TITLE\\s*\\\"([^\\\"]*)\\\"\\s*$");
    QRegularExpression rxTrack("^\\s*TRACK\\s*(\\d+)\\s*AUDIO\\s*$");
    QRegularExpression rxIndex("^\\s*INDEX\\s*0?1\\s*(\\d+)\\:(\\d+)\\:(\\d+)\\s*$");
    QRegularExpression rxDate("^REM\\s*DATE\\s*\\\"?(.*?)\\\"?\\s*$");

    QString cuePerformer;
    QString cueTitle;
    MSE_CueSheetTrack* cueTrack = nullptr;
    QString s;
    MSE_CueSheet* theCueSheet = new MSE_CueSheet;
    theCueSheet->cueFilename = filename;
    int p;

    while(!f.atEnd())
    {
        s = f.readLineUTF8();
        QRegularExpressionMatch match = rxPerformer.match(s);
        if(match.hasMatch())
        {
            if(!cueTrack)
                cuePerformer = match.captured(1).trimmed();
            else
                cueTrack->performer = match.captured(1).trimmed();
            continue;
        }
        match = rxTitle.match(s);
        if(match.hasMatch())
        {
            if(!cueTrack)
                cueTitle = match.captured(1).trimmed();
            else
                cueTrack->title = match.captured(1).trimmed();
            continue;
        }
        match = rxTrack.match(s);
        if(match.hasMatch())
        {
            p = match.captured(1).toInt()-1;
            if(p != theCueSheet->tracks.size())
            {
                theCueSheet->isValid = false;
                cueSheetsCache.append(theCueSheet);
                SETERROR(MSE_Object::Err::cueIndexLost, filename);
                return nullptr;
            }
            cueTrack = new MSE_CueSheetTrack;
            cueTrack->index = p;
            cueTrack->startPos = 0;
            cueTrack->endPos = 0;
            cueTrack->sheet = theCueSheet;
            cueTrack->performer = cuePerformer;
            cueTrack->title = cueTitle;
            theCueSheet->tracks.append(cueTrack);
            continue;
        }
        match = rxIndex.match(s);
        if(match.hasMatch())
        {
            if(cueTrack)
            {
                cueTrack->startPos =
                        match.captured(1).toInt()*60+
                        match.captured(2).toInt()+
                        match.captured(3).toDouble()/75.0;
                if(cueTrack->index >= 1)
                    theCueSheet->tracks[cueTrack->index-1]->endPos = cueTrack->startPos;
            }
            continue;
        }
        match = rxDate.match(s);
        if(match.hasMatch())
        {
            theCueSheet->date = match.captured(1);
            continue;
        }
    }
    f.close();

    if(!setSourceDataForCueSheet(theCueSheet))
    {
        theCueSheet->isValid = false;
        cueSheetsCache.append(theCueSheet);
        return nullptr;
    }

    theCueSheet->title = cueTitle;
    theCueSheet->isValid = true;
    cueSheetsCache.append(theCueSheet);
    return theCueSheet;
}

/*!
 * Searches for a playlist entry by its URI and returns its index or -1 if nothing is found.
 *
 * \sa MSE_SoundSource::getPlaylistUri
 */
int MSE_Playlist::indexOfUri(const QString &uri)
{
    MSE_Sources::iterator iBegin = playlist.begin();
    MSE_Sources::iterator iEnd = playlist.end();
    int a = 0;
    for(MSE_Sources::iterator i = iBegin; i != iEnd; ++i)
    {
        if((*i)->getPlaylistUri() == uri)
            return a;
        a++;
    }
    return -1;
}

/*!
 * Shuffles a playlist.
 * A current *index* will be cahnged after this function call.
 */
void MSE_Playlist::shuffle()
{
    int n = playlist.size();
    if((n == 0) || (n == 1))
        return;

    std::uniform_int_distribution<int> distribution(0, n-1);
    std::mt19937 rng;
    rng.seed(QDateTime::currentMSecsSinceEpoch());

    int a;
    for(a=0; a<n; a++)
        playlist.swapItemsAt(a, distribution(rng));

    for(a=0; a<n; a++)
        playlist[a]->index = a;

    if(currentSource)
    {
        index = currentSource->index;
        updateHistoryIndex();
    }
}

/*!
 * Sets a playback mode.
 */
void MSE_Playlist::setPlaybackMode(MSE_PlaylistPlaybackMode mode)
{
    playbackMode = mode;
    history->clear();
    historyIndex = -1;
    if(playbackMode == mse_ppmRandom)
        updateHistoryIndex();
    emit onPlaybackModeChange();
}

/*!
 * Adds a sound source to a playlist.
 * End users should not use this.
 */
void MSE_Playlist::addToPlaylistRaw(MSE_Source *src)
{
    src->index = playlist.size();
    playlist.append(src);
    historyIndex = -1;
    history->clear();
}

/*!
 * Fill *sourceType* and *sourceFilename* fields for a *cueSheet*.
 * *cueCheet->cueFilename* must be set beforehand.
 *
 * This function searches for a file that matches a provided CUE sheet.
 * It searches for a first valid sound file in a same directory and with a same basename as CUE file.
 */
bool MSE_Playlist::setSourceDataForCueSheet(MSE_CueSheet *cueSheet)
{
    QFileInfo info(cueSheet->cueFilename);
    QString dirname = info.absolutePath();
    QDir d(dirname);
    QString baseName = info.completeBaseName()+".";
    QStringList iList = d.entryList(QDir::Files);
    foreach(QString fName, iList)
    {
        if(fName.startsWith(baseName))
        {
            cueSheet->sourceType = engine->typeByUri(fName);
            if(cueSheet->sourceType != mse_sctUnknown)
            {
                cueSheet->dataSourceFilename = dirname+"/"+fName;
                return true;
            }
        }
    }
    SETERROR(MSE_Object::Err::cueSourceNotFound, cueSheet->cueFilename);
    return false;
}

/*!
 * Parses ASX file.
 * A stream position in *dev* must point to first symbol of a playlist (not BOM mark).
 * The function appends successfully parsed entries to a *list* variable.
 * Returns false if an error occured during the parsing.
 */
bool MSE_Playlist::parseASX(QIODevice *dev, QList<MSE_PlaylistEntry>& list)
{
    QString s;
    QIODevice* nDev = dev;
    QXmlStreamReader* xml = new QXmlStreamReader(nDev);
    while(!xml->atEnd() && !xml->hasError())
    {
        QXmlStreamReader::TokenType token = xml->readNext();
        if(token == QXmlStreamReader::StartDocument)
            continue;
        if(token == QXmlStreamReader::StartElement)
        {
            if(xml->name() == "entry")
            {
                while(!((xml->tokenType() == QXmlStreamReader::EndElement) && (xml->name() == "entry")))
                {
                    if(xml->tokenType() == QXmlStreamReader::StartElement)
                    {
                        if(xml->name() == "ref")
                        {
                            QXmlStreamAttributes attributes = xml->attributes();
                            if(attributes.hasAttribute("href"))
                            {
                                s = attributes.value("href").toString().trimmed();
                                list.append(MSE_PlaylistEntry(s));
                            }
                        }
                    }
                    xml->readNext();
                }
            }
        }
    }
    bool result = !xml->hasError();
    xml->clear();
    delete xml;
    return result;
}

/*!
 * Same as parseASX(), but parses M3U file.
 */
bool MSE_Playlist::parseM3U(QIODevice* dev, QList<MSE_PlaylistEntry>& list)
{
    try{
        QIODeviceExDec _dev(dev);
        // assuming it's already M3U, so this check is redundant, but we're just skipping the first line anyway
        CHECK_S(MSE_Playlist, _dev.readLineUTF8().startsWith("#EXTM3U"), MSE_Object::Err::invalidFormat);
        QString s;
        QByteArray data;
        bool isNotUtf = false;
        QSharedPointer<MSE_SourceTags> tags;

        // https://en.wikipedia.org/wiki/M3U#Extended_M3U
        QRegularExpression rxInf("^#EXTINF:(?:[^,]*,)*(.+)$");
        QRegularExpression rxAlb("^#EXTALB:(.+)$");
        QRegularExpression rxArt("^#EXTART:(.+)$");
        QRegularExpression rxGenre("^#EXTGENRE:(.+)$");

        QString extAlb;
        QString extArt;
        QString extGenre;

        forever
        {
            if(_dev.atEnd())
                return true;
            data = _dev.readUntilReturn();
            if(isNotUtf)
            {
                s = QString::fromLatin1(data);
            }
            else
            {
                isNotUtf = _dev.isNotUtf8(data);
                if(isNotUtf)
                    s = QString::fromLatin1(data);
                else
                    s = QString::fromUtf8(data);
            }
            s = s.trimmed();
            if(s.isEmpty())
                continue;

            if(s.startsWith("#"))
            {
                if(s.startsWith("#EXT"))
                {
                    if(!tags)
                        tags = QSharedPointer<MSE_SourceTags>::create();

                    QRegularExpressionMatch match = rxInf.match(s);
                    if(match.hasMatch())
                    {
                        tags->trackTitle = match.captured(1).trimmed();
                        continue;
                    }

                    match = rxAlb.match(s);
                    if(match.hasMatch())
                    {
                        auto extStr = match.captured(1).trimmed();
                        if(!extStr.isEmpty())
                            extAlb = extStr;
                        continue;
                    }

                    match = rxArt.match(s);
                    if(match.hasMatch())
                    {
                        auto extStr = match.captured(1).trimmed();
                        if(!extStr.isEmpty())
                            extArt = extStr;
                        continue;
                    }

                    match = rxGenre.match(s);
                    if(match.hasMatch())
                    {
                        auto extStr = match.captured(1).trimmed();
                        if(!extStr.isEmpty())
                            extGenre = extStr;
                        continue;
                    }
                }
                continue;
            }

            if(!tags.isNull())
            {
                tags->trackAlbum = extAlb;
                tags->trackArtist = extArt;
                tags->genre = extGenre;
            }

            list.append(MSE_PlaylistEntry(s, tags));
            tags.clear(); // tags were moved to the shared pointer in MSE_PlaylistEntry
        }
    }catch(...){
        SETERROR_S(MSE_Playlist, MSE_Object::Err::readError);
        return false;
    }
}

/*!
 * Same as parseASX(), but parses XSPF file.
 */
bool MSE_Playlist::parseXSPF(QIODevice* dev, QList<MSE_PlaylistEntry>& list)
{
    QString s;
    QIODevice* nDev = dev;
    QXmlStreamReader* xml = new QXmlStreamReader(nDev);
    while(!xml->atEnd() && !xml->hasError())
    {
        QXmlStreamReader::TokenType token = xml->readNext();
        if(token == QXmlStreamReader::StartDocument)
            continue;
        if(token == QXmlStreamReader::StartElement)
        {
            if(xml->name() == "track")
            {
                while(!((xml->tokenType() == QXmlStreamReader::EndElement) && (xml->name() == "track")))
                {
                    if(xml->tokenType() == QXmlStreamReader::StartElement)
                    {
                        if(xml->name() == "location")
                        {
                            s = xml->readElementText().trimmed();
                            list.append(MSE_PlaylistEntry(s));
                        }
                    }
                    xml->readNext();
                }
            }
        }
    }
    bool result = !xml->hasError();
    xml->clear();
    delete xml;
    return result;
}

/*!
 * Same as parseASX(), but parses PLS file.
 */
bool MSE_Playlist::parsePLS(QIODevice* dev, QList<MSE_PlaylistEntry>& list)
{
    try{
        QIODeviceExDec _dev(dev);
        CHECK_S(MSE_Playlist, _dev.readLineUTF8() == "[playlist]", MSE_Object::Err::invalidFormat);
        QString s;
        QRegularExpression rx("^File\\d+\\s*\\=\\s*(.+)\\s*$");
        forever
        {
            if(_dev.atEnd())
                return true;
            s = _dev.readLineUTF8();
            QRegularExpressionMatch match = rx.match(s);
            if(match.hasMatch())
                list.append(MSE_PlaylistEntry(match.captured(1)));
        }
    }catch(...){
        SETERROR_S(MSE_Playlist, MSE_Object::Err::readError);
        return false;
    }
}

/*!
 * Same as parseASX(), but parses WPL file.
 */
bool MSE_Playlist::parseWPL(QIODevice* dev, QList<MSE_PlaylistEntry>& list)
{
    QString s;
    QIODevice* nDev = dev;
    QXmlStreamReader* xml = new QXmlStreamReader(nDev);
    while(!xml->atEnd() && !xml->hasError())
    {
        QXmlStreamReader::TokenType token = xml->readNext();
        if(token == QXmlStreamReader::StartDocument)
            continue;
        if(token == QXmlStreamReader::StartElement)
        {
            if(xml->name() == "seq")
            {
                while(!((xml->tokenType() == QXmlStreamReader::EndElement) && (xml->name() == "seq")))
                {
                    if(xml->tokenType() == QXmlStreamReader::StartElement)
                    {
                        if(xml->name() == "media")
                        {
                            QXmlStreamAttributes attributes = xml->attributes();
                            if(attributes.hasAttribute("src"))
                            {
                                s = attributes.value("src").toString().trimmed();
                                list.append(MSE_PlaylistEntry(s));
                            }
                        }
                    }
                    xml->readNext();
                }
            }
        }
    }
    bool result = !xml->hasError();
    xml->clear();
    delete xml;
    return result;
}

/*!
 * Writes a list of playlist format in a playlist format to *dev*.
 * The format of a target playlist is specified by *playlistType* parameter.
 */
bool MSE_Playlist::write(QIODevice *dev, const QList<MSE_PlaylistEntry> &playlist, MSE_PlaylistFormatType playlistType)
{
    switch(playlistType)
    {
        case mse_pftASX:
            return writeASX(dev, playlist);

        case mse_pftM3U:
            return writeM3U(dev, playlist);

        case mse_pftXSPF:
            return writeXSPF(dev, playlist);

        case mse_pftPLS:
            return writePLS(dev, playlist);

        case mse_pftWPL:
            return writeWPL(dev, playlist);

        default:
            return false;
    }
}

/*!
 * Writes a list of playlist entries in ASX format to *dev*.
 */
bool MSE_Playlist::writeASX(QIODevice *dev, const QList<MSE_PlaylistEntry> &entries)
{
    QXmlStreamWriter* xml = new QXmlStreamWriter(dev);

    xml->setAutoFormatting(true);
    xml->writeStartDocument();
    xml->writeStartElement("asx");
    xml->writeAttribute("version", "3.0");
    foreach(auto entry, entries)
    {
        xml->writeStartElement("entry");
        xml->writeStartElement("ref");
        xml->writeAttribute("href", entry.uri);
        xml->writeEndElement();
        xml->writeEndElement();
    }
    xml->writeEndDocument();
    if(xml->hasError())
    {
        SETERROR_S(MSE_Playlist, MSE_Object::Err::writeError);
        delete xml;
        return false;
    }

    delete xml;
    return true;
}

/*!
 * Same as writeASX(), only for M3U format.
 */
bool MSE_Playlist::writeM3U(QIODevice *dev, const QList<MSE_PlaylistEntry> &entries)
{
    try{
        QIODeviceExDec _dev(dev);
        _dev.writeLnUTF8("#EXTM3U");

        foreach(auto entry, entries)
        {
            auto tags = entry.tags.data();
            if(tags)
            {
                if(!tags->trackTitle.isEmpty())
                    _dev.writeLnUTF8(QString("#EXTINF:-1,")+tags->trackTitle); // currently track length is not saved anywhere
                if(!tags->trackArtist.isEmpty())
                    _dev.writeLnUTF8(QString("#EXTART:")+tags->trackArtist);
                if(!tags->trackAlbum.isEmpty())
                    _dev.writeLnUTF8(QString("#EXTALB:")+tags->trackAlbum);
                if(!tags->genre.isEmpty())
                    _dev.writeLnUTF8(QString("#EXTGENRE:")+tags->genre);
            }
            _dev.writeLnUTF8(entry.uri);
        }
    }catch(...){
        SETERROR_S(MSE_Playlist, MSE_Object::Err::writeError);
        return false;
    }
    return true;
}

/*!
 * Same as writeASX(), only for XSPF format.
 */
bool MSE_Playlist::writeXSPF(QIODevice *dev, const QList<MSE_PlaylistEntry> &entries)
{
    QXmlStreamWriter* xml = new QXmlStreamWriter(dev);

    xml->setAutoFormatting(true);
    xml->writeStartDocument();
    xml->writeStartElement("playlist");
    xml->writeAttribute("version", "1");
    xml->writeAttribute("xmlns", "http://xspf.org/ns/0/");
    xml->writeStartElement("trackList");
    foreach(auto entry, entries)
    {
        xml->writeStartElement("track");
        xml->writeTextElement("location", entry.uri);
        xml->writeEndElement();
    }
    xml->writeEndDocument();
    if(xml->hasError())
    {
        SETERROR_S(MSE_Playlist, MSE_Object::Err::writeError);
        delete xml;
        return false;
    }

    delete xml;
    return true;
}

/*!
 * Same as writeASX(), only for PLS format.
 */
bool MSE_Playlist::writePLS(QIODevice *dev, const QList<MSE_PlaylistEntry> &entries)
{
    try{
        QIODeviceExDec _dev(dev);
        _dev.writeLnUTF8("[playlist]");
        QString prefix("File");
        int a=0;
        foreach(auto entry, entries)
            _dev.writeLnUTF8(prefix+QString::number(++a)+"="+entry.uri);
        _dev.writeLnUTF8(QStringLiteral("NumberOfEntries=")+QString::number(a));
        _dev.writeLnUTF8(QString("Version=2"));
    }catch(...){
        SETERROR_S(MSE_Playlist, MSE_Object::Err::writeError);
        return false;
    }
    return true;
}

/*!
 * Same as writeASX(), only for WPL format.
 */
bool MSE_Playlist::writeWPL(QIODevice *dev, const QList<MSE_PlaylistEntry> &entries)
{
    QXmlStreamWriter* xml = nullptr;

    try{
        QIODeviceExDec _dev(dev);
        _dev.writeLnUTF8("<?wpl version=\"1.0\" encoding=\"UTF-8\"?>");

        xml = new QXmlStreamWriter(&_dev);

        xml->setAutoFormatting(true);
        xml->writeStartElement("smil");
        xml->writeStartElement("body");
        xml->writeStartElement("seq");
        foreach(auto entry, entries)
        {
            xml->writeStartElement("media");
            xml->writeAttribute("src", entry.uri);
            xml->writeEndElement();
        }
        xml->writeEndDocument();
        if(xml->hasError())
            throw std::exception();
    }catch(...){
        SETERROR_S(MSE_Playlist, MSE_Object::Err::writeError);
        delete xml;
        return false;
    }

    delete xml;
    return true;
}

/*!
 * Writes a current playlist to *dev*.
 * The format of a target playlist is specified by *playlistType* parameter.
 */
bool MSE_Playlist::write(QIODevice *dev, MSE_PlaylistFormatType playlistType) const
{
    QList<MSE_PlaylistEntry> entries;
    foreach(MSE_Source* src, playlist)
        entries.append(src->entry);
    if(!write(dev, entries, playlistType))
        return false;
    return true;
}

/*!
 * Writes a list of a filenames playlist to a file.
 * The format of a target playlist is specified by *playlistType* parameter.
 */
bool MSE_Playlist::write(const QString &filename, const QList<MSE_PlaylistEntry> &playlist, MSE_PlaylistFormatType playlistType)
{
    QFileEx f;
    f.setFileName(filename);
    if(!f.open(QIODevice::WriteOnly))
        return false;
    bool result = write(&f, playlist, playlistType);
    f.close();
    return result;
}

/*!
 * Writes a current playlist to a file.
 * The format of a target playlist is specified by *playlistType* parameter.
 */
bool MSE_Playlist::write(const QString &filename, MSE_PlaylistFormatType playlistType) const
{
    QList<MSE_PlaylistEntry> entries;
    foreach(MSE_Source* src, playlist)
        entries.append(src->entry);
    return write(filename, entries, playlistType);
}

/*!
 * Returns true if a filename has an extension that corresponds to a playlist format supported by MSE.
 */
bool MSE_Playlist::hasSupportedExtension(const QString &filename)
{
    QFileInfo fi(filename);
    QString ext = fi.suffix().toLower();
    if((ext == "m3u")||(ext == "m3u8")||(ext == "asx")||(ext == "pls")||(ext == "xspf")||(ext == "wpl")||(ext == "cue"))
        return true;
    return false;
}

/*!
 * Returns true if a filename has an extension that corresponds to a playlist format supported by MSE.
 */
bool MSE_Playlist::hasSupportedExtension(const QString &filename, bool &isCue)
{
    QFileInfo fi(filename);
    QString ext = fi.suffix().toLower();
    isCue = (ext == "cue");
    if(isCue||(ext == "m3u")||(ext == "m3u8")||(ext == "asx")||(ext == "pls")||(ext == "xspf")||(ext == "wpl"))
        return true;
    return false;
}

/*!
 * Returns a playlist type by its abbreviation.
 *
 * *name* | return value
 * ----------------------
 *  ASX   | mse_pftASX
 *  M3U   | mse_pftM3U
 *  XSPF  | mse_pftXSPF
 *  PLS   | mse_pftPLS
 *  WPL   | mse_pftWPL
 *  CUE   | mse_pftCUE
 */
MSE_PlaylistFormatType MSE_Playlist::typeByName(const QString &name)
{
    if(name == "ASX")
        return mse_pftASX;
    if(name == "M3U")
        return mse_pftM3U;
    if(name == "XSPF")
        return mse_pftXSPF;
    if(name == "PLS")
        return mse_pftPLS;
    if(name == "WPL")
        return mse_pftWPL;
    if(name == "CUE")
        return mse_pftCUE;
    return mse_pftUnknown;
}

/*!
 * Returns an extension (with a leading dot)
 * for a specified playlist format.
 */
QString MSE_Playlist::extByType(MSE_PlaylistFormatType playlistType)
{
    switch(playlistType)
    {
        case mse_pftASX:
            return ".asx";

        case mse_pftM3U:
            return ".m3u";

        case mse_pftXSPF:
            return ".xspf";

        case mse_pftPLS:
            return ".pls";

        case mse_pftWPL:
            return ".wpl";

        case mse_pftCUE:
            return ".cue";

        default:
            return "";
    }
}

/*!
 * Returns a MSE_PlaylistPlaybackMode value by its name.
 * If a return value cannot be determined then the function returns mse_ppmAllLoop
 * and sets *ok* (if it's not nullptr) to false, otherwise - sets ok to true.
 *
 *  *str*      | result
 * -------------------------------
 *  track_once | mse_ppmTrackOnce
 *  all_once   | mse_ppmAllOnce
 *  track_loop | mse_ppmTrackLoop
 *  all_loop   | mse_ppmAllLoop
 *  random     | mse_ppmRandom
 */
MSE_PlaylistPlaybackMode MSE_Playlist::playbackModeFromString(const QString &str, bool *ok)
{
    if(str == "track_once")
    {
        if(ok)
            *ok = true;
        return mse_ppmTrackOnce;
    }
    if(str == "all_once")
    {
        if(ok)
            *ok = true;
        return mse_ppmAllOnce;
    }
    if(str == "track_loop")
    {
        if(ok)
            *ok = true;
        return mse_ppmTrackLoop;
    }
    if(str == "all_loop")
    {
        if(ok)
            *ok = true;
        return mse_ppmAllLoop;
    }
    if(str == "random")
    {
        if(ok)
            *ok = true;
        return mse_ppmRandom;
    }
    if(ok)
        *ok = false;
    return mse_ppmAllLoop;
}

/*!
 * Returns a string representaion of a given MSE_PlaylistPlaybackMode value.
 * Returns an empty string for an invalid value.
 *
 * \sa playbackModeFromString
 */
QString MSE_Playlist::playbackModeToString(MSE_PlaylistPlaybackMode mode)
{
    switch(mode)
    {
        case mse_ppmTrackOnce: return QStringLiteral("track_once");
        case mse_ppmAllOnce: return QStringLiteral("all_once");
        case mse_ppmTrackLoop: return QStringLiteral("track_loop");
        case mse_ppmAllLoop: return QStringLiteral("all_loop");
        case mse_ppmRandom: return QStringLiteral("random");
        default: return QString();
    }
}

/*!
 * Clears the playlist and adds a sound source from a single file.
 */
bool MSE_Playlist::setFile(const MSE_PlaylistEntry &entry)
{
    clear();
    return addFile(entry);
}

/*!
 * Generates a shuffled copy of a playlist.
 * The sound sources are not copied.
 */
void MSE_Playlist::generateShuffle(MSE_Sources *sources)
{
    sources->clear();
    sources->append(playlist);

    int n = playlist.size();
    if((n == 0) || (n == 1))
        return;

    std::uniform_int_distribution<int> distribution(0, n-1);
    std::mt19937 rng;
    rng.seed(QDateTime::currentMSecsSinceEpoch());

    for(int a=0; a<n; a++)
        sources->swapItemsAt(a, distribution(rng));
}

/*!
 * Appends a new portion to a history list.
 * Each portion is a shuffled list generated by generateShuffle().
 *
 * \sa prependHistoryShuffle, getHistory
 */
void MSE_Playlist::appendHistoryShuffle()
{
    if(history->isEmpty())
    {
        generateShuffle(history);
    }
    else
    {
        MSE_Sources* sources = new MSE_Sources();
        generateShuffle(sources);
        if(sources->first() == history->last())
            sources->swapItemsAt(0, sources->size() - 1);
        history->append(*sources);
        delete sources;
    }
}

/*!
 * Prepends a new portion to a beginning of history list.
 * Each portion is a shuffled list generated by generateShuffle().
 *
 * \sa appendHistoryShuffle, getHistory
 */
void MSE_Playlist::prependHistoryShuffle()
{
    if(history->isEmpty())
    {
        generateShuffle(history);
    }
    else
    {
        MSE_Sources* sources = new MSE_Sources();
        generateShuffle(sources);
        if(sources->last() == history->first())
            sources->swapItemsAt(sources->size()-1, 0);
        sources->append(*history);
        delete history;
        history = sources;
    }
}

/*!
 * Updates a position of a current sound source in a history list.
 *
 * \sa getHistoryIndex
 */
void MSE_Playlist::updateHistoryIndex()
{
    if(!currentSource)
    {
        historyIndex = -1;
        return;
    }

    if(history->isEmpty())
    {
        generateShuffle(history);
        historyIndex = 0;
        forever
        {
            if(history->at(historyIndex) == currentSource)
            {
                history->swapItemsAt(historyIndex, 0);
                historyIndex = 0;
                return;
            }
            historyIndex++;
        }
    }

    historyIndex = historyIndex / history->size() * history->size();
    forever
    {
        if(history->at(historyIndex) == currentSource)
            break;
        historyIndex++;
    }
}

/*!
 * Returns an index of a track that will be played next
 * or -1 if no tracks will be played after a current track.
 */
int MSE_Playlist::getNextIndex()
{
    if(!queue.isEmpty())
        return queue.at(0)->index;

    if(playlist.isEmpty())
        return -1;

    int newIndex;

    switch(playbackMode)
    {
        case mse_ppmTrackOnce:
            return -1;

        case mse_ppmTrackLoop:
            return index;

        case mse_ppmAllOnce:
            newIndex = index+1;
            if(newIndex == playlist.size())
                return -1;
            return newIndex;

        case mse_ppmAllLoop:
            newIndex = index+1;
            if(newIndex == playlist.size())
                newIndex = 0;
            return newIndex;

        case mse_ppmRandom:
            newIndex = historyIndex + 1;
            if(newIndex == history->size())
                appendHistoryShuffle();
            return history->at(newIndex)->index;

        default:
            return -1;
    }
}

/*!
 * Returns an index of a track that was played prior a current track.
 * If no tracks were played before a current track,
 * then this function returns an index of a track that is logically before a current track.
 * For example, if playbackMode is mse_ppmAllLoop and an index of a current track is zero,
 * then the "previously" played track is the one with a last index in the playlist.
 *
 * The function returns -1 if there is no previous track (real or imaginary).
 */
int MSE_Playlist::getPrevIndex()
{
    if(playlist.isEmpty())
        return -1;

    int newIndex;

    switch(playbackMode)
    {
        case mse_ppmTrackOnce:
            return -1;

        case mse_ppmTrackLoop:
            return index;

        case mse_ppmAllOnce:
            if(index == 0)
                return -1;
            return index - 1;

        case mse_ppmAllLoop:
            if(index == 0)
                return playlist.size()-1;
            return index - 1;

        case mse_ppmRandom:
            newIndex = historyIndex - 1;
            if(historyIndex == 0)
            {
                prependHistoryShuffle();
                newIndex = playlist.size() - 1;
            }
            return history->at(newIndex)->index;

        default:
            return -1;
    }
}

/*!
 * Tries to move the index to a next position.
 * Returns false on failure.
 */
bool MSE_Playlist::tryMoveToNext()
{
    int newIndex = getNextIndex();
    if(newIndex < 0)
        return false;
    index = newIndex;
    currentSource = playlist[index];
    if(queue.isEmpty())
    {
        if(playbackMode == mse_ppmRandom)
            historyIndex++;
    }
    else
    {
        queue.removeFirst();
    }
    return true;
}

/*!
 * Tries to move the index to a previous position.
 * Returns false on failure.
 *
 * \note See getPrevIndex() for a clarification of what "previous" track means.
 */
bool MSE_Playlist::tryMoveToPrev()
{
    int newIndex = getPrevIndex();
    if(newIndex < 0)
        return false;
    index = newIndex;
    currentSource = playlist[index];
    if(playbackMode == mse_ppmRandom)
    {
        if(historyIndex == 0)
            historyIndex = playlist.size() - 1;
        else
            historyIndex--;
    }
    return true;
}

/*!
 * Moves a playlist index to an entry that should be played next.
 * If there's no such an entry, then a current source will be nullptr
 * and a playlist index will be -1.
 *
 * \sa tryMoveToNext
 */
bool MSE_Playlist::moveToNext()
{
    if(tryMoveToNext())
        return true;
    index = -1;
    currentSource = nullptr;
    return false;
}

/*!
 * Moves a playlist index to a previous track.
 * If there's no such an entry, then a current source will be nullptr
 * and a playlist index will be -1.
 *
 * \note See getPrevIndex() for a clarification of what "previous" track means.
 *
 * \sa tryMoveToNext
 */
bool MSE_Playlist::moveToPrev()
{
    if(tryMoveToPrev())
        return true;
    index = -1;
    currentSource = nullptr;
    return false;
}

/*!
 * Sets a new playlist index.
 * Returns false if a new index is invalid.
 */
bool MSE_Playlist::setIndex(int newIndex)
{
    CHECK((newIndex >= 0) && (newIndex < playlist.size()), MSE_Object::Err::outOfRange);

    index = newIndex;
    currentSource = playlist[index];

    if(playbackMode == mse_ppmRandom)
        updateHistoryIndex();

    return true;
}

/*!
 * Returns an audio source, that should be played next
 * or nullptr if there's no such a source.
 *
 * \sa moveToNext
 */
MSE_Source *MSE_Playlist::getNextSource()
{
    int newIndex = getNextIndex();
    if(newIndex < 0)
        return nullptr;
    return playlist.at(newIndex);
}

/*!
 * Returns an audio source, that was played previously played
 * or nullptr if there's no such a source.
 *
 * \note See getPrevIndex() for a clarification of what "previous" track means.
 *
 * \sa moveToNext
 */
MSE_Source *MSE_Playlist::getPrevSource()
{
    int newIndex = getPrevIndex();
    if(newIndex < 0)
        return nullptr;
    return playlist.at(newIndex);
}

/*!
 * Returns true if a current source is the first one
 * in a sequence of sources in the playlist
 * that are from the same directory.
 *
 * In other words, it returns true if a source is a first in a playlist
 * (not applied when a random playback is on)
 * or a previous track's is from an other directory
 * (not applied to URLs).
 */
bool MSE_Playlist::isFirstInDir()
{
    if(isAtStart())
        return true;
    const MSE_Source* prevSource = getPrevSource();
    if(!prevSource)
        return true;
    QString curParentDir = (QFileInfo(currentSource->entry.filename)).absoluteDir().absolutePath();
    QString prevParentDir = (QFileInfo(prevSource->entry.filename)).absoluteDir().absolutePath();
    return curParentDir != prevParentDir;
}

/*!
 * Returns true if a current source is the last one
 * in a sequence of sources in the playlist
 * that are from the same directory.
 *
 * In other words, it returns true if a source is a last in a playlist
 * (not applied when a random playback is on)
 * or a next track's is from an other directory
 * (not applied to URLs).
 */
bool MSE_Playlist::isLastInDir()
{
    if(isAtEnd())
        return true;
    const MSE_Source* nextSource = getNextSource();
    if(!nextSource)
        return true;
    QString curParentDir = (QFileInfo(currentSource->entry.filename)).absoluteDir().absolutePath();
    QString nextParentDir = (QFileInfo(nextSource->entry.filename)).absoluteDir().absolutePath();
    return curParentDir != nextParentDir;
}

/*!
 * Returns true if a current track is the first one in the playlist.
 * If there's no current source, this function returns true.
 * If there's more than one track an a random playback is on,
 * then this function returns false.
 */
bool MSE_Playlist::isAtStart()
{
    if(!currentSource || (playlist.size() <= 1))
        return true;
    if((index == 0) && (playbackMode != mse_ppmRandom))
        return true;
    return false;
}

/*!
 * Returns true if a current track is the last one in the playlist.
 * If there's no current source, this function returns true.
 * If there's more than one track an a random playback is on,
 * then this function returns false.
 */
bool MSE_Playlist::isAtEnd()
{
    if(!currentSource || (playlist.size() <= 1))
        return true;
    if((index == (playlist.size()-1)) && (playbackMode != mse_ppmRandom))
        return true;
    return false;
}

/*!
 * Moves a playlist index to a sound source
 * that is the first one in a sequence of sources in the playlist
 * that are from the same directory.
 *
 * See isFirstInDir() for more information.
 *
 * \sa moveToFirstInPrevDir
 */
bool MSE_Playlist::moveToFirstInDir()
{
    if(playbackMode == mse_ppmRandom)
    {
        int filesToSkip = playlist.size()+1;
        while(!isFirstInDir())
        {
            filesToSkip--;
            if(!filesToSkip)
                break;
            if(!moveToPrev())
                return false;
        }
    }
    else
    {
        while(!isFirstInDir())
            if(!moveToPrev())
                return false;
    }
    return true;
}

/*!
 * Moves a playlist index to a sound source
 * that is the first one in a *previous* sequence of sources in the playlist
 * that are from the same directory.
 *
 * See isFirstInDir() for more information.
 *
 * \sa moveToFirstInDir
 */
bool MSE_Playlist::moveToFirstInPrevDir()
{
    if(!moveToFirstInDir())
        return false;
    if(!moveToPrev())
        return false;
    return moveToFirstInDir();
}

/*!
 * Moves a playlist index to a sound source
 * that is the first one in a *next* sequence of sources in the playlist
 * that are from the same directory.
 *
 * See isFirstInDir() for more information.
 *
 * \sa moveToFirstInDir
 */
bool MSE_Playlist::moveToFirstInNextDir()
{
    if(playbackMode == mse_ppmRandom)
    {
        int filesToSkip = playlist.size()+1;
        while(!isLastInDir())
        {
            filesToSkip--;
            if(!filesToSkip)
                break;
            if(!moveToNext())
                return false;
        }
    }
    else
    {
        while(!isLastInDir())
            if(!moveToNext())
                return false;
    }
    return moveToNext();
}
