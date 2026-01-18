#include "KeywordPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>

KeywordPanel::KeywordPanel(QWidget* parent)
    : QWidget(parent) {

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* groupBox = new QGroupBox("监控关键词", this);
    QVBoxLayout* layout = new QVBoxLayout(groupBox);

    // 输入区域
    QHBoxLayout* inputLayout = new QHBoxLayout();
    m_inputEdit = new QLineEdit(groupBox);
    m_inputEdit->setPlaceholderText("输入关键词...");
    m_addBtn = new QPushButton("添加", groupBox);
    inputLayout->addWidget(m_inputEdit);
    inputLayout->addWidget(m_addBtn);
    layout->addLayout(inputLayout);

    // 列表
    m_listWidget = new QListWidget(groupBox);
    m_listWidget->setMaximumHeight(150);
    layout->addWidget(m_listWidget);

    // 删除按钮
    m_deleteBtn = new QPushButton("删除选中", groupBox);
    layout->addWidget(m_deleteBtn);

    mainLayout->addWidget(groupBox);

    // 连接信号
    connect(m_addBtn, &QPushButton::clicked, this, &KeywordPanel::onAddClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &KeywordPanel::onDeleteClicked);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &KeywordPanel::onAddClicked);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &KeywordPanel::onItemDoubleClicked);
}

void KeywordPanel::setKeywords(const QList<Keyword>& keywords) {
    m_keywords = keywords;
    updateList();
}

QList<Keyword> KeywordPanel::getKeywords() const {
    return m_keywords;
}

void KeywordPanel::updateList() {
    m_listWidget->clear();
    for (const auto& kw : m_keywords) {
        QListWidgetItem* item = new QListWidgetItem(kw.text);
        item->setData(Qt::UserRole, kw.id);
        if (!kw.isEnabled) {
            item->setForeground(Qt::gray);
        }
        m_listWidget->addItem(item);
    }
}

void KeywordPanel::onAddClicked() {
    QString text = m_inputEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    // 检查是否已存在
    for (const auto& kw : m_keywords) {
        if (kw.text == text) {
            QMessageBox::warning(this, "提示", "该关键词已存在");
            return;
        }
    }

    Keyword kw(text);
    m_keywords.append(kw);
    updateList();
    m_inputEdit->clear();

    emit keywordsChanged();
}

void KeywordPanel::onDeleteClicked() {
    QListWidgetItem* item = m_listWidget->currentItem();
    if (!item) {
        return;
    }

    QString id = item->data(Qt::UserRole).toString();
    for (int i = 0; i < m_keywords.size(); ++i) {
        if (m_keywords[i].id == id) {
            m_keywords.removeAt(i);
            break;
        }
    }

    updateList();
    emit keywordsChanged();
}

void KeywordPanel::onItemDoubleClicked(QListWidgetItem* item) {
    QString id = item->data(Qt::UserRole).toString();
    for (int i = 0; i < m_keywords.size(); ++i) {
        if (m_keywords[i].id == id) {
            m_keywords[i].isEnabled = !m_keywords[i].isEnabled;
            break;
        }
    }

    updateList();
    emit keywordsChanged();
}
