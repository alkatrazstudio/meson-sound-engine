/****************************************************************************}
{ MesonSoundEngine.qbs - sound engine                                        }
{                                                                            }
{ Copyright (c) 2011 Alexey Parfenov <zxed@alkatrazstudio.net>               }
{                                                                            }
{ This library is free software: you can redistribute it and/or modify it    }
{ under the terms of the GNU General Public License as published by          }
{ the Free Software Foundation, either version 3 of the License,             }
{ or (at your option) any later version.                                     }
{                                                                            }
{ This library is distributed in the hope that it will be useful,            }
{ but WITHOUT ANY WARRANTY; without even the implied warranty of             }
{ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU           }
{ General Public License for more details: https://gnu.org/licenses/gpl.html }
{****************************************************************************/

import qbs.FileInfo

Module {
    property bool mpris: false
    property bool lastfm: false
    property bool mixer: mpris || lastfm
    property bool sourceUrl: false

    readonly property bool mprisEnabled: {
        return mpris && Common.isLinux
    }

    readonly property bool mixerEnabled: {
        return mprisEnabled || lastfm
    }

    Depends {name: 'cpp'}
    Depends {
        name: 'Qt'
        submodules: {
            var mods = ['core']
            if(mprisEnabled)
                mods.push('dbus')
            if(lastfm || sourceUrl)
                mods.push('network')
            return mods
        }
    }
    Depends {name: 'Common'}
    Depends {name: 'CoreApp'}
    Depends {name: 'ErrorManager'}
    Depends {name: 'QIODeviceHelper'}

    Depends {
        condition: mprisEnabled
        name: 'mpris-qt5'
    }

    Depends {
        condition: Common.isLinux
        name: 'alsa'
    }

    Depends {
        condition: !Common.isWindows
        name: 'quazip'
    }

    Depends {name: 'zlib'}

    Group {
        name: 'MesonSoundEngine'
        files: {
            var files = [
                'mse/*.cpp',
                'mse/*.h',
                'mse/sources/source.cpp',
                'mse/sources/source.h',
                'mse/sources/source_module.cpp',
                'mse/sources/source_module.h',
                'mse/sources/source_plugin.cpp',
                'mse/sources/source_plugin.h',
                'mse/sources/source_stream.cpp',
                'mse/sources/source_stream.h',
            ]

            if(MesonSoundEngine.sourceUrl)
            {
                files.push('mse/sources/source_url.cpp')
                files.push('mse/sources/source_url.h')
            }

            if(MesonSoundEngine.mprisEnabled)
            {
                files.push('mse/utils/mpris.cpp')
                files.push('mse/utils/mpris.h')
            }

            if(MesonSoundEngine.lastfm)
            {
                files.push('mse/utils/lastfm.cpp')
                files.push('mse/utils/lastfm.h')
            }

            if(MesonSoundEngine.mixerEnabled)
            {
                files.push('mse/utils/mixer.cpp')
                files.push('mse/utils/mixer.h')
            }

            return files
        }
    }

    cpp.includePaths: FileInfo.relativePath(product.sourceDirectory, path)
    cpp.dynamicLibraries: {
        var libs = ['bass']
        if(MesonSoundEngine.mixerEnabled)
            libs.push('bassmix')
        if(Common.isWindows)
        {
            libs.push('ole32')
            libs.push('quazip5')
        }
        return libs
    }

    cpp.defines: {
        var defs = []
        if(MesonSoundEngine.mprisEnabled)
            defs.push('MSE_MODULE_MPRIS')
        if(MesonSoundEngine.lastfm)
            defs.push('MSE_MODULE_LASTFM')
        if(MesonSoundEngine.mixerEnabled)
            defs.push('MSE_MODULE_MIXER')
        if(MesonSoundEngine.sourceUrl)
            defs.push('MSE_MODULE_SOURCE_URL')
        return defs
    }

    Properties {
        condition: Common.isOSX

        cpp.frameworks: outer.concat([
            'CoreAudio'
        ])
    }
}
