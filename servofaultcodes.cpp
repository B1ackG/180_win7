#include "servofaultcodes.h"

#include <QHash>

namespace ServoFaultCodes {
namespace {

struct CodeInfo {
    const char *name;
    bool resettable;
};

const QHash<quint32, CodeInfo> &codeTable()
{
    static const QHash<quint32, CodeInfo> table = [] {
        QHash<quint32, CodeInfo> out;
        auto addCode = [&out](quint32 code, const char *name, bool resettable) {
            out.insert(code, CodeInfo{name, resettable});
        };

        addCode(0x01080108u, "写入存储参数故障", true);
        addCode(0x11080108u, "读取存储参数故障", true);
        addCode(0x21080108u, "写EEPROM校验错误", true);
        addCode(0x31080108u, "读EEPROM校验错误", true);
        addCode(0x41080108u, "单个数据读取频繁", true);
        addCode(0x31200120u, "电机与驱动器功率不匹配", true);
        addCode(0x01210121u, "伺服使能指令重复", true);
        addCode(0x06000600u, "惯量辨识失败", true);
        addCode(0x06010601u, "原点回归警告", true);
        addCode(0x16010601u, "原点复归开关异常", true);
        addCode(0x26010601u, "回零模式设置异常", true);
        addCode(0x07300730u, "编码器电池警告", true);
        addCode(0x09000900u, "DI紧急刹车", true);
        addCode(0x09010901u, "停机方式无效警告", true);
        addCode(0x09020902u, "DI设置无效", true);
        addCode(0x19020902u, "DO设置无效", true);
        addCode(0x29020902u, "转矩到达设置无效", true);
        addCode(0x39020902u, "位置比较目标无效警告", true);
        addCode(0x09080908u, "机型识别校验码失败", true);
        addCode(0x09090909u, "电机过载警告", true);
        addCode(0x09100910u, "控制电过压", true);
        addCode(0x09200920u, "制动电阻过载", true);
        addCode(0x09210921u, "动态制动电阻过载", true);
        addCode(0x09220922u, "外接制动电阻阻值过小", true);
        addCode(0x09240924u, "泄放管过温", true);
        addCode(0x09410941u, "变更参数需重新上电生效", true);
        addCode(0x09420942u, "参数存储频繁", true);
        addCode(0x09500950u, "正向超程警告", true);
        addCode(0x09520952u, "反向超程警告", true);
        addCode(0x0A410A41u, "转矩波动补偿失败", true);

        addCode(0x01500150u, "STO进入安全状态", true);
        addCode(0x11500150u, "STO输入状态异常", true);
        addCode(0x21500150u, "Buffer5V电压检测异常", true);
        addCode(0x31500150u, "STO输入电路硬件诊断失败", true);
        addCode(0x41500150u, "PWM Buffer硬件诊断失败", true);
        addCode(0x22080208u, "编码器通讯超时", true);
        addCode(0x32080208u, "电流采样故障", true);
        addCode(0x42080208u, "FPGA电流环运算超时", true);
        addCode(0x03200320u, "制动电阻过载", true);
        addCode(0x03202320u, "泄放故障", true);
        addCode(0x03210321u, "动态制动电阻过载", true);
        addCode(0x04000400u, "主回路电过压", true);
        addCode(0x04100410u, "主回路电欠压", true);
        addCode(0x14100410u, "主回路断电", true);
        addCode(0x05000500u, "电机超速", true);
        addCode(0x15000500u, "速度反馈溢出", true);
        addCode(0x25000500u, "FPGA位置反馈脉冲过速", true);
        addCode(0x06020602u, "角度辨识失败", true);
        addCode(0x26020602u, "UVW三相相序接反", true);
        addCode(0x06050605u, "使能速度过快", true);
        addCode(0x06200620u, "电机过载", true);
        addCode(0x06300630u, "堵转电机过热保护", true);
        addCode(0x06400640u, "逆变IGBT结温过高", true);
        addCode(0x16400640u, "续流二极管结温过高", true);
        addCode(0x06500650u, "散热片温度过高", true);
        addCode(0x09390939u, "电机动力线断线", true);
        addCode(0x0B000B00u, "位置偏差过大", true);
        addCode(0x1B000B00u, "位置偏差溢出", true);

        addCode(0x01220122u, "多圈绝对值编码器设置错误", true);
        addCode(0x11220122u, "DI功能分配故障", true);
        addCode(0x21220122u, "DO功能分配故障", true);
        addCode(0x31220122u, "旋转模式上限过大", true);
        addCode(0x04200420u, "主回路电缺相", true);
        addCode(0x04300430u, "控制电源欠压", true);
        addCode(0x06610661u, "STune调整失败", true);
        addCode(0x06620662u, "ETune调整失败", true);
        addCode(0x06630663u, "ITune调整失败", true);
        addCode(0x06640664u, "共振过大", true);
        addCode(0x07310731u, "编码器电池失效与多圈圈数丢失", true);
        addCode(0x07330733u, "编码器多圈计数错误", true);
        addCode(0x07350735u, "编码器多圈计数溢出", true);
        addCode(0x07600760u, "编码器过热", true);
        addCode(0x1B010B01u, "位置指令增量单次过大", true);
        addCode(0x2B010B01u, "位置指令增量持续过大", true);
        addCode(0x3B010B01u, "指令溢出", true);
        addCode(0x4B010B01u, "单圈绝对值模式目标位置超过机械单圈位置的最大值", true);
        // 旧表曾写成 0x4B016B01，现场若仍下发该编码则归一到 EB01.4。
        addCode(0x4B016B01u, "单圈绝对值模式目标位置超过机械单圈位置的最大值", true);
        addCode(0x1B030B03u, "电子齿轮比设定超限-第一组电子齿轮比", true);
        addCode(0x0E080E08u, "同步信号丢失", true);
        addCode(0x1E080E08u, "状态切换异常", true);
        addCode(0x2E080E08u, "IRQ丢失", true);
        addCode(0x3E080E08u, "网线连接不可靠", true);
        addCode(0x4E080E08u, "数据丢帧保护异常", true);
        addCode(0x5E080E08u, "数据帧转发异常", true);
        addCode(0x6E080E08u, "数据更新超时异常", true);
        addCode(0x0E090E09u, "软限位位置设定错误", true);
        addCode(0x1E090E09u, "原点位置设定错误", true);
        addCode(0x2E090E09u, "齿轮比超限", true);
        addCode(0x3E090E09u, "无同步信号", true);
        addCode(0x5E090E09u, "PDO映射超限", true);
        addCode(0x0E100E10u, "MailBox设定异常保护", true);
        addCode(0x1E100E10u, "SM2配置异常", true);
        addCode(0x2E100E10u, "SM3配置异常", true);
        addCode(0x3E100E10u, "PDO看门狗设定异常", true);
        addCode(0x4E100E10u, "PLL未完成异常保护（没有sync信号错误）", true);
        addCode(0x5E100E10u, "PHY配置异常", true);
        addCode(0x0E110E11u, "ESI校验错误", true);
        addCode(0x1E110E11u, "总线读取EEPROM失败", true);
        addCode(0x2E110E11u, "总线更新EEPROM失败", true);
        addCode(0x3E110E11u, "ESI与驱动器型号不匹配", true);
        addCode(0x0E130E13u, "EtherCAT同步周期设定错误", true);
        addCode(0x0E150E15u, "EtherCAT同步周期误差过大", true);

        addCode(0x01010101u, "H02及以上功能码参数异常", false);
        addCode(0x11010101u, "H00/H01组参数异常", false);
        addCode(0x21010101u, "参数总个数变化读写时地址异常", false);
        addCode(0x01020102u, "FPGA通信建立的异常", false);
        addCode(0x11020102u, "FPGA初始化启动异常", false);
        addCode(0x81020102u, "FPGA与MCU版本号不匹配", false);
        addCode(0x11040104u, "MCU运行超时（MCU死机）", false);
        addCode(0x21040104u, "FPGA运行超时（FPGA死机）", false);
        addCode(0x41040104u, "MCU指令更新超时", false);
        addCode(0x01200120u, "无法识别的编码器类型", false);
        addCode(0x11200120u, "无对应型号电机", false);
        addCode(0x21200120u, "无对应型号驱动器", false);
        addCode(0x51200120u, "电机与驱动器电流匹配错误", false);
        addCode(0x61200120u, "FPGA与电机型号不匹配", false);
        addCode(0x71200120u, "机型参数校验错误", false);
        addCode(0x81200120u, "结温参数校验错误", false);
        addCode(0x01360136u, "编码器ROM电机参数校验异常", false);
        addCode(0x11360136u, "编码器ROM电机参数读取异常", false);
        addCode(0x21360136u, "转矩波动补偿数据校验错误", false);
        addCode(0x01400140u, "加密芯片校验故障", false);
        addCode(0x11400140u, "MCU密钥计算失败", false);
        addCode(0x21400140u, "加密芯片版本错误", false);
        addCode(0x31400140u, "加密芯片损坏", false);
        addCode(0x02010201u, "P相过流", false);
        addCode(0x12010201u, "U相过流", false);
        addCode(0x22010201u, "V相过流", false);
        addCode(0x42010201u, "N相过流", false);
        addCode(0x02100210u, "输出对地短路", false);
        addCode(0x02340234u, "飞车", false);
        addCode(0x07400740u, "绝对值编码器通讯超时", false);
        addCode(0x27400740u, "绝对值编码器错误", false);
        addCode(0x37400740u, "绝对值编码器单圈解算错误", false);
        addCode(0x67400740u, "编码器写入故障", false);
        addCode(0x07650765u, "尼康编码器过热或者过速", false);
        addCode(0x0A330A33u, "编码器读写校验异常", false);
        addCode(0x0E120E12u, "EtherCAT初始化失败", false);
        addCode(0x0E160E16u, "MCU与ESC通信异常", false);

        return out;
    }();
    return table;
}

quint32 canonicalCode(quint32 code)
{
    if (code == 0x4B016B01u) {
        return 0x4B010B01u;
    }
    return code;
}

CodeInfo lookupCode(quint32 code, quint32 &matchedCode)
{
    const auto &table = codeTable();
    auto it = table.constFind(code);
    if (it != table.cend()) {
        matchedCode = canonicalCode(code);
        return it.value();
    }
    return CodeInfo{nullptr, false};
}

} // namespace

QString displayCodeFromRaw(quint32 code)
{
    const quint16 ident = static_cast<quint16>(code & 0xFFFF);
    const int sub = static_cast<int>((code >> 28) & 0xF);
    const int prefix = (ident >> 8) & 0xFF;
    const int suffix = ident & 0xFF;
    return QStringLiteral("E%1%2.%3")
        .arg(prefix, 0, 16)
        .arg(suffix, 2, 16, QChar('0'))
        .arg(sub)
        .toUpper();
}

QVector<AxisFault> decodeRegisterWords(const QVector<quint16> &words)
{
    QVector<AxisFault> faults;
    const int pairCount = qMin(kAxisCount, words.size() / 2);
    for (int axisIndex = 0; axisIndex < pairCount; ++axisIndex) {
        const quint16 highWord = words.at(axisIndex * 2);
        const quint16 lowWord = words.at(axisIndex * 2 + 1);
        if (highWord == 0 && lowWord == 0) {
            continue;
        }

        const quint32 codeHighLow =
            (static_cast<quint32>(highWord) << 16) | static_cast<quint32>(lowWord);
        const quint32 codeLowHigh =
            (static_cast<quint32>(lowWord) << 16) | static_cast<quint32>(highWord);

        quint32 matchedCode = codeHighLow;
        CodeInfo info = lookupCode(codeHighLow, matchedCode);
        if (info.name == nullptr) {
            info = lookupCode(codeLowHigh, matchedCode);
        }
        if (info.name == nullptr) {
            matchedCode = codeHighLow;
        }

        AxisFault fault;
        fault.axisIndex = axisIndex + 1;
        fault.rawCode = matchedCode;
        fault.displayCode = displayCodeFromRaw(matchedCode);
        fault.known = (info.name != nullptr);
        fault.name = fault.known
            ? QString::fromUtf8(info.name)
            : QStringLiteral("未知故障");
        fault.resettable = !fault.known || info.resettable;
        faults.append(fault);
    }
    return faults;
}

QStringList faultLines(const QVector<AxisFault> &faults)
{
    QStringList lines;
    lines.reserve(faults.size());
    for (const AxisFault &fault : faults) {
        lines.append(QStringLiteral("%1轴发生%2（%3）")
                         .arg(fault.axisIndex)
                         .arg(fault.name)
                         .arg(fault.displayCode));
    }
    return lines;
}

QStringList historyLines(const QVector<AxisFault> &faults)
{
    QStringList lines;
    lines.reserve(faults.size());
    for (const AxisFault &fault : faults) {
        lines.append(QStringLiteral("%1轴发生%2")
                         .arg(fault.axisIndex)
                         .arg(fault.name));
    }
    return lines;
}

QString historySummary(const QVector<AxisFault> &faults)
{
    return historyLines(faults).join(QStringLiteral("，"));
}

QString faultSignature(const QVector<AxisFault> &faults)
{
    QStringList parts;
    parts.reserve(faults.size());
    for (const AxisFault &fault : faults) {
        parts.append(QStringLiteral("%1:%2").arg(fault.axisIndex).arg(fault.displayCode));
    }
    return parts.join(QLatin1Char('|'));
}

bool hasKnownNonResettable(const QVector<AxisFault> &faults)
{
    for (const AxisFault &fault : faults) {
        if (fault.known && !fault.resettable) {
            return true;
        }
    }
    return false;
}

bool hasResettableFault(const QVector<AxisFault> &faults)
{
    for (const AxisFault &fault : faults) {
        if (fault.resettable) {
            return true;
        }
    }
    return false;
}

} // namespace ServoFaultCodes
