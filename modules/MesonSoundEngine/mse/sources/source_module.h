/****************************************************************************}
{ source_module.h - module file                                              }
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

class MSE_SourceModule : public MSE_Source
{
    Q_OBJECT
public:
    explicit MSE_SourceModule(MSE_Playlist *parent);
    virtual HCHANNEL open();
    virtual bool close();

protected:
    virtual bool getTags(MSE_SourceTags &tags);
    bool parseTagsMOD(MSE_SourceTags &tags);

    QByteArray memFile;
    HCHANNEL channel;
};
