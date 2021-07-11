#include "utils.h"

#include <QStandardPaths>

namespace MSE_Utils
{
    /*!
     * Transforms file:// links to a normal filenames.
     * On Unix, if a filename begins with ~,
     * then this function will repace ~ with a home directory.
     */
    QString normalizeUri(const QString &source)
    {
        QString result = source.trimmed();

        if(result.startsWith("file:///"))
        {
    #ifdef Q_OS_WIN
            result = result.mid(8);
    #else
            result = result.mid(7);
    #endif
        }
    #ifndef Q_OS_WIN
        else
        {
            if(result.startsWith('~'))
            {
                if(result.size() == 1)
                    result.replace(0, 1, getHomeDir());
                else
                    if(result.at(1) == '/')
                        result.replace(0, 1, getHomeDir());
            }
        }
    #endif
        return result;
    }


    /*!
     * Returns user's home directory.
     */
    const QString& getHomeDir()
    {
        static QStringList locations = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
        static QString homeDir;
        return homeDir;
    }
}
