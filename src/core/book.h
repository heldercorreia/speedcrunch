// SPDX-FileCopyrightText: 2013-2014, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_BOOK_H
#define CORE_BOOK_H

#include "core/pageserver.h"

#include <QList>

class Book : public PageServer {
    Q_OBJECT

public:
    explicit Book(QObject* parent = 0) : PageServer(parent) { createPages(); }
    virtual void createPages();
    QString searchResultsPage(const QString& query);
    void invalidateSearchIndex() { m_searchIndex.clear(); }

private:
    Q_DISABLE_COPY(Book)

    struct SearchPage {
        QString path;
        QString title;
        QString text;
    };

    void buildSearchIndex();
    QList<SearchPage> m_searchIndex;
};

#endif // CORE_BOOK_H
