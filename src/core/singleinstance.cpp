// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "singleinstance.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QThread>
#include <QVariant>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int maximumMessageSize = 16384;

bool shuttingDown()
{
    return QCoreApplication::instance()->property("speedcrunchShutdownInProgress").toBool();
}
}

LaunchRequest LaunchRequest::fromEnvironment()
{
    return {qgetenv("XDG_ACTIVATION_TOKEN"), qgetenv("DESKTOP_STARTUP_ID")};
}

QByteArray LaunchRequest::encode() const
{
    if (activationToken.isEmpty() && startupId.isEmpty() && requestId.isEmpty())
        return QByteArrayLiteral("activate\n");
    QJsonObject metadata;
    metadata.insert(QStringLiteral("token"), QString::fromUtf8(activationToken));
    metadata.insert(QStringLiteral("startupId"), QString::fromUtf8(startupId));
    metadata.insert(QStringLiteral("id"), requestId);
    return QByteArrayLiteral("activate ") + QJsonDocument(metadata).toJson(QJsonDocument::Compact) + '\n';
}

bool LaunchRequest::decode(const QByteArray& message, LaunchRequest* request)
{
    if (!request || message.size() > maximumMessageSize)
        return false;
    const QByteArray line = message.trimmed();
    if (line == QByteArrayLiteral("activate")) {
        *request = {{}, {}, {}};
        return true;
    }
    if (!line.startsWith("activate "))
        return false;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line.mid(9), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    const QJsonObject object = document.object();
    const QJsonValue token = object.value(QStringLiteral("token"));
    const QJsonValue startupId = object.value(QStringLiteral("startupId"));
    const QJsonValue id = object.value(QStringLiteral("id"));
    if ((!token.isUndefined() && !token.isString())
        || (!startupId.isUndefined() && !startupId.isString())
        || (!id.isUndefined() && (!id.isString() || id.toString().size() > 128)))
        return false;
    *request = {token.toString().toUtf8(), startupId.toString().toUtf8(), id.toString()};
    return true;
}

SingleInstance::SingleInstance(QObject* parent) : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, [this]() {
        while (m_server.hasPendingConnections()) {
            QLocalSocket* socket = m_server.nextPendingConnection();
            if (!socket)
                continue;
            socket->setReadBufferSize(maximumMessageSize + 1);
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer* timeout = new QTimer(socket);
            timeout->setSingleShot(true);
            connect(timeout, &QTimer::timeout, socket, &QLocalSocket::abort);
            timeout->start(2000);
            const auto readRequest = [this, socket, timeout]() {
                if (socket->property("requestHandled").toBool())
                    return;
                if (!socket->canReadLine() && socket->bytesAvailable() <= maximumMessageSize)
                    return;
                socket->setProperty("requestHandled", true);
                timeout->stop();
                LaunchRequest request;
                const bool valid = socket->bytesAvailable() <= maximumMessageSize
                    && LaunchRequest::decode(socket->readLine(maximumMessageSize + 1), &request);
                const bool accepted = valid && m_server.isListening() && !shuttingDown();
                socket->write(!valid ? "invalid\n" : accepted ? "accepted\n" : "shutting-down\n");
                socket->disconnectFromServer();
                if (!accepted || (!request.requestId.isEmpty() && m_acceptedRequests.contains(request.requestId)))
                    return;
                // The launcher can retry after a startup timeout. Acknowledge
                // that retry without creating a second window or tab.
                if (!request.requestId.isEmpty()) {
                    m_acceptedRequests.insert(request.requestId);
                    m_requestOrder.enqueue(request.requestId);
                    if (m_requestOrder.size() > 1024)
                        m_acceptedRequests.remove(m_requestOrder.dequeue());
                }
                emit launchRequested(request);
            };
            connect(socket, &QLocalSocket::readyRead, socket, readRequest);
            readRequest();
        }
    });
}

SingleInstance::~SingleInstance()
{
    close();
}

bool SingleInstance::notifyRunningInstance(const QString& name, const LaunchRequest& request)
{
    QLocalSocket socket;
    socket.setReadBufferSize(256);
    socket.connectToServer(name, QIODevice::ReadWrite);
    if (!socket.waitForConnected(250))
        return false;
#ifdef Q_OS_WIN
    // The process launched by the user can pass its foreground permission to
    // the running instance before asking that instance to show a window.
    ULONG processId = 0;
    if (GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(socket.socketDescriptor()), &processId))
        AllowSetForegroundWindow(processId);
#endif
    const QByteArray message = request.encode();
    if (socket.write(message) != message.size())
        return false;
    if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(250))
        return false;
    QElapsedTimer timer;
    timer.start();
    while (!socket.canReadLine()) {
        const int remaining = 750 - int(timer.elapsed());
        if (remaining <= 0 || !socket.waitForReadyRead(remaining))
            return false;
    }
    return socket.readLine().trimmed() == QByteArrayLiteral("accepted");
}

SingleInstance::StartResult SingleInstance::start(const QString& name, const LaunchRequest& request)
{
    if (notifyRunningInstance(name, request))
        return SecondaryInstance;

    // A socket timeout can mean that the first instance is still constructing
    // its UI. Only the process holding this lock may remove a stale socket.
    const QByteArray hash = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256).toHex();
    auto lock = std::make_unique<QLockFile>(QDir::temp().filePath(
        QStringLiteral("speedcrunch-instance-%1.lock").arg(QString::fromLatin1(hash))));
    lock->setStaleLockTime(0);
    QElapsedTimer timer;
    timer.start();
    while (!lock->tryLock()) {
        if (lock->error() != QLockFile::LockFailedError || timer.elapsed() >= 5000) {
            qWarning() << "Could not acquire singleton lock:" << lock->error();
            return ServerUnavailable;
        }
        if (notifyRunningInstance(name, request))
            return SecondaryInstance;
        QThread::msleep(25);
    }
    m_lock = std::move(lock);
    if (m_server.listen(name))
        return PrimaryInstance;
    if (m_server.serverError() == QAbstractSocket::AddressInUseError) {
        if (notifyRunningInstance(name, request))
            return SecondaryInstance;
        QLocalServer::removeServer(name);
        if (m_server.listen(name))
            return PrimaryInstance;
    }
    qWarning() << "Could not initialize singleton server:" << m_server.errorString();
    m_lock.reset();
    return ServerUnavailable;
}

void SingleInstance::close()
{
    m_server.close();
    m_lock.reset();
}
