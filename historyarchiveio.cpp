#include "historyarchiveio.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {

constexpr int kMaxFieldChars = 512;
constexpr int kMaxRecords = 100000;
constexpr qint64 kMaxFileBytes = 50LL * 1024 * 1024;

void fail(HistoryLoadResult *result, const QString &error)
{
    result->records.clear();
    result->truncatedFieldCount = 0;
    result->repairedLineCount = 0;
    result->skippedLineCount = 0;
    result->skippedLocations.clear();
    result->warning.clear();
    result->error = error;
}

bool isSignedEnvelope(const QJsonObject &object)
{
    return object.contains(QStringLiteral("payload"))
        && object.contains(QStringLiteral("sig"));
}

QString truncateField(const QString &value, int *truncatedCount)
{
    if (value.size() <= kMaxFieldChars) {
        return value;
    }
    ++(*truncatedCount);
    return value.left(kMaxFieldChars);
}

bool readStringField(const QJsonObject &object,
                     const char *key,
                     QString *value,
                     int *truncatedCount,
                     QString *error,
                     const QString &location)
{
    const QJsonValue jsonValue = object.value(QLatin1String(key));
    if (!object.contains(QLatin1String(key)) || !jsonValue.isString()) {
        *error = QStringLiteral("%1格式不正确").arg(location);
        return false;
    }
    *value = truncateField(jsonValue.toString(), truncatedCount);
    return true;
}

QDateTime parseTimestamp(const QString &text)
{
    QDateTime timestamp = QDateTime::fromString(text, Qt::ISODate);
    if (!timestamp.isValid()) {
        timestamp = QDateTime::fromString(text, Qt::ISODateWithMs);
    }
    return timestamp;
}

void sortNewestFirst(QList<OperationRecord> *records)
{
    std::stable_sort(records->begin(), records->end(),
                     [](const OperationRecord &left, const OperationRecord &right) {
                         return left.timestamp > right.timestamp;
                     });
}

} // namespace

HistoryLoadResult HistoryArchiveIo::loadFile(const QString &fileName, const QDate &today)
{
    return loadChecked(fileName, today, false);
}

HistoryLoadResult HistoryArchiveIo::loadDateFile(const QString &fileName, const QDate &expectedDate)
{
    return loadChecked(fileName, expectedDate, true);
}

HistoryLoadResult HistoryArchiveIo::loadChecked(const QString &fileName,
                                                const QDate &boundaryDate,
                                                bool exactDate)
{
    HistoryLoadResult result;
    const QString path = fileName.trimmed();
    if (path.isEmpty()) {
        result.error = QStringLiteral("未选择历史记录文件");
        return result;
    }

    const QFileInfo info(path);
    if (!info.exists()) {
        result.error = QStringLiteral("历史记录文件不存在");
        return result;
    }
    if (!info.isFile()) {
        result.error = QStringLiteral("请选择历史记录文件");
        return result;
    }

    const QString suffix = info.suffix().toLower();
    if (suffix != QLatin1String("json") && suffix != QLatin1String("jsonl")) {
        result.error = QStringLiteral("请选择 JSON 或 JSONL 历史记录文件");
        return result;
    }
    if (!info.isReadable()) {
        result.error = QStringLiteral("没有读取该历史记录文件的权限");
        return result;
    }
    if (info.size() > kMaxFileBytes) {
        result.error = QStringLiteral("历史记录文件过大，无法加载");
        return result;
    }
    if (info.size() == 0) {
        result.error = QStringLiteral("历史记录文件是空的");
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.error() == QFileDevice::PermissionsError
            ? QStringLiteral("没有读取该历史记录文件的权限")
            : QStringLiteral("历史记录文件无法读取");
        return result;
    }

    const bool parsed = suffix == QLatin1String("json")
        ? parseJsonArray(file.readAll(), boundaryDate, exactDate, &result)
        : parseJsonLines(&file, boundaryDate, exactDate, &result);
    if (!parsed) {
        return result;
    }

    sortNewestFirst(&result.records);
    return result;
}

bool HistoryArchiveIo::parseJsonArray(const QByteArray &data,
                                    const QDate &boundaryDate,
                                    bool exactDate,
                                    HistoryLoadResult *result)
{
    QByteArray json = data;
    if (json.startsWith("\xEF\xBB\xBF")) {
        json.remove(0, 3);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        fail(result, QStringLiteral("历史记录文件格式不正确"));
        return false;
    }

    const QJsonArray array = document.array();
    if (array.size() > kMaxRecords) {
        fail(result, QStringLiteral("历史记录条数过多，无法加载"));
        return false;
    }

    result->records.reserve(array.size());
    int index = 0;
    for (const QJsonValue &value : array) {
        ++index;
        const QString location = QStringLiteral("第 %1 条记录").arg(index);
        if (!value.isObject() || isSignedEnvelope(value.toObject())) {
            fail(result, QStringLiteral("%1格式不正确").arg(location));
            return false;
        }

        OperationRecord record;
        QString error;
        if (!validateRecordObject(value.toObject(), boundaryDate, exactDate, location, &record,
                                  &result->truncatedFieldCount, &error)) {
            fail(result, error);
            return false;
        }
        result->records.append(record);
    }
    return true;
}

bool HistoryArchiveIo::parseJsonLines(QIODevice *device,
                                    const QDate &boundaryDate,
                                    bool exactDate,
                                    HistoryLoadResult *result)
{
    int lineNumber = 0;
    int nonEmptyCount = 0;
    while (!device->atEnd()) {
        QByteArray line = device->readLine();
        ++lineNumber;
        if (lineNumber == 1 && line.startsWith("\xEF\xBB\xBF")) {
            line.remove(0, 3);
        }
        if (line.endsWith('\n')) {
            line.chop(1);
        }
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        const bool hadNull = line.contains('\0');
        if (hadNull) {
            line.replace('\0', "");
        }
        if (line.trimmed().isEmpty()) {
            if (hadNull) {
                ++result->skippedLineCount;
                if (result->skippedLocations.size() < 10) {
                    result->skippedLocations.append(QString::number(lineNumber));
                }
            }
            continue;
        }
        ++nonEmptyCount;
        if (result->records.size() >= kMaxRecords) {
            fail(result, QStringLiteral("历史记录条数过多，无法加载"));
            return false;
        }

        const QString location = QStringLiteral("第 %1 行记录").arg(lineNumber);
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()
            && isSignedEnvelope(document.object())) {
            fail(result, QStringLiteral("%1格式不正确").arg(location));
            return false;
        }
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            ++result->skippedLineCount;
            if (result->skippedLocations.size() < 10) {
                result->skippedLocations.append(QString::number(lineNumber));
            }
            continue;
        }
        if (hadNull) {
            ++result->repairedLineCount;
        }

        OperationRecord record;
        QString error;
        if (!validateRecordObject(document.object(), boundaryDate, exactDate, location, &record,
                                  &result->truncatedFieldCount, &error)) {
            if (error.contains(QStringLiteral("不属于"))) {
                fail(result, error);
                return false;
            }
            ++result->skippedLineCount;
            if (result->skippedLocations.size() < 10) {
                result->skippedLocations.append(QString::number(lineNumber));
            }
            continue;
        }
        result->records.append(record);
    }

    if (result->skippedLineCount > 100
        || (nonEmptyCount > 0 && result->skippedLineCount * 100 > nonEmptyCount)) {
        fail(result, QStringLiteral("历史文件损坏内容过多，无法安全读取"));
        return false;
    }
    if (result->records.isEmpty()) {
        fail(result, QStringLiteral("历史记录文件中没有有效记录"));
        return false;
    }
    if (result->repairedLineCount > 0 || result->skippedLineCount > 0) {
        QStringList parts;
        if (result->repairedLineCount > 0) {
            parts.append(QStringLiteral("已恢复%1行空字节损坏内容").arg(result->repairedLineCount));
        }
        if (result->skippedLineCount > 0) {
            parts.append(QStringLiteral("已跳过%1行损坏内容").arg(result->skippedLineCount));
        }
        parts.append(QStringLiteral("其余%1条记录已读取").arg(result->records.size()));
        result->warning = parts.join(QStringLiteral("，"));
    }
    return true;
}

bool HistoryArchiveIo::validateRecordObject(const QJsonObject &object,
                                            const QDate &boundaryDate,
                                            bool exactDate,
                                            const QString &location,
                                            OperationRecord *record,
                                            int *truncatedCount,
                                            QString *error)
{
    const QJsonValue timestampValue = object.value(QStringLiteral("timestamp"));
    if (!timestampValue.isString()) {
        *error = QStringLiteral("%1格式不正确").arg(location);
        return false;
    }
    const QDateTime timestamp = parseTimestamp(timestampValue.toString());
    if (!timestamp.isValid()) {
        *error = QStringLiteral("%1的时间无效").arg(location);
        return false;
    }
    if (exactDate) {
        if (!boundaryDate.isValid() || timestamp.date() != boundaryDate) {
            *error = QStringLiteral("%1不属于所选日期").arg(location);
            return false;
        }
    } else if (timestamp.date() >= boundaryDate) {
        *error = QStringLiteral("%1不属于历史存档").arg(location);
        return false;
    }

    qint64 runtimeMs = 0;
    if (object.contains(QStringLiteral("runtime_ms"))) {
        const QJsonValue runtimeValue = object.value(QStringLiteral("runtime_ms"));
        if (!runtimeValue.isDouble()) {
            *error = QStringLiteral("%1格式不正确").arg(location);
            return false;
        }
        runtimeMs = static_cast<qint64>(runtimeValue.toDouble());
    }

    QString pageName;
    QString controlName;
    QString controlType;
    QString operation;
    QString oldValue;
    QString newValue;
    if (!readStringField(object, "pageName", &pageName, truncatedCount, error, location)
        || !readStringField(object, "controlName", &controlName, truncatedCount, error, location)
        || !readStringField(object, "controlType", &controlType, truncatedCount, error, location)
        || !readStringField(object, "operation", &operation, truncatedCount, error, location)
        || !readStringField(object, "oldValue", &oldValue, truncatedCount, error, location)
        || !readStringField(object, "newValue", &newValue, truncatedCount, error, location)) {
        return false;
    }

    OperationRecord parsed;
    parsed.timestamp = timestamp;
    parsed.runtimeMs = runtimeMs;
    parsed.pageName = pageName;
    parsed.controlName = controlName;
    parsed.controlType = controlType;
    parsed.operation = operation;
    parsed.oldValue = oldValue;
    parsed.newValue = newValue;

    *record = parsed;
    return true;
}

bool HistoryArchiveIo::fileContainsDate(const QString &fileName, const QDate &date, bool jsonLines)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (jsonLines) {
        while (!file.atEnd()) {
            const QByteArray line = file.readLine().trimmed();
            if (line.isEmpty()) {
                continue;
            }
            const QJsonDocument document = QJsonDocument::fromJson(line);
            if (!document.isObject()) {
                continue;
            }
            const QDateTime timestamp = parseTimestamp(document.object().value(QStringLiteral("timestamp")).toString());
            return timestamp.isValid() && timestamp.date() == date;
        }
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray() || document.array().isEmpty()) {
        return false;
    }
    for (const QJsonValue &value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QDateTime timestamp = parseTimestamp(value.toObject().value(QStringLiteral("timestamp")).toString());
        if (timestamp.isValid()) {
            return timestamp.date() == date;
        }
    }
    return false;
}

HistoryDateScanResult HistoryArchiveIo::scanAvailableDates(const QString &directory, const QDate &today)
{
    Q_UNUSED(today)
    HistoryDateScanResult result;
    QDir dir(directory);
    if (!dir.exists()) {
        return result;
    }
    if (!QFileInfo(directory).isReadable()) {
        result.error = QStringLiteral("没有读取历史记录的权限");
        return result;
    }

    const QRegularExpression pattern(QStringLiteral("^operation_history_(\\d{8})\\.(jsonl|json)$"));
    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("operation_history_*.json")
                                                                << QStringLiteral("operation_history_*.jsonl"),
                                                  QDir::Files | QDir::Readable);
    QMap<QDate, QFileInfo> jsonlFiles;
    QMap<QDate, QFileInfo> jsonFiles;
    for (const QFileInfo &info : files) {
        const QRegularExpressionMatch match = pattern.match(info.fileName());
        if (!match.hasMatch()) {
            continue;
        }
        const QDate date = QDate::fromString(match.captured(1), QStringLiteral("yyyyMMdd"));
        if (!date.isValid() || info.size() <= 0 || info.size() > kMaxFileBytes) {
            continue;
        }
        if (info.suffix().compare(QStringLiteral("jsonl"), Qt::CaseInsensitive) == 0) {
            jsonlFiles.insert(date, info);
        } else {
            jsonFiles.insert(date, info);
        }
    }

    QSet<QDate> dates;
    for (auto it = jsonlFiles.cbegin(); it != jsonlFiles.cend(); ++it) {
        dates.insert(it.key());
    }
    for (auto it = jsonFiles.cbegin(); it != jsonFiles.cend(); ++it) {
        dates.insert(it.key());
    }
    QStringList unreadableDates;
    for (const QDate &date : dates) {
        const bool hasJsonl = jsonlFiles.contains(date);
        const bool hasJson = jsonFiles.contains(date);
        const HistoryLoadResult jsonlResult = hasJsonl
            ? loadDateFile(jsonlFiles.value(date).absoluteFilePath(), date)
            : HistoryLoadResult();
        const HistoryLoadResult jsonResult = hasJson
            ? loadDateFile(jsonFiles.value(date).absoluteFilePath(), date)
            : HistoryLoadResult();
        if (hasJsonl) {
            (void)fileContainsDate(jsonlFiles.value(date).absoluteFilePath(), date, true);
        }

        const bool jsonlClean = jsonlResult.ok()
            && jsonlResult.skippedLineCount == 0
            && jsonlResult.repairedLineCount == 0;
        const bool jsonClean = jsonResult.ok() && jsonResult.skippedLineCount == 0;
        HistoryDateEntry entry;
        entry.date = date;
        if (jsonlClean) {
            entry.path = jsonlFiles.value(date).absoluteFilePath();
            entry.jsonLines = true;
            entry.size = jsonlFiles.value(date).size();
        } else if (jsonlResult.ok() && jsonClean) {
            entry.path = jsonFiles.value(date).absoluteFilePath();
            entry.jsonLines = false;
            entry.size = jsonFiles.value(date).size();
        } else if (!jsonResult.ok() && jsonlResult.ok()) {
            entry.path = jsonlFiles.value(date).absoluteFilePath();
            entry.jsonLines = true;
            entry.size = jsonlFiles.value(date).size();
        } else if (jsonResult.ok()) {
            entry.path = jsonFiles.value(date).absoluteFilePath();
            entry.jsonLines = false;
            entry.size = jsonFiles.value(date).size();
        } else {
            unreadableDates.append(date.toString(QStringLiteral("yyyy-MM-dd")));
            continue;
        }
        result.entries.insert(date, entry);
    }
    if (!unreadableDates.isEmpty()) {
        result.warning = QStringLiteral("以下日期的历史文件无法读取：%1").arg(unreadableDates.join(QStringLiteral("，")));
    }
    return result;
}
