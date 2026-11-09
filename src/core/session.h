// SPDX-FileCopyrightText: 2015-2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_SESSION_H
#define CORE_SESSION_H

#include "hmath.h"
#include "sessionhistory.h"
#include "variable.h"
#include "userfunction.h"
#include "userunit.h"
#include <QList>
#include <QString>
#include <QJsonArray>
#include <functional>
#include <memory>

class Evaluator;
class Session {
private:
    typedef QList<HistoryEntry> History ;
    // History is loaded on first use, so these change in const accessors.
    mutable History m_history;
    mutable int m_historyHead = 0; // Logical index 0 maps to physical m_historyHead.
    // Restored entries stay as JSON until something reads the history.
    // Sessions in hidden tabs then skip converting all their entries.
    // m_history is empty while saved entries are waiting.
    mutable QJsonArray m_savedHistory;
    mutable qsizetype m_savedHistoryFirst = 0;
    // Sessions in hidden tabs read their saved file when first used. Until
    // then only the name is known, and every other accessor reads the file.
    mutable std::function<void(Session&)> m_deferredFileLoad;
    mutable QString m_deferredFilePath;
    std::unique_ptr<Evaluator> m_evaluator;
    QString m_name;
    QString m_editorText;
    bool m_private = false;
    int m_historyLimit = 1000; // 0: unlimited.
    int physicalHistoryIndex(int logicalIndex) const;
    void normalizeHistoryOrder();
    void bindEvaluator();
    // Every history accessor checks for deferred entries first, so this also
    // reads an unread file.
    bool hasSavedHistory() const
    {
        loadDeferredFile();
        return m_savedHistory.size() > m_savedHistoryFirst;
    }
    void loadSavedHistory() const;
    void convertSavedHistory(const QJsonArray& history, qsizetype first) const;
    void readDeferredFile() const;

public:
    Session();
    Session(QJsonObject & json);
    Session(const Session& other);
    Session& operator=(const Session& other);
    ~Session();

    Evaluator* evaluator() { loadDeferredFile(); return m_evaluator.get(); }
    const Evaluator* evaluator() const { loadDeferredFile(); return m_evaluator.get(); }

    void load();
    void save();

    // Leaves the saved file unread until something other than the name or
    // the private flag is used. The loader fills the session from the file.
    void deferFile(const QString& filePath, std::function<void(Session&)> load);
    bool hasDeferredFile() const { return bool(m_deferredFileLoad); }
    QString deferredFilePath() const { return m_deferredFilePath; }
    void loadDeferredFile() const { if (m_deferredFileLoad) readDeferredFile(); }

    void serialize(QJsonObject &json) const;
    int deSerialize(const QJsonObject & json, bool merge);
    QString name() const { return m_name; }
    void setName(const QString& name);
    // Private sessions are never saved, so a session with an unread file is not private.
    bool isPrivate() const { return m_private; }
    void setPrivate(bool value) { loadDeferredFile(); m_private = value; }
    QString editorText() const { loadDeferredFile(); return m_editorText; }
    void setEditorText(const QString& text) { loadDeferredFile(); m_editorText = text; }
    int historyLimit() const { loadDeferredFile(); return m_historyLimit; }
    void setHistoryLimit(int limit);


    void addVariable(const Variable & var);
    bool hasVariable(const QString & id) const;
    void removeVariable(const QString & id);
    void clearVariables();
    Variable getVariable(const QString & id) const;
    QList<Variable> variablesToList() const;
    bool isBuiltInVariable(const QString &id) const;

    void addHistoryEntry(const HistoryEntry & entry);
    void appendSavedHistory(const QJsonArray& history);
    void deferSavedHistory(const QJsonArray& history);
    bool hasDeferredHistory() const { return hasSavedHistory(); }
    QJsonArray serializedHistory() const;
    bool nextHistoryEntryReachesLimit() const;
    void insertHistoryEntry(const int index, const HistoryEntry & entry);
    void removeHistoryEntryAt(const int index);
    int historySize() const;
    bool historyIsEmpty() const { return historySize() == 0; }
    const HistoryEntry& historyEntryAtRef(const int index) const;
    HistoryEntry historyEntryAt(const int index) const;
    // Reads one entry without loading the rest of the deferred history.
    HistoryEntry peekHistoryEntry(const int index) const;
    QList<HistoryEntry> historyToList() const;
    void applyHistoryLimit();
    void clearHistory();

    void addUserFunction(const UserFunction & func);
    void removeUserFunction(const QString & str);
    void clearUserFunctions();
    bool hasUserFunction(const QString & str) const;
    QList<UserFunction> UserFunctionsToList() const;
    const UserFunction * getUserFunction(const QString & fname) const;

    void addUserUnit(const UserUnit& unit);
    void removeUserUnit(const QString& name);
    void clearUserUnits();
    bool hasUserUnit(const QString& name) const;
    QList<UserUnit> userUnitsToList() const;
    const UserUnit* getUserUnit(const QString& name) const;
};

#endif // CORE_SESSION_H
