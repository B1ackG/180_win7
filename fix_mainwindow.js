const fs = require('fs');

const path = '/home/l/work/180_win7/mainwindow.cpp';
let content = fs.readFileSync(path, 'utf8');

// Insert at the end of constructor
const initAutoSaveStr = `    m_recorder->initAutoSave(); // 初始化操作记录自动保存`;
if (!content.includes(initAutoSaveStr)) {
    content = content.replace(
        /(m_recorder = new OperationRecorder\(this\);)/,
        `$1\n${initAutoSaveStr}`
    );
}

// Update the auto save UI section
const uiUpdateStr = `            if (m_recorder->recordCount() > 0) {
                QString autoSaveInfo = QString("自动保存: 已启用 | 记录数: %1 | 运行时长: %2分钟")
                    .arg(m_recorder->recordCount())
                    .arg(m_recorder->getRuntimeDuration() / 60000);

                // 可以添加一个标签显示这个信息
                QLabel *autoSaveLabel = recordPage->findChild<QLabel*>("autoSaveInfo");
                if (autoSaveLabel) {
                    autoSaveLabel->setText(autoSaveInfo);
                }
            }`;

if (!content.includes('autoSaveInfo')) {
    content = content.replace(
        /(titleTcpLabel->setStyleSheet.*?;[\s\S]*?)(        \/\/ 清空并显示新内容)/,
        `$1        // 显示自动保存信息\n${uiUpdateStr}\n\n$2`
    );
}

fs.writeFileSync(path, content);
