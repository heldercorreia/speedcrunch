// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_VERSIONCHECK_H
#define GUI_VERSIONCHECK_H

#include <QObject>
#include <QPointer>
#include <QNetworkReply>
#include <QString>
#include <QThread>
#include <QVector>

class QWidget;
class VersionCheckWorker;

class VersionCheck : public QObject {
    Q_OBJECT

public:
    enum CheckTrigger {
        CheckTriggerAutomatic,
        CheckTriggerManual
    };

    explicit VersionCheck(QWidget* parentWindow, QObject* parent = 0);
    ~VersionCheck() override;

    void checkForUpdateAfterFirstPaint();
    void checkForUpdateIfDue();
    void checkForUpdateNow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void handleReplyFinished(QNetworkReply::NetworkError error, const QByteArray& payload,
                             int httpStatus, const QString& errorString);
    static bool compareVersions(const QString& left, const QString& right, int* result);
    static bool tryParseVersion(const QString& version, QVector<qulonglong>* components);
    static void showUpdateDialog(QWidget* parent, const QString& latestVersion);
    static void showUpToDateDialog(QWidget* parent, const QString& currentVersion);
    static void showNoConnectivityDialog(QWidget* parent);
    static void showVerificationFailedDialog(QWidget* parent);
    static bool isConnectivityError(QNetworkReply::NetworkError error);

    void markCheckAttempted();
    bool isCheckDue() const;
    void startRequest(CheckTrigger trigger);
    void storeFetchedVersion(const QString& version);
    bool shouldNotifyForVersion(const QString& version) const;
    void markVersionNotified(const QString& version);

    QPointer<QWidget> m_parentWindow;
    QPointer<QThread> m_networkThread;
    VersionCheckWorker* m_networkWorker = nullptr;
    bool m_requestPending = false;
    CheckTrigger m_trigger = CheckTriggerAutomatic;
    bool m_automaticCheckScheduled = false;
};

#endif
