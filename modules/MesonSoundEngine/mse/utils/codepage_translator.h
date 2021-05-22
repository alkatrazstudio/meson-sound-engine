#pragma once

#include <QString>
#include <QSharedPointer>

struct UCharsetDetector;
struct UConverter;

class MSE_CodepageTranslator {
    using Callback = std::function<void(const QString&)>;

    struct Entry {
        Callback callback;
        QByteArray strData;
        QString result;
        bool needICU = true;
    };

    struct ConvEntry {
        QSharedPointer<UConverter> converter;
        int confidence;
    };

    QList<Entry> entries;
    bool useICU;
    int minConfidence;

    void convertAllToLatin();
    bool detectCodepage(const QByteArray& text, QList<ConvEntry> &cnvPtrs);
    bool translateWithICU(
        QSharedPointer<UConverter> cnvPtr,
        UConverter* utfCnv,
        const QByteArray& strData,
        QString& result);
    bool translateWithoutICU(const QByteArray &strData, QString& result);

public:
    MSE_CodepageTranslator(bool useICU, int minConfidence = 0);
    void addEntry(const char* strData, int dataLen, const Callback &callback);
    void processEntries(const QString& reference);
    void clearEntries();
};
