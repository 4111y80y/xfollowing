#ifndef KEYWORDPANEL_H
#define KEYWORDPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QList>
#include "Data/Keyword.h"

class KeywordPanel : public QWidget {
    Q_OBJECT

public:
    explicit KeywordPanel(QWidget* parent = nullptr);

    void setKeywords(const QList<Keyword>& keywords);
    QList<Keyword> getKeywords() const;

signals:
    void keywordsChanged();

private slots:
    void onAddClicked();
    void onDeleteClicked();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void updateList();

    QLineEdit* m_inputEdit;
    QPushButton* m_addBtn;
    QPushButton* m_deleteBtn;
    QListWidget* m_listWidget;

    QList<Keyword> m_keywords;
};

#endif // KEYWORDPANEL_H
