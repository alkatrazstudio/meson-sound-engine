/****************************************************************************}
{ source_module.cpp - module file                                            }
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

#include "mse/sources/source_module.h"
#include "mse/sound.h"

MSE_SourceModule::MSE_SourceModule(MSE_Playlist *parent) : MSE_Source(parent)
  ,channel(0)
{
    type = mse_sctModule;
}

HCHANNEL MSE_SourceModule::open()
{
    channel = BASS_MusicLoad(
        false,
        getUtfFilename(),
        0, 0,
        sound->getDefaultMusicFlags(),
        0
    );
    if(channel)
        return channel;

    // check if the module is zipped
    if(!sound->getEngine()->unzipFile(filename, memFile))
        return false;
    // open unzipped file from memory
    channel = BASS_MusicLoad(
        true,
        memFile.constData(),
        0, memFile.size(),
        sound->getDefaultMusicFlags(),
        0
    );
    if(!channel)
    {
        memFile.clear();
        return 0;
    }

    return channel;
}

bool MSE_SourceModule::close()
{
    BASS_MusicFree(channel);
    channel = 0;
    memFile.clear();
    return true;
}

bool MSE_SourceModule::parseTagsMOD(MSE_SourceTags &tags)
{
    const char* tagsData = BASS_ChannelGetTags(channel, BASS_TAG_MUSIC_NAME);
    if(!tagsData)
        return false;
    tags.trackTitle = QString::fromUtf8(tagsData);
    return true;
}

bool MSE_SourceModule::getTags(MSE_SourceTags &tags)
{
    if(MSE_Source::getTags(tags))
        return true;
    return parseTagsMOD(tags);
}