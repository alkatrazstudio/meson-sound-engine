/****************************************************************************}
{ source_plugin.h - plugin-supported file                                    }
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

#include "mse/sources/source_stream.h"

class MSE_SourcePlugin : public MSE_SourceStream
{
    Q_OBJECT
public:
    explicit MSE_SourcePlugin(MSE_Playlist *parent);

protected:
    virtual bool getTags(MSE_SourceTags &tags);
    bool parseTagsAPE(MSE_SourceTags &tags);
    bool parseTagsMP4(MSE_SourceTags &tags);
    bool parseTagsWMA(MSE_SourceTags &tags);
};
