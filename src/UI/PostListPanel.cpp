#include "PostListPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

PostListPanel::PostListPanel(QWidget* parent)
    : QWidget(parent)
    , m_hideFollowed(false)
    , m_currentPage(0)
    , m_pageSize(100)
    , m_totalPages(0) {

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

    // 分页控件
    QHBoxLayout* pageLayout = new QHBoxLayout();
    pageLayout->setContentsMargins(5, 5, 5, 5);

    m_firstBtn = new QPushButton("<<", this);
    m_prevBtn = new QPushButton("<", this);
    m_pageLabel = new QLabel("Page 0/0 (Total: 0)", this);
    m_nextBtn = new QPushButton(">", this);
    m_lastBtn = new QPushButton(">>", this);

    m_firstBtn->setFixedWidth(40);
    m_prevBtn->setFixedWidth(40);
    m_nextBtn->setFixedWidth(40);
    m_lastBtn->setFixedWidth(40);

    pageLayout->addStretch();
    pageLayout->addWidget(m_firstBtn);
    pageLayout->addWidget(m_prevBtn);
    pageLayout->addWidget(m_pageLabel);
    pageLayout->addWidget(m_nextBtn);
    pageLayout->addWidget(m_lastBtn);
    pageLayout->addStretch();

    layout->addLayout(pageLayout);

    connect(m_tableWidget, &QTableWidget::cellClicked, this, &PostListPanel::onItemClicked);
    connect(m_firstBtn, &QPushButton::clicked, this, &PostListPanel::firstPage);
    connect(m_prevBtn, &QPushButton::clicked, this, &PostListPanel::prevPage);
    connect(m_nextBtn, &QPushButton::clicked, this, &PostListPanel::nextPage);
    connect(m_lastBtn, &QPushButton::clicked, this, &PostListPanel::lastPage);
}

void PostListPanel::setPosts(const QList<Post>& posts) {
    m_posts = posts;
    // 重置到第一页
    m_currentPage = 0;
    updateTable();
}

void PostListPanel::setHideFollowed(bool hide) {
    m_hideFollowed = hide;
    // 重置到第一页
    m_currentPage = 0;
    updateTable();
}

void PostListPanel::updateTable() {
    m_tableWidget->setRowCount(0);
    m_filteredPosts.clear();

    // 固定作者的handle，不会被隐藏
    const QString pinnedAuthorHandle = "4111y80y";

    for (const auto& post : m_posts) {
        // 固定帖子永远显示，不会被隐藏
        bool isPinned = (post.authorHandle == pinnedAuthorHandle);

        if (m_hideFollowed && post.isFollowed && !isPinned) {
            continue;
        }
        m_filteredPosts.append(post);
    }

    // 计算分页信息
    m_totalPages = (m_filteredPosts.size() + m_pageSize - 1) / m_pageSize;
    if (m_totalPages == 0) m_totalPages = 1;

    // 确保当前页有效
    if (m_currentPage >= m_totalPages) {
        m_currentPage = m_totalPages - 1;
    }
    if (m_currentPage < 0) {
        m_currentPage = 0;
    }

    // 计算当前页的数据范围
    int start = m_currentPage * m_pageSize;
    int end = qMin(start + m_pageSize, m_filteredPosts.size());

    // 只渲染当前页的数据
    for (int i = start; i < end; ++i) {
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

    // 更新分页信息显示
    updatePageInfo();
}

void PostListPanel::updatePageInfo() {
    m_pageLabel->setText(QString("Page %1/%2 (Total: %3)")
        .arg(m_currentPage + 1)
        .arg(m_totalPages)
        .arg(m_filteredPosts.size()));

    // 更新按钮状态
    m_firstBtn->setEnabled(m_currentPage > 0);
    m_prevBtn->setEnabled(m_currentPage > 0);
    m_nextBtn->setEnabled(m_currentPage < m_totalPages - 1);
    m_lastBtn->setEnabled(m_currentPage < m_totalPages - 1);
}

void PostListPanel::setPage(int page) {
    if (page >= 0 && page < m_totalPages && page != m_currentPage) {
        m_currentPage = page;
        updateTable();
    }
}

void PostListPanel::nextPage() {
    setPage(m_currentPage + 1);
}

void PostListPanel::prevPage() {
    setPage(m_currentPage - 1);
}

void PostListPanel::firstPage() {
    setPage(0);
}

void PostListPanel::lastPage() {
    setPage(m_totalPages - 1);
}

void PostListPanel::onItemClicked(int row, int column) {
    Q_UNUSED(column);

    // 转换为 m_filteredPosts 中的实际索引
    int actualIndex = m_currentPage * m_pageSize + row;
    if (actualIndex >= 0 && actualIndex < m_filteredPosts.size()) {
        emit postClicked(m_filteredPosts[actualIndex]);
    }
}
