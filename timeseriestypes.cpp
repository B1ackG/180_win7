#include "timeseriestypes.h"

#include <cstddef>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

struct ChannelInfo {
    TimeSeriesChannel channel;
    const char *column;
    const char *name;
    const char *unit;
    TimeSeriesSource source;
    int decimals;
};

const ChannelInfo kChannels[] = {
    {TimeSeriesChannel::J1AngleDeg, "j1_angle_deg", "悬臂角度", "°", TimeSeriesSource::Main, 1},
    {TimeSeriesChannel::J2HeightMm, "j2_height_mm", "升降高度", "mm", TimeSeriesSource::Main, 0},
    {TimeSeriesChannel::J3TotalLengthMm, "j3_total_length_mm", "总伸展长度", "mm", TimeSeriesSource::Main, 0},
    {TimeSeriesChannel::J4AngleDeg, "j4_angle_deg", "末端角度", "°", TimeSeriesSource::Main, 1},
    {TimeSeriesChannel::CounterweightHeightMm, "counterweight_height_mm", "配重缸升降", "mm", TimeSeriesSource::Main, 0},
    {TimeSeriesChannel::ColumnTorquePercent, "column_torque_percent", "立柱升降扭矩", "%", TimeSeriesSource::Main, 1},
    {TimeSeriesChannel::CounterweightTorquePercent, "counterweight_torque_percent", "后配重缸升降扭矩", "%", TimeSeriesSource::Main, 1},
    {TimeSeriesChannel::ArmTiltXDeg, "arm_tilt_x_deg", "悬臂X轴倾角", "°", TimeSeriesSource::Main, 1},
    {TimeSeriesChannel::ArmTiltYDeg, "arm_tilt_y_deg", "悬臂Y轴倾角", "°", TimeSeriesSource::Main, 1},
    {TimeSeriesChannel::PlaneHeightMm, "plane_height_mm", "装配面高度", "mm", TimeSeriesSource::Main, 0},
    {TimeSeriesChannel::LoadKg, "load_kg", "当前负载", "KG", TimeSeriesSource::Main, 0},
    {TimeSeriesChannel::TotalPowerW, "total_power_w", "总功率", "W", TimeSeriesSource::Main, 0},
    {TimeSeriesChannel::ChassisTiltXDeg, "chassis_tilt_x_deg", "底盘X轴倾角", "°", TimeSeriesSource::Agv, 1},
    {TimeSeriesChannel::ChassisTiltYDeg, "chassis_tilt_y_deg", "底盘Y轴倾角", "°", TimeSeriesSource::Agv, 1},
    {TimeSeriesChannel::ArmTorquePercent, "arm_torque_percent", "前配重缸升降扭矩", "%", TimeSeriesSource::Main, 1},
    {TimeSeriesChannel::FrontSupportForceKg, "front_support_force_kg", "前支撑缸支撑力", "KG", TimeSeriesSource::Main, 0},
};

static_assert(sizeof(kChannels) / sizeof(kChannels[0])
                  == static_cast<size_t>(TimeSeriesChannel::Count),
              "kChannels must match TimeSeriesChannel::Count");

const ChannelInfo *infoFor(TimeSeriesChannel channel)
{
    const int index = static_cast<int>(channel);
    if (index < 0 || index >= static_cast<int>(TimeSeriesChannel::Count)) {
        return nullptr;
    }
    return &kChannels[index];
}

int clampInt(int value, int minValue, int maxValue, int fallback)
{
    if (value < minValue || value > maxValue) {
        return fallback;
    }
    return value;
}

} // namespace

TimeSeriesConfig TimeSeriesConfig::fromIni(const QString &iniPath)
{
    QSettings settings(iniPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("TimeSeries"));

    TimeSeriesConfig config;
    config.enabled = settings.value(QStringLiteral("enabled"), true).toBool();
    config.sampleIntervalMs = settings.value(QStringLiteral("sample_interval_ms"),
                                             kDefaultSampleIntervalMs).toInt();
    config.flushIntervalMs = settings.value(QStringLiteral("flush_interval_ms"),
                                            kDefaultFlushIntervalMs).toInt();
    config.retentionDays = settings.value(QStringLiteral("retention_days"),
                                          kDefaultRetentionDays).toInt();
    config.reserveFreeMb = settings.value(QStringLiteral("reserve_free_mb"),
                                          kDefaultReserveFreeMb).toInt();
    config.maxQueueRows = settings.value(QStringLiteral("max_queue_rows"),
                                         kDefaultMaxQueueRows).toInt();
    config.databaseDirectory = settings.value(QStringLiteral("database_directory")).toString().trimmed();
    settings.endGroup();
    return config.normalized();
}

QString TimeSeriesConfig::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/TimeSeriesRecords");
}

QString TimeSeriesConfig::resolvedDirectory() const
{
    const QString trimmed = databaseDirectory.trimmed();
    return trimmed.isEmpty() ? defaultDirectory() : trimmed;
}

TimeSeriesConfig TimeSeriesConfig::normalized() const
{
    TimeSeriesConfig out = *this;
    out.sampleIntervalMs = clampInt(sampleIntervalMs,
                                    kMinSampleIntervalMs,
                                    kMaxSampleIntervalMs,
                                    kDefaultSampleIntervalMs);
    out.flushIntervalMs = clampInt(flushIntervalMs,
                                   kMinFlushIntervalMs,
                                   kMaxFlushIntervalMs,
                                   kDefaultFlushIntervalMs);
    out.retentionDays = clampInt(retentionDays,
                                 kMinRetentionDays,
                                 kMaxRetentionDays,
                                 kDefaultRetentionDays);
    out.reserveFreeMb = reserveFreeMb < kMinReserveFreeMb
        ? kDefaultReserveFreeMb
        : reserveFreeMb;
    out.maxQueueRows = clampInt(maxQueueRows,
                                kMinQueueRows,
                                kMaxQueueRows,
                                kDefaultMaxQueueRows);
    out.databaseDirectory = resolvedDirectory();
    return out;
}

namespace TimeSeriesMeta {

int channelCount()
{
    return static_cast<int>(TimeSeriesChannel::Count);
}

TimeSeriesChannel channelAt(int index)
{
    if (index < 0 || index >= channelCount()) {
        return TimeSeriesChannel::J1AngleDeg;
    }
    return static_cast<TimeSeriesChannel>(index);
}

QString columnName(TimeSeriesChannel channel)
{
    const ChannelInfo *info = infoFor(channel);
    return info ? QString::fromUtf8(info->column) : QString();
}

QString displayName(TimeSeriesChannel channel)
{
    const ChannelInfo *info = infoFor(channel);
    return info ? QString::fromUtf8(info->name) : QString();
}

QString unit(TimeSeriesChannel channel)
{
    const ChannelInfo *info = infoFor(channel);
    return info ? QString::fromUtf8(info->unit) : QString();
}

TimeSeriesSource source(TimeSeriesChannel channel)
{
    const ChannelInfo *info = infoFor(channel);
    return info ? info->source : TimeSeriesSource::Main;
}

bool isFiniteNumber(double value)
{
    // 0 is a real idle reading. Denormals are not: a torn LREAL (exponent
    // word still 0) decodes to ~1e-322 and plots as 0.0%.
    return std::isfinite(value) && (value == 0.0 || std::isnormal(value));
}

int displayDecimals(TimeSeriesChannel channel)
{
    const ChannelInfo *info = infoFor(channel);
    return info ? info->decimals : 1;
}

QVector<TimeSeriesChannel> allChannels()
{
    QVector<TimeSeriesChannel> channels;
    channels.reserve(channelCount());
    for (int i = 0; i < channelCount(); ++i) {
        channels.append(channelAt(i));
    }
    return channels;
}

QString dailyFileName(const QDate &date)
{
    return QStringLiteral("metrics_%1.db").arg(date.toString(QStringLiteral("yyyyMMdd")));
}

QString dailyFilePath(const QString &directory, const QDate &date)
{
    return QDir(directory).filePath(dailyFileName(date));
}

QVector<TimeSeriesChannel> conditionWorkChannels()
{
    return {
        TimeSeriesChannel::J1AngleDeg,
        TimeSeriesChannel::J2HeightMm,
        TimeSeriesChannel::J3TotalLengthMm,
        TimeSeriesChannel::J4AngleDeg,
        TimeSeriesChannel::CounterweightHeightMm,
        TimeSeriesChannel::PlaneHeightMm,
        TimeSeriesChannel::LoadKg
    };
}

QVector<TimeSeriesChannel> conditionStateChannels()
{
    return {
        TimeSeriesChannel::ColumnTorquePercent,
        TimeSeriesChannel::CounterweightTorquePercent,
        TimeSeriesChannel::ArmTorquePercent,
        TimeSeriesChannel::ArmTiltXDeg,
        TimeSeriesChannel::ArmTiltYDeg,
        TimeSeriesChannel::ChassisTiltXDeg,
        TimeSeriesChannel::ChassisTiltYDeg,
        TimeSeriesChannel::TotalPowerW,
        TimeSeriesChannel::FrontSupportForceKg
    };
}

int conditionQuantum(TimeSeriesChannel channel)
{
    switch (channel) {
    case TimeSeriesChannel::J2HeightMm:
    case TimeSeriesChannel::J3TotalLengthMm:
    case TimeSeriesChannel::CounterweightHeightMm:
    case TimeSeriesChannel::PlaneHeightMm:
        return 10;
    case TimeSeriesChannel::LoadKg:
    case TimeSeriesChannel::FrontSupportForceKg:
        return 5;
    case TimeSeriesChannel::TotalPowerW:
        return 10;
    default:
        return 1;
    }
}

qint64 conditionBucket(double value, int quantum)
{
    const int step = qMax(1, quantum);
    return qRound(value / static_cast<double>(step));
}

} // namespace TimeSeriesMeta
