#ifndef FEATURESWITCHWIDGET_H
#define FEATURESWITCHWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QMap>
#include <QString>

class QLineEdit;
class QVBoxLayout;
class QShowEvent;
class TechVirtualKeyboard;

/**
 * @brief 功能开关管理页面 (厂家专用)
 * 允许实时调整大/小功能的启用状态，并支持保存到配置文件。
 */
class FeatureSwitchWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FeatureSwitchWidget(QWidget *parent = nullptr);

signals:
    void runtimeSettingsChanged();
    void operationHistoryEntry(const QString &controlName,
                               const QString &oldValue,
                               const QString &newValue);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    bool onApply();
    void onSave();
    void onReload();
    void onToggleAll(bool checked);

private:
    struct LimitEdits {
        QLineEdit *minEdit = nullptr;
        QLineEdit *maxEdit = nullptr;
    };
    struct ArcGaugeEdits {
        QCheckBox *visible = nullptr;
        QLineEdit *minEdit = nullptr;
        QLineEdit *maxEdit = nullptr;
    };
    struct SliderEditEdits {
        QCheckBox *visible = nullptr;
        QLineEdit *displayMinEdit = nullptr;
        QLineEdit *displayMaxEdit = nullptr;
    };

    void setupUI();
    void loadCurrentState();
    void setupPollingUI(QVBoxLayout *scrollLayout);
    void setupSliderLimitUI(QVBoxLayout *scrollLayout);
    void setupTechSliderEditUI(QVBoxLayout *scrollLayout);
    void refreshTechSliderEditList();
    void setupPlaneHeightOffsetUI(QVBoxLayout *scrollLayout);
    void setupDeviceControlChildVisibilityUI(QVBoxLayout *scrollLayout);

    bool loadPollingState();
    bool savePollingState(QString *error);
    bool loadSliderLimitState();
    bool saveSliderLimitState(QString *error);
    bool loadTechSliderEditState();
    bool saveTechSliderEditState(QString *error);
    bool loadPlaneHeightOffsetState();
    bool savePlaneHeightOffsetState(QString *error);
    bool loadButtonVisibilityState();
    bool saveButtonVisibilityState(QString *error);

    bool validateIntEdit(QLineEdit *edit, int min, int max, int fallback, QString *error);
    bool validateDoubleEdit(QLineEdit *edit, double min, double max, double fallback, QString *error);
    bool validateRange(QLineEdit *minEdit, QLineEdit *maxEdit,
                       double fallbackMin, double fallbackMax, QString *error);
    bool validateAllInputs(QString *error);
    bool settingsWriteOk(class QSettings &settings, const QString &fileName, QString *error) const;
    void rememberValidEditText(QLineEdit *edit, const QString &text = QString());
    QString lastValidEditText(const QLineEdit *edit, const QString &fallback) const;
    void restoreLineEditText(QLineEdit *edit, const QString &text);
    void showConsoleError(const QString &message);
    void wireIntEdit(QLineEdit *edit, int min, int max, int fallback);
    void wireDoubleEdit(QLineEdit *edit, double min, double max, double fallback);
    QLineEdit *makeLimitEdit(QWidget *parent);
    void recordIniChanges(const QString &fileName, const QString &group,
                          const QMap<QString, QString> &before);
    void emitHistoryIfChanged(const QString &controlName,
                              const QString &oldValue,
                              const QString &newValue);

    QMap<QString, QCheckBox*> m_bigCheckboxes;
    QMap<QString, QCheckBox*> m_smallCheckboxes;

    QLineEdit *m_editMainModbusPoll = nullptr;
    QLineEdit *m_editMainUiPoll = nullptr;
    QLineEdit *m_editMainDeviceStatusPoll = nullptr;
    QLineEdit *m_editMainDeviceStatusStart = nullptr;
    QLineEdit *m_editMainDeviceStatusCount = nullptr;
    QLineEdit *m_editMainControlSyncStart = nullptr;
    QLineEdit *m_editMainControlSyncCount = nullptr;
    QLineEdit *m_editMainReconnect = nullptr;
    QLineEdit *m_editAgvPoll = nullptr;
    QLineEdit *m_editAgvReconnect = nullptr;
    QCheckBox *m_cbMainUiStateSync = nullptr;
    QCheckBox *m_cbAgvUiStateSync = nullptr;

    QMap<QString, ArcGaugeEdits> m_arcGaugeEdits;
    QMap<QString, LimitEdits> m_limitEdits;
    QMap<QString, SliderEditEdits> m_sliderEditEdits;
    QMap<QString, QCheckBox*> m_deviceControlChildCheckboxes;
    QLineEdit *m_editPlaneHeightOffset = nullptr;
    QWidget *m_sliderEditListHost = nullptr;
    QVBoxLayout *m_sliderEditListLayout = nullptr;

    TechVirtualKeyboard *m_virtualKeyboard = nullptr;
};

#endif // FEATURESWITCHWIDGET_H
