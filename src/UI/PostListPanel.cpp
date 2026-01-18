#include "PostListPanel.h"
#include <QVBoxLayout>
#include <QHeaderView>

PostListPanel::PostListPanel(QWidget* parent)
    : QWidget(parent)
    , m_hideFollowed(false) {

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(4);
    m_tableWidget->setHorizontalHeaderLabels({"作者", "内容", "关键词", "状态"});
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setAlternatingRowColors(true);

    // 设置列宽
    m_tableWidget->setColumnWidth(0, 120);
    m_tableWidget->setColumnWidth(2, 80);
    m_tableWidget->setColumnWidth(3, 60);

    layout->addWidget(m_tableWidget);

    connect(m_tableWidget, &QTableWidget::cellClicked, this, &PostListPanel::onItemClicked);
}

void PostListPanel::setPosts(const QList<Post>& posts) {
    m_posts = posts;
    updateTable();
}

void PostListPanel::setHideFollowed(bool hide) {
    m_hideFollowed = hide;
    updateTable();
}

void PostListPanel::updateTable() {
    m_tableWidget->setRowCount(0);
    m_filteredPosts.clear();

    for (const auto& post : m_posts) {
        if (m_hideFollowed && post.isFollowed) {
            continue;
        }
        m_filteredPosts.append(post);
    }

    for (int i = 0; i < m_filteredPosts.size(); ++i) {
        const Post& post = m_filteredPosts[i];
        int row = m_tableWidget->rowCount();
        m_tableWidget->insertRow(row);

        // 作者
        QTableWidgetItem* authorItem = new QTableWidgetItem("@" + post.authorHandle);
        authorItem->setToolTip(post.authorName);
        m_tableWidget->setItem(row, 0, authorItem);

        // 内容
        QString contentPreview = post.content.left(50);
        if (post.content.length() > 50) {
            contentPreview += "...";
        }
        QTableWidgetItem* contentItem = new QTableWidgetItem(contentPreview);
        contentItem->setToolTip(post.content);
        m_tableWidget->setItem(row, 1, contentItem);

        // 关键词
        m_tableWidget->setItem(row, 2, new QTableWidgetItem(post.matchedKeyword));

        // 状态
        QString status = post.isFollowed ? "[v]" : "";
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        if (post.isFollowed) {
            statusItem->setForeground(Qt::green);
        }
        m_tableWidget->setItem(row, 3, statusItem);
    }
}

void PostListPanel::onItemClicked(int row, int column) {
    Q_UNUSED(column);

    if (row >= 0 && row < m_filteredPosts.size()) {
        emit postClicked(m_filteredPosts[row]);
    }
}
