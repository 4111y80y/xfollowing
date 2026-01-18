#ifndef POSTLISTPANEL_H
#define POSTLISTPANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QList>
#include "Data/Post.h"

class PostListPanel : public QWidget {
    Q_OBJECT

public:
    explicit PostListPanel(QWidget* parent = nullptr);

    void setPosts(const QList<Post>& posts);
    void setHideFollowed(bool hide);

signals:
    void postClicked(const Post& post);

private slots:
    void onItemClicked(int row, int column);

private:
    void updateTable();

    QTableWidget* m_tableWidget;
    QList<Post> m_posts;
    QList<Post> m_filteredPosts;
    bool m_hideFollowed;
};

#endif // POSTLISTPANEL_H
