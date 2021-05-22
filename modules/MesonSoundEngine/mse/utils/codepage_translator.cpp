#include "codepage_translator.h"

#include "qiodevicehelper.h"

#ifdef MSE_ICU
    #include "unicode/ucsdet.h"
    #include "unicode/ucnv.h"
#endif

MSE_CodepageTranslator::MSE_CodepageTranslator(bool useICU, int minConfidence)
    :useICU(useICU)
    ,minConfidence(minConfidence)
{
}

void MSE_CodepageTranslator::addEntry(const char* strData, int dataLen, const Callback &callback)
{
    Entry entry {
        .callback = callback,
        .strData = QByteArray(strData, dataLen),
        .result = QString()
    };
    entries.push_back(entry);
}

void MSE_CodepageTranslator::processEntries(const QString& reference)
{
    QMutableListIterator<Entry> i(entries);

#ifdef MSE_ICU
    QByteArray allText;
    bool needICU = false;

    int maxStrSize = 0;
    for(const Entry& entry : qAsConst(entries))
        maxStrSize += entry.strData.size();
    allText.reserve(maxStrSize);
#endif

    while(i.hasNext())
    {
        auto& entry = i.next();
        if(entry.strData.isEmpty())
        {
            entry.needICU = false;
            continue;
        }

        if(translateWithoutICU(entry.strData, entry.result))
        {
            entry.needICU = false;
            continue;
        }

#ifdef MSE_ICU
        allText.append(entry.strData);
        needICU = true;
#else
        entry.result = QString::fromLatin1(entry.strData);
#endif
    }

#ifdef MSE_ICU
    if(needICU && useICU)
    {
        QList<ConvEntry> convs;
        if(!detectCodepage(allText, convs))
        {
            convertAllToLatin();
            return;
        }

        auto err = U_ZERO_ERROR;
        auto *utfCnv = ucnv_open("UTF8", &err);
        if(U_FAILURE(err))
        {
            convertAllToLatin();
            return;
        }

        QHash<int, QString> bestTr;
        int bestConfidence = 0;
        int bestRefScore = 0;

        QString validRef;
        validRef.reserve(reference.size());

        for(const QChar& c : qAsConst(reference))
        {
            if(c.unicode() < 128 || c.isSpace() || c.isDigit() || c.isPunct())
                continue;
            validRef.append(c);
        }

        for(const auto& conv : qAsConst(convs))
        {
            QMutableListIterator<Entry> i(entries);
            int entryIndex = -1;
            QHash<int, QString> trEntry;
            int refScore = 0;
            int charsCounted = 0;
            bool ok = true;
            while(i.hasNext())
            {
                entryIndex++;
                auto& entry = i.next();
                if(!entry.needICU)
                    continue;
                QString result;
                if(translateWithICU(conv.converter, utfCnv, entry.strData, result))
                {
                    if(!validRef.isEmpty())
                    {
                        for(const QChar& c : qAsConst(result))
                        {
                            if(c.unicode() < 128 || c.isSpace() || c.isDigit() || c.isPunct())
                                continue;
                            charsCounted++;
                            if(validRef.contains(c.toUpper()) || reference.contains(c.toLower()))
                                refScore++;
                        }
                    }
                    trEntry[entryIndex] = result;
                }
                else
                {
                    ok = false;
                    break;
                }
            }

            if(!ok)
                continue;

            if(validRef.isEmpty())
            {
                if(conv.confidence >= minConfidence)
                {
                    bestTr = trEntry;
                    bestConfidence = conv.confidence;
                }
                break;
            }

            if(refScore > bestRefScore)
            {
                bestTr = trEntry;
                bestConfidence = conv.confidence;
                bestRefScore = refScore;
                continue;
            }

            if(refScore == bestRefScore && conv.confidence > bestConfidence)
            {
                bestTr = trEntry;
                bestConfidence = conv.confidence;
            }
        }

        ucnv_close(utfCnv);

        if(bestRefScore > 0 || bestConfidence > 0)
        {
            for(auto i = bestTr.constKeyValueBegin(); i != bestTr.constKeyValueEnd(); i++)
            {
                auto entryIndex = (*i).first;
                entries[entryIndex].result = (*i).second;
            }
        }
        else
        {
            convertAllToLatin();
            return;
        }
    }
#endif

    for(const Entry& entry : qAsConst(entries))
    {
        const QString result = entry.result.trimmed();
        entry.callback(result);
    }

    clearEntries();
}

bool MSE_CodepageTranslator::detectCodepage(
    const QByteArray& text,
    QList<ConvEntry>& cnvPtrs
)
{
#ifndef MSE_ICU
    Q_UNUSED(text)
    Q_UNUSED(cnvPtrs)
    return false;
#else
    if(text.isEmpty())
        return false;

    auto err = U_ZERO_ERROR;
    auto *det = ucsdet_open(&err);
    auto detPtr = QSharedPointer<UCharsetDetector>(det, [](UCharsetDetector* det){
        ucsdet_close(det);
    });
    if(U_FAILURE(err))
        return false;

    err = U_ZERO_ERROR;
    ucsdet_setText(det, text.constData(), text.size(), &err);
    if(U_FAILURE(err))
        return false;

    err = U_ZERO_ERROR;
    auto const *match = ucsdet_detect(det, &err);
    if(U_FAILURE(err) || match == nullptr)
        return false;

    auto nMatches = 0;
    auto const *matches = ucsdet_detectAll(det, &nMatches, &err);
    if(U_FAILURE(err) || matches == nullptr)
        return false;

    for(int i = 0; i < nMatches; i++)
    {
        err = U_ZERO_ERROR;
        auto confidence = ucsdet_getConfidence(matches[i], &err);
        if(U_FAILURE(err))
            continue;

        if(confidence < minConfidence)
            break; // converters are sorted by confidence

        err = U_ZERO_ERROR;
        auto *detName = ucsdet_getName(matches[i], &err);
        if(U_FAILURE(err))
            continue;

        err = U_ZERO_ERROR;
        auto *cnv = ucnv_open(detName, &err);
        if(U_FAILURE(err))
            continue;

        auto cnvPtr = QSharedPointer<UConverter>(cnv, [](UConverter* cnv){
            ucnv_close(cnv);
        });

        ConvEntry conv {
            .converter = cnvPtr,
            .confidence = confidence
        };
        cnvPtrs.append(conv);
    }

    if(cnvPtrs.isEmpty())
        return false;

    return true;
#endif
}

bool MSE_CodepageTranslator::translateWithICU(
    QSharedPointer<UConverter> cnvPtr,
    UConverter* utfCnv,
    const QByteArray& strData,
    QString& result
)
{
#ifndef MSE_ICU
    return false;
#else
    if(strData.isEmpty())
        return false;

    auto utfCharSize = ucnv_getMaxCharSize(utfCnv);
    auto nTargetBytes = UCNV_GET_MAX_BYTES_FOR_STRING(strData.size(), utfCharSize);
    char utfBytes[nTargetBytes + 1];
    char* utfPtr = utfBytes;

    const char* sourceBytes = strData.constData();
    const char* endSourceBytePtr = strData.constEnd();

    auto err = U_ZERO_ERROR;
    ucnv_convertEx(
        utfCnv,
        cnvPtr.data(),
        &utfPtr,
        &utfBytes[nTargetBytes + 1],
        &sourceBytes,
        endSourceBytePtr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        true,
        true,
        &err
    );
    if(U_FAILURE(err))
        return false;

    auto nResultBytes = utfPtr - utfBytes;
    result = QString::fromUtf8(utfBytes, nResultBytes);
    return true;
#endif
}

void MSE_CodepageTranslator::convertAllToLatin()
{
    for(const Entry& entry : qAsConst(entries))
    {
        auto s = entry.result.isEmpty() ? QString::fromLatin1(entry.strData) : entry.result;
        entry.callback(s);
    }
    clearEntries();
}

void MSE_CodepageTranslator::clearEntries()
{
    entries.clear();
}

bool MSE_CodepageTranslator::translateWithoutICU(const QByteArray& strData, QString& result)
{
    // detect UTF-16 BOM 0xFEFF or 0xFFEF
    const void* rawData = strData.constData();
    if(
        (strData.size() > 1)
        &&
        (
            (
                (static_cast<const unsigned char*>(rawData)[0] == 0xFF)
                &&
                (static_cast<const unsigned char*>(rawData)[1] == 0xFE)
            )
            ||
            (
                (static_cast<const unsigned char*>(rawData)[0] == 0xFE)
                &&
                (static_cast<const unsigned char*>(rawData)[1] == 0xFF)
            )
        )
    )
    {
        result = QString::fromUtf16(static_cast<const ushort*>(rawData), strData.size()/2);
        return true;
    }

    if(QIODeviceEx::isNotUtf8(static_cast<const char*>(rawData), strData.size()))
        return false;

    result = QString::fromUtf8(strData);
    return true;
}
