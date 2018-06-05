/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include <QDebug>
#include <QProcess>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include "DockerUtilities.h"

using namespace tomviz;

class DockerUtilitiesTest : public QObject
{
  Q_OBJECT

private:
  docker::DockerRunInvocation* run(
    const QString& image, const QString& entryPoint = QString(),
    const QStringList& containerArgs = QStringList(),
    const QMap<QString, QString>& bindMounts = QMap<QString, QString>())
  {
    auto runInvocation =
      docker::run(image, entryPoint, containerArgs, bindMounts);
    QSignalSpy runError(runInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy runFinished(runInvocation,
                           &docker::DockerPullInvocation::finished);
    runFinished.wait();

    return runInvocation;
  }

  void remove(const QString& containerId)
  {
    auto removeInvocation = docker::remove(containerId);
    QSignalSpy removeError(removeInvocation,
                           &docker::DockerRemoveInvocation::error);
    QSignalSpy removeFinished(removeInvocation,
                              &docker::DockerRemoveInvocation::finished);
    removeFinished.wait();
    removeInvocation->deleteLater();
  }

  void pull(const QString& image)
  {
    auto alpinePullInvocation = docker::pull(image);
    QSignalSpy alpinePullError(alpinePullInvocation,
                               &docker::DockerPullInvocation::error);
    QSignalSpy alpinePullFinished(alpinePullInvocation,
                                  &docker::DockerPullInvocation::finished);
    alpinePullFinished.wait();
  }

private slots:
  void initTestCase()
  {
    qRegisterMetaType<QProcess::ProcessError>();
    qRegisterMetaType<QProcess::ExitStatus>();
    pull("alpine");
    pull("hello-world");
  }

  void cleanupTestCase() {}

  void runTest()
  {
    auto runInvocation = docker::run("hello-world");
    QSignalSpy runError(runInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy runFinished(runInvocation,
                           &docker::DockerPullInvocation::finished);
    runFinished.wait();
    QVERIFY(runError.isEmpty());
    QCOMPARE(runFinished.size(), 1);
    auto arguments = runFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);

    auto containerId = runInvocation->containerId();
    QVERIFY(!containerId.isEmpty());
    runInvocation->deleteLater();

    auto logInvocation = docker::logs(containerId);
    QSignalSpy logError(logInvocation, &docker::DockerLogsInvocation::error);
    QSignalSpy logFinished(logInvocation,
                           &docker::DockerLogsInvocation::finished);
    logFinished.wait();
    QVERIFY(logError.isEmpty());
    QCOMPARE(logFinished.size(), 1);
    arguments = logFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    QVERIFY(logInvocation->logs().trimmed().startsWith("Hello from Docker!"));
    logInvocation->deleteLater();
    remove(containerId);
  }

  void pullTest()
  {
    auto pullInvocation = docker::run("alpine");
    QSignalSpy pullError(pullInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy pullFinished(pullInvocation,
                            &docker::DockerPullInvocation::finished);
    pullFinished.wait();
    QVERIFY(pullError.isEmpty());
    QCOMPARE(pullFinished.size(), 1);
    auto arguments = pullFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
  }

  void runBindMountTest()
  {
    // We can't bind mount volumes on CircleCi so skip the test.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains("CIRCLECI")) {
      QSKIP("Running on CircleCI skipping mount test.");
    }

    QMap<QString, QString> bindMounts;
    QTemporaryDir tempDir;

    bindMounts[tempDir.path()] = "/test";
    QString entryPoint = "/bin/sh";
    QStringList args;
    args.append("-c");
    args.append("echo 'world' > /test/hello.txt");

    auto runInvocation = docker::run("alpine", entryPoint, args, bindMounts);
    QSignalSpy runError(runInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy runFinished(runInvocation,
                           &docker::DockerPullInvocation::finished);
    runFinished.wait();
    QVERIFY(runError.isEmpty());
    QCOMPARE(runFinished.size(), 1);
    auto arguments = runFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    auto containerId = runInvocation->containerId();
    remove(containerId);
    runInvocation->deleteLater();

    QFile file(QDir(tempDir.path()).filePath("hello.txt"));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString(file.readLine()).trimmed(), QString("world"));
  }

  void dockerErrorTest()
  {
    auto runInvocation = docker::run("alpine", "/bin/bash");
    QSignalSpy runError(runInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy runFinished(runInvocation,
                           &docker::DockerPullInvocation::finished);
    runFinished.wait();
    QVERIFY(runError.isEmpty());
    QCOMPARE(runFinished.size(), 1);
    auto arguments = runFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 127);
    runInvocation->deleteLater();
  }

  void stopTest()
  {
    QString entryPoint = "/bin/sh";
    QStringList args;
    args.append("-c");
    args.append("sleep 30");

    auto runInvocation = run("alpine", entryPoint, args);
    auto containerId = runInvocation->containerId();
    QVERIFY(!containerId.isEmpty());
    runInvocation->deleteLater();

    auto stopInvocation = docker::stop(containerId, 1);
    QSignalSpy stopError(stopInvocation, &docker::DockerPullInvocation::error);
    QSignalSpy stopFinished(stopInvocation,
                            &docker::DockerPullInvocation::finished);
    stopFinished.wait();
    QVERIFY(stopError.isEmpty());
    QCOMPARE(stopFinished.size(), 1);
    auto arguments = stopFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    stopInvocation->deleteLater();

    auto inspectInvocation = docker::inspect(containerId);
    QSignalSpy inspectError(inspectInvocation,
                            &docker::DockerPullInvocation::error);
    QSignalSpy inspectFinished(inspectInvocation,
                               &docker::DockerPullInvocation::finished);
    inspectFinished.wait();
    QVERIFY(inspectError.isEmpty());
    QCOMPARE(inspectFinished.size(), 1);
    arguments = inspectFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    QCOMPARE(inspectInvocation->status(), QString("exited"));
    inspectInvocation->deleteLater();
    remove(containerId);
  }

  void inspectTest()
  {
    auto runInvocation = run("alpine");
    auto containerId = runInvocation->containerId();
    QVERIFY(!containerId.isEmpty());
    runInvocation->deleteLater();

    auto inspectInvocation = docker::inspect(containerId);
    QSignalSpy inspectError(inspectInvocation,
                            &docker::DockerPullInvocation::error);
    QSignalSpy inspectFinished(inspectInvocation,
                               &docker::DockerPullInvocation::finished);
    inspectFinished.wait();
    QVERIFY(inspectError.isEmpty());
    QCOMPARE(inspectFinished.size(), 1);
    auto arguments = inspectFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 0);
    QCOMPARE(inspectInvocation->status(), QString("exited"));
    QCOMPARE(inspectInvocation->exitCode(), 0);
    inspectInvocation->deleteLater();
    remove(containerId);
  }

  void removeTest()
  {
    auto runInvocation = run("alpine");
    auto containerId = runInvocation->containerId();
    QVERIFY(!containerId.isEmpty());
    runInvocation->deleteLater();

    auto removeInvocation = docker::remove(containerId);
    QSignalSpy removeError(removeInvocation,
                           &docker::DockerRemoveInvocation::error);
    QSignalSpy removeFinished(removeInvocation,
                              &docker::DockerRemoveInvocation::finished);
    removeFinished.wait();
    QVERIFY(removeError.isEmpty());
    QCOMPARE(removeFinished.size(), 1);
    removeInvocation->deleteLater();

    auto inspectInvocation = docker::inspect(containerId);
    QSignalSpy inspectError(inspectInvocation,
                            &docker::DockerPullInvocation::error);
    QSignalSpy inspectFinished(inspectInvocation,
                               &docker::DockerPullInvocation::finished);
    inspectFinished.wait();
    QVERIFY(inspectError.isEmpty());
    QCOMPARE(inspectFinished.size(), 1);
    auto arguments = inspectFinished.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 1);
    inspectInvocation->deleteLater();
  }
};

QTEST_GUILESS_MAIN(DockerUtilitiesTest)
#include "DockerUtilitiesTest.moc"
