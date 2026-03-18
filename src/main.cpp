// This file is part of the SpeedCrunch project
// Copyright (C) 2004 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2007-2009, 2014 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

#include "gui/mainwindow.h"
#include "core/settings.h"

#include <QCoreApplication>
#include <QApplication>
#include <QCryptographicHash>
#include <QAbstractSocket>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>

namespace {

QString singletonServerName()
{
    const QString scope = QCoreApplication::applicationFilePath()
        + QLatin1Char('|')
        + Settings::getConfigPath();
    const QByteArray hash = QCryptographicHash::hash(scope.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("speedcrunch-single-instance-%1").arg(QString::fromLatin1(hash.left(24)));
}

void activateMainWindow(MainWindow* window)
{
    if (!window)
        return;

    if (window->windowState() & Qt::WindowMinimized)
        window->setWindowState(window->windowState() & ~Qt::WindowMinimized);

    window->show();
    window->raise();
    window->activateWindow();
}

bool notifyRunningInstance(const QString& serverName)
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::WriteOnly);
    if (!socket.waitForConnected(250))
        return false;

    socket.write("activate");
    socket.waitForBytesWritten(250);
    socket.disconnectFromServer();
    return true;
}

bool startSingletonServer(QLocalServer* server, const QString& serverName, bool* alreadyRunning)
{
    if (alreadyRunning)
        *alreadyRunning = false;

    if (server->listen(serverName))
        return true;

    if (server->serverError() == QAbstractSocket::AddressInUseError) {
        if (notifyRunningInstance(serverName)) {
            if (alreadyRunning)
                *alreadyRunning = true;
            return false;
        }
        QLocalServer::removeServer(serverName);
        if (server->listen(serverName))
            return true;
    }

    qWarning() << "Could not initialize singleton server:" << server->errorString();
    return false;
}

}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    QCoreApplication::setApplicationName("SpeedCrunch");
    QCoreApplication::setOrganizationDomain("speedcrunch.org");

    Settings* settings = Settings::instance();
    QLocalServer singletonServer;
    MainWindow* mainWindow = 0;
    bool pendingActivation = false;

    if (settings->singleInstance) {
        const QString serverName = singletonServerName();
        if (notifyRunningInstance(serverName))
            return 0;

        bool alreadyRunning = false;
        if (startSingletonServer(&singletonServer, serverName, &alreadyRunning)) {
            QObject::connect(&singletonServer, &QLocalServer::newConnection, &application, [&]() {
                while (singletonServer.hasPendingConnections()) {
                    QLocalSocket* socket = singletonServer.nextPendingConnection();
                    if (!socket)
                        continue;
                    socket->close();
                    socket->deleteLater();
                }

                if (mainWindow)
                    activateMainWindow(mainWindow);
                else
                    pendingActivation = true;
            });
        } else if (alreadyRunning) {
            return 0;
        }
    }

    MainWindow window;
    mainWindow = &window;
    window.show();
    if (pendingActivation)
        activateMainWindow(mainWindow);

    application.connect(&application, SIGNAL(lastWindowClosed()), &application, SLOT(quit()));

    return application.exec();
}
