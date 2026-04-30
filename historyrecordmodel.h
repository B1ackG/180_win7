#ifndef HISTORYRECORDMODEL_H
#define HISTORYRECORDMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>
#include <QList>

#include "operationrecorder.h"

/**
 * @brief 供 HistoryList.qml 使用的操作记录列表模型。
 */
class HistoryRecordModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        TimeRole = Qt::UserRole + 1,
        PageRole,
        ControlRole,
        ControlTypeRole,
        OpRole,
        OldValRole,
        NewValRole
    };

    explicit HistoryRecordModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;

    void loadFromRecords(const QList<OperationRecord> &records);
    void appendRecord(const OperationRecord &record);
    void clear();

signals:
    void countChanged();

private:
    QList<OperationRecord> m_records;
};

#endif // HISTORYRECORDMODEL_H
