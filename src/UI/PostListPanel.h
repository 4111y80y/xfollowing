#ifndef POSTLISTPANEL_H
#define POSTLISTPANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QList>
#include <QLabel>
#include <QPushButton>
#include "Data/Post.h"

class PostListPanel : public QWidget {
    Q_OBJECT

public:
    explicit PostListPanel(QWidget* parent = nullptr);

    void setPosts(const QList<Post>& posts);
    void setHideFollowed(bool hide);

signals:
    void postClicked(const Post& post);

public slots:
    void setPage(int page);
    void nextPage();
    void prevPage();
    void firstPage();
    void lastPage();

private slots:
    void onItemClicked(int row, int column);

private:
    void updateTable();
    void updatePageInfo();

    QTableWidget* m_tableWidget;
    QList<Post> m_posts;
    QList<Post> m_filteredPosts;
    bool m_hideFollowed;

    // 分页相关
    int m_currentPage = 0;
    int m_pageSize = 100;  // 每页100条
    int m_totalPages = 0;
    QLabel* m_pageLabel;
    QPushButton* m_firstBtn;
    QPushButton* m_prevBtn;
    QPushButton* m_nextBtn;
    QPushButton* m_lastBtn;
};

#endif // POSTLISTPANEL_H
