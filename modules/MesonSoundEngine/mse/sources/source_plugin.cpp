/****************************************************************************}
{ source_plugin.cpp - plugin-supported file                                  }
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

#include "mse/sources/source_plugin.h"
#include "mse/sound.h"

MSE_SourcePlugin::MSE_SourcePlugin(MSE_Playlist *parent) : MSE_SourceStream(parent)
{
    type = mse_sctPlugin;
}

bool MSE_SourcePlugin::getTags(MSE_SourceTags &tags)
{
    if(MSE_SourceStream::getTags(tags))
        return true;

    if(!parseTagsOGG(stream, tags, BASS_TAG_MP4))
        if(!parseTagsOGG(stream, tags, BASS_TAG_APE))
            if(!parseTagsOGG(stream, tags, BASS_TAG_WMA))
                return false;

    return true;
}
