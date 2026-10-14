#ifndef HISTORYARCHIVEIO_H
#define HISTORYARCHIVEIO_H

#include "operationrecorder.h"

#include <QDate>
#include <QJsonObject>
#include <QMap>
#include <QString>

class QIODevice;

/**
 * @brief 只读历史存档的加载结果。失败时 error 非空，records 为空。
 */
struct HistoryLoadResult {
    QList<OperationRecord> records;
    QString error;
    int truncatedFieldCount = 0;

    bool ok() const { return error.isEmpty(); }
};

struct HistoryDateEntry {
    QDate date;
    QString path;
    bool jsonLines = false;
    qint64 size = 0;
};

struct HistoryDateScanResult {
    QMap<QDate, HistoryDateEntry> entries;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

/**
 * @brief 读取并校验磁盘上的历史存档，不修改 OperationRecorder 的实时数据。
 */
class HistoryArchiveIo final
{
public:
    static HistoryLoadResult loadFile(const QString &fileName,
                                      const QDate &today = QDate::currentDate());
    static HistoryDateScanResult scanAvailableDates(const QString &directory,
                                                    const QDate &today = QDate::currentDate());
    static HistoryLoadResult loadDateFile(const QString &fileName,
                                          const QDate &expectedDate);

private:
    static HistoryLoadResult loadChecked(const QString &fileName,
                                         const QDate &boundaryDate,
                                         bool exactDate);
    static bool parseJsonArray(const QByteArray &data,
                               const QDate &boundaryDate,
                               bool exactDate,
                               HistoryLoadResult *result);
    static bool parseJsonLines(QIODevice *device,
                               const QDate &boundaryDate,
                               bool exactDate,
                               HistoryLoadResult *result);
    static bool validateRecordObject(const QJsonObject &object,
                                     const QDate &boundaryDate,
                                     bool exactDate,
                                     const QString &location,
                                     OperationRecord *record,
                                     int *truncatedCount,
                                     QString *error);
    static bool fileContainsDate(const QString &fileName, const QDate &date, bool jsonLines);
};

#endif // HISTORYARCHIVEIO_H
