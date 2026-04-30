#include "historyrecordmodel.h"

#include <QModelIndex>

HistoryRecordModel::HistoryRecordModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int HistoryRecordModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_records.size();
}

QVariant HistoryRecordModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return QVariant();

    const OperationRecord &r = m_records.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return r.toString();
    case TimeRole:
        return r.timestamp.toString(QStringLiteral("hh:mm:ss"));
    case PageRole:
        return r.pageName;
    case ControlRole:
        return r.controlName;
    case ControlTypeRole:
        return r.controlType;
    case OpRole:
        return r.operation;
    case OldValRole:
        return r.oldValue.toString();
    case NewValRole:
        return r.newValue.toString();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> HistoryRecordModel::roleNames() const
{
    return {
        {TimeRole, "time"},
        {PageRole, "page"},
        {ControlRole, "control"},
        {ControlTypeRole, "controlType"},
        {OpRole, "op"},
        {OldValRole, "oldVal"},
        {NewValRole, "newVal"}
    };
}

QVariantMap HistoryRecordModel::get(int row) const
{
    QVariantMap out;
    if (row < 0 || row >= m_records.size())
        return out;

    const OperationRecord &r = m_records.at(row);
    out[QStringLiteral("controlType")] = r.controlType;
    out[QStringLiteral("op")] = r.operation;
    out[QStringLiteral("page")] = r.pageName;
    out[QStringLiteral("control")] = r.controlName;
    return out;
}

void HistoryRecordModel::loadFromRecords(const QList<OperationRecord> &records)
{
    beginResetModel();
    m_records = records;
    endResetModel();
    emit countChanged();
}

void HistoryRecordModel::appendRecord(const OperationRecord &record)
{
    const int row = m_records.size();
    beginInsertRows(QModelIndex(), row, row);
    m_records.append(record);
    endInsertRows();
    emit countChanged();
}

void HistoryRecordModel::clear()
{
    if (m_records.isEmpty())
        return;
    beginResetModel();
    m_records.clear();
    endResetModel();
    emit countChanged();
}
