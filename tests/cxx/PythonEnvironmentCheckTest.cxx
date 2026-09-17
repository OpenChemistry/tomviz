/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "EditNodeWidget.h"
#include "Pipeline.h"
#include "PythonEnvironmentCheck.h"
#include "Utilities.h"
#include "transforms/LegacyPythonTransform.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "TomvizTest.h"

using tomviz::readInJSONDescription;
using tomviz::pipeline::EditNodeWidget;
using tomviz::pipeline::LegacyPythonTransform;
using tomviz::pipeline::Pipeline;
using tomviz::pipeline::PythonEnvironmentCheck;
using tomviz::pipeline::PythonEnvironmentInfo;
using Status = PythonEnvironmentInfo::Status;

namespace {

const char* kRequired = "3.1.3";

void writeFile(const QString& path, const QByteArray& content,
               bool executable)
{
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    << path.toStdString();
  file.write(content);
  file.close();
  if (executable) {
    ASSERT_TRUE(file.setPermissions(
      file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser |
      QFileDevice::ExeGroup | QFileDevice::ExeOther));
  }
}

/// A fake environment: <root>/bin/python (or python.exe on Windows),
/// without tomviz-pipeline until installCli() is called.
struct FakeEnv
{
  QTemporaryDir dir;

  QString root() const { return QDir::cleanPath(dir.path()); }
#if defined(Q_OS_WIN)
  QString binDir() const { return root() + "/Scripts"; }
  QString interpreter() const { return root() + "/python.exe"; }
  QString cli() const { return binDir() + "/tomviz-pipeline.exe"; }
#else
  QString binDir() const { return root() + "/bin"; }
  QString interpreter() const { return binDir() + "/python"; }
  QString cli() const { return binDir() + "/tomviz-pipeline"; }
#endif

  void create()
  {
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir().mkpath(binDir()));
    writeFile(interpreter(), "", true);
  }

  /// POSIX: a shell script standing in for the click entry point.
  void installCli(const QByteArray& script)
  {
    writeFile(cli(), script, true);
  }

  void installCliReporting(const QString& version)
  {
    installCli("#!/bin/sh\necho \"tomviz-pipeline, version " +
               version.toUtf8() + "\"\n");
  }
};

EditNodeWidget* makeSam2Editor(Pipeline& pipeline)
{
  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(readInJSONDescription("SAM2Segment3D"));
  transform->setScript("def transform(dataset):\n    pass\n");
  pipeline.addNode(transform);
  return transform->createPropertiesWidget(&pipeline, nullptr);
}

PythonEnvironmentInfo runAsync(const QString& path)
{
  tomviz_test::ensureQApp();
  PythonEnvironmentCheck check;
  PythonEnvironmentInfo result;
  bool done = false;
  QObject::connect(&check, &PythonEnvironmentCheck::finished, &check,
                   [&](const PythonEnvironmentInfo& info) {
                     result = info;
                     done = true;
                   });
  check.start(path, 10000, kRequired);
  EXPECT_TRUE(QTest::qWaitFor([&]() { return done; }, 15000));
  return result;
}

} // namespace

// ---- version rule -----------------------------------------------------------

TEST(PythonEnvironmentCheckTest, ParsesMajorMinorPatchPrefix)
{
  int major = -1, minor = -1, patch = -1;
  EXPECT_TRUE(PythonEnvironmentCheck::parseVersion("3.1.3", major, minor,
                                                   patch));
  EXPECT_EQ(major, 3);
  EXPECT_EQ(minor, 1);
  EXPECT_EQ(patch, 3);

  EXPECT_TRUE(PythonEnvironmentCheck::parseVersion("3.2.0b1", major, minor,
                                                   patch));
  EXPECT_EQ(minor, 2);
  EXPECT_EQ(patch, 0);

  EXPECT_TRUE(PythonEnvironmentCheck::parseVersion("v3.1", major, minor,
                                                   patch));
  EXPECT_EQ(patch, 0);

  EXPECT_TRUE(PythonEnvironmentCheck::parseVersion(" 3.1.4.dev0 ", major,
                                                   minor, patch));
  EXPECT_EQ(patch, 4);

  EXPECT_FALSE(PythonEnvironmentCheck::parseVersion("3", major, minor,
                                                    patch));
  EXPECT_FALSE(PythonEnvironmentCheck::parseVersion("beta", major, minor,
                                                    patch));
  EXPECT_FALSE(PythonEnvironmentCheck::parseVersion("", major, minor,
                                                    patch));
}

TEST(PythonEnvironmentCheckTest, AcceptsSameMajorNotOlderThanFloor)
{
  // >= 3.1.3 and < 4: newer minors keep the CLI contract, so only a
  // new major is treated as incompatible.
  EXPECT_TRUE(PythonEnvironmentCheck::isCompatibleVersion("3.1.3", "3.1.3"));
  EXPECT_TRUE(PythonEnvironmentCheck::isCompatibleVersion("3.1.9", "3.1.3"));
  EXPECT_TRUE(
    PythonEnvironmentCheck::isCompatibleVersion("3.1.10.dev0", "3.1.3"));
  EXPECT_TRUE(PythonEnvironmentCheck::isCompatibleVersion("3.2.0", "3.1.3"));
  EXPECT_TRUE(PythonEnvironmentCheck::isCompatibleVersion("3.10.1", "3.1.3"));
  EXPECT_FALSE(PythonEnvironmentCheck::isCompatibleVersion("3.1.2", "3.1.3"));
  EXPECT_FALSE(
    PythonEnvironmentCheck::isCompatibleVersion("3.0.0beta1", "3.1.3"));
  EXPECT_FALSE(PythonEnvironmentCheck::isCompatibleVersion("4.0.0", "3.1.3"));
  EXPECT_FALSE(PythonEnvironmentCheck::isCompatibleVersion("4.1.3", "3.1.3"));
  EXPECT_FALSE(PythonEnvironmentCheck::isCompatibleVersion("junk", "3.1.3"));
  // An unparsable floor disables the rule rather than rejecting all.
  EXPECT_TRUE(PythonEnvironmentCheck::isCompatibleVersion("1.0.0", "???"));
}

TEST(PythonEnvironmentCheckTest, RequirementSpecCoversNextMajor)
{
  EXPECT_EQ(PythonEnvironmentCheck::requirementSpec("3.1.3"),
            "tomviz-pipeline>=3.1.3,<4");
  EXPECT_EQ(PythonEnvironmentCheck::requirementSpec("9.9.9"),
            "tomviz-pipeline>=9.9.9,<10");
  // The build's own floor is well-formed.
  int major = 0, minor = 0, patch = 0;
  EXPECT_TRUE(PythonEnvironmentCheck::parseVersion(
    PythonEnvironmentCheck::requiredVersion(), major, minor, patch));
}

// ---- environment root resolution -------------------------------------------

TEST(PythonEnvironmentCheckTest, ResolvesRootFromRootBinDirOrInterpreter)
{
  FakeEnv env;
  env.create();

  EXPECT_EQ(PythonEnvironmentCheck::resolveEnvironmentRoot(env.root()),
            env.root());
  EXPECT_EQ(PythonEnvironmentCheck::resolveEnvironmentRoot(env.root() + "/"),
            env.root());
  EXPECT_EQ(PythonEnvironmentCheck::resolveEnvironmentRoot(env.binDir()),
            env.root());
  EXPECT_EQ(
    PythonEnvironmentCheck::resolveEnvironmentRoot(env.interpreter()),
    env.root());
  EXPECT_EQ(PythonEnvironmentCheck::resolveEnvironmentRoot("  " + env.root() +
                                                           "  "),
            env.root());
}

TEST(PythonEnvironmentCheckTest, RejectsNonEnvironments)
{
  QTemporaryDir plain;
  ASSERT_TRUE(plain.isValid());
  ASSERT_TRUE(QDir().mkpath(plain.path() + "/bin"));
  writeFile(plain.path() + "/notes.txt", "hi", false);

  EXPECT_TRUE(PythonEnvironmentCheck::resolveEnvironmentRoot("").isEmpty());
  EXPECT_TRUE(
    PythonEnvironmentCheck::resolveEnvironmentRoot(plain.path()).isEmpty());
  EXPECT_TRUE(PythonEnvironmentCheck::resolveEnvironmentRoot(plain.path() +
                                                             "/bin")
                .isEmpty());
  EXPECT_TRUE(PythonEnvironmentCheck::resolveEnvironmentRoot(plain.path() +
                                                             "/notes.txt")
                .isEmpty());
  EXPECT_TRUE(PythonEnvironmentCheck::resolveEnvironmentRoot(
                plain.path() + "/does/not/exist")
                .isEmpty());
}

// ---- filesystem verdicts ---------------------------------------------------

TEST(PythonEnvironmentCheckTest, ReportsMissingPathAndNonEnvironment)
{
  PythonEnvironmentInfo none = PythonEnvironmentCheck::check("", 1000,
                                                             kRequired);
  EXPECT_EQ(none.status, Status::NoPath);
  EXPECT_TRUE(none.message.isEmpty());

  QTemporaryDir plain;
  ASSERT_TRUE(plain.isValid());
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(plain.path(), 1000, kRequired);
  EXPECT_EQ(info.status, Status::NotAnEnvironment);
  EXPECT_FALSE(info.ok());
  EXPECT_TRUE(info.envPath.isEmpty());
  EXPECT_TRUE(info.message.contains("not a Python environment"));
}

TEST(PythonEnvironmentCheckTest, ReportsMissingCli)
{
  FakeEnv env;
  env.create();
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.binDir(), 1000, kRequired);
  EXPECT_EQ(info.status, Status::CliMissing);
  EXPECT_EQ(info.envPath, env.root());
  EXPECT_TRUE(info.cliPath.isEmpty());
  EXPECT_TRUE(info.message.contains("not installed in this environment"));
  EXPECT_FALSE(info.message.contains(env.root()));
  // Problem, blank line, then how to fix it.
  EXPECT_TRUE(info.message.contains(
    "\n\nTo fix: activate the environment, then run:\n"
    "pip install \"tomviz-pipeline>=3.1.3,<4\""));
}

#if !defined(Q_OS_WIN)

// ---- subprocess verdicts (POSIX: the fake CLI is a shell script) -----------

TEST(PythonEnvironmentCheckTest, AcceptsCompatibleVersion)
{
  FakeEnv env;
  env.create();
  env.installCliReporting("3.1.5");
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::Ok) << info.message.toStdString();
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(info.version, "3.1.5");
  EXPECT_EQ(info.cliPath, env.cli());
  EXPECT_EQ(info.message,
            "This environment is compatible: tomviz-pipeline 3.1.5");
}

TEST(PythonEnvironmentCheckTest, RejectsTooOldVersion)
{
  FakeEnv env;
  env.create();
  env.installCliReporting("3.0.0beta1");
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::VersionTooOld);
  EXPECT_EQ(info.version, "3.0.0beta1");
  EXPECT_TRUE(info.message.contains("too old"));
  EXPECT_TRUE(info.message.contains("pip install -U"));
}

TEST(PythonEnvironmentCheckTest, SuggestsCondaWhenCondaInstalledIt)
{
  FakeEnv env;
  env.create();
  env.installCliReporting("3.1.1");
  // conda's record of the package it installed.
  ASSERT_TRUE(QDir().mkpath(env.root() + "/conda-meta"));
  writeFile(env.root() + "/conda-meta/tomviz-pipeline-3.1.1-pyhd8ed1ab_0.json",
            "{}", false);

  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::VersionTooOld);
  EXPECT_TRUE(info.message.contains(
    "then run:\nconda install -c conda-forge \"tomviz-pipeline>=3.1.3,<4\""))
    << info.message.toStdString();
  EXPECT_FALSE(info.message.contains("pip install"));

  // A conda env where conda did NOT install it (pip did, or nothing
  // did) keeps the pip hint: conda has no record to disagree with.
  ASSERT_TRUE(QFile::remove(
    env.root() + "/conda-meta/tomviz-pipeline-3.1.1-pyhd8ed1ab_0.json"));
  info = PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_TRUE(info.message.contains("pip install -U"));
}

TEST(PythonEnvironmentCheckTest, AcceptsNextMinorRejectsNextMajor)
{
  FakeEnv env;
  env.create();
  // A newer minor keeps the CLI contract: no false alarm the day the
  // library ships 3.2.
  env.installCliReporting("3.2.0");
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::Ok);

  env.installCliReporting("4.0.0");
  info = PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::VersionTooNew);
  EXPECT_TRUE(info.message.contains("newer"));
}

TEST(PythonEnvironmentCheckTest, TreatsMissingVersionOptionAsTooOld)
{
  // tomviz-pipeline < 3.1 has no --version option: click exits 2 with
  // "No such option". Report that as an old install with the upgrade
  // command, not as a broken one.
  FakeEnv env;
  env.create();
  env.installCli("#!/bin/sh\necho \"Usage: tomviz-pipeline [OPTIONS]\" >&2\n"
                 "echo \"Try 'tomviz-pipeline --help' for help.\" >&2\n"
                 "echo >&2\necho \"Error: No such option '--version'.\" >&2\n"
                 "exit 2\n");
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::VersionTooOld) << info.message.toStdString();
  EXPECT_TRUE(info.version.isEmpty());
  EXPECT_EQ(info.message,
            "tomviz-pipeline in this environment predates 3.1.3 and is too "
            "old.\n\nTo fix: activate the environment, then run:\n"
            "pip install -U \"tomviz-pipeline>=3.1.3,<4\"");
}

TEST(PythonEnvironmentCheckTest, ReportsCliThatFails)
{
  FakeEnv env;
  env.create();
  env.installCli("#!/bin/sh\necho 'ModuleNotFoundError: No module named "
                 "numpy' >&2\nexit 1\n");
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::CliBroken);
  EXPECT_TRUE(info.message.contains("failed to run (exit code 1)"));
  EXPECT_TRUE(info.message.contains("ModuleNotFoundError"));
  EXPECT_TRUE(info.message.contains(
    "\n\nCheck the error above. If a dependency is missing, activate the "
    "environment, then run:\npip install \"tomviz-pipeline>=3.1.3,<4\""));
  EXPECT_FALSE(info.message.contains("force-reinstall"));
}

TEST(PythonEnvironmentCheckTest, ReportsCliThatPrintsNoVersion)
{
  FakeEnv env;
  env.create();
  env.installCli("#!/bin/sh\necho 'usage: something'\n");
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::CliBroken);
  EXPECT_TRUE(info.message.contains("no version"));
}

TEST(PythonEnvironmentCheckTest, ReportsCliWithDanglingInterpreter)
{
  // A venv that was moved: the entry point's shebang names an
  // interpreter that no longer exists.
  FakeEnv env;
  env.create();
  env.installCli("#!/nonexistent/env/bin/python\nprint('unreachable')\n");
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 10000, kRequired);
  EXPECT_EQ(info.status, Status::CliBroken);
  EXPECT_TRUE(info.message.contains("could not be started") ||
              info.message.contains("failed to run"))
    << info.message.toStdString();
  EXPECT_FALSE(info.message.contains("pip install"));
}

TEST(PythonEnvironmentCheckTest, ReportsCliThatHangs)
{
  FakeEnv env;
  env.create();
  env.installCli("#!/bin/sh\nsleep 30\n");
  PythonEnvironmentInfo info =
    PythonEnvironmentCheck::check(env.root(), 500, kRequired);
  EXPECT_EQ(info.status, Status::CliBroken);
  EXPECT_TRUE(info.message.contains("did not respond"));
  EXPECT_FALSE(info.message.contains("pip install"));
}

TEST(PythonEnvironmentCheckTest, AsyncDeliversSameVerdicts)
{
  FakeEnv env;
  env.create();

  PythonEnvironmentInfo missing = runAsync(env.root());
  EXPECT_EQ(missing.status, Status::CliMissing);

  env.installCliReporting("3.1.3");
  PythonEnvironmentInfo ok = runAsync(env.interpreter());
  EXPECT_EQ(ok.status, Status::Ok) << ok.message.toStdString();
  EXPECT_EQ(ok.envPath, env.root());
  EXPECT_EQ(ok.version, "3.1.3");
}

TEST(PythonEnvironmentCheckTest, AsyncSupersededCheckIsDropped)
{
  tomviz_test::ensureQApp();
  FakeEnv slow;
  slow.create();
  slow.installCli("#!/bin/sh\nsleep 2\necho 'tomviz-pipeline, version "
                  "3.1.3'\n");
  FakeEnv fast;
  fast.create();
  fast.installCliReporting("3.1.4");

  PythonEnvironmentCheck check;
  QList<PythonEnvironmentInfo> results;
  QObject::connect(&check, &PythonEnvironmentCheck::finished, &check,
                   [&](const PythonEnvironmentInfo& info) {
                     results.append(info);
                   });
  check.start(slow.root(), 10000, kRequired);
  EXPECT_TRUE(check.isRunning());
  check.start(fast.root(), 10000, kRequired);
  EXPECT_TRUE(QTest::qWaitFor([&]() { return !results.isEmpty(); }, 15000));
  // Give the superseded (killed) process time to have reported, had it
  // not been dropped.
  QTest::qWait(300);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results.first().envPath, fast.root());
  EXPECT_EQ(results.first().version, "3.1.4");
}

// ---- editor integration -----------------------------------------------------

TEST(PythonEnvironmentCheckTest, EditorShowsVerdictAndCanonicalizesRoot)
{
  QCoreApplication::setOrganizationName("tomviz-tests");
  QCoreApplication::setApplicationName("tomviz-tests");
  tomviz_test::ensureQApp();
  QSettings().remove("externalEnvPaths/SAM2Segment3D");

  FakeEnv env;
  env.create();
  env.installCliReporting("3.1.7");

  Pipeline pipeline;
  auto* editor = makeSam2Editor(pipeline);
  auto* envEdit = editor->findChild<QLineEdit*>("executorEnvPathEdit");
  auto* status = editor->findChild<QLabel*>("executorEnvStatusLabel");
  ASSERT_NE(envEdit, nullptr);
  ASSERT_NE(status, nullptr);
  EXPECT_TRUE(status->isHidden());

  // Picking <env>/bin: the verdict appears and the field is rewritten
  // to the environment root.
  envEdit->setText(env.binDir());
  EXPECT_TRUE(QTest::qWaitFor(
    [&]() { return status->text().contains("3.1.7"); }, 15000))
    << status->text().toStdString();
  EXPECT_FALSE(status->isHidden());
  EXPECT_EQ(envEdit->text(), env.root());

  // A directory that isn't an environment is called out.
  QTemporaryDir plain;
  ASSERT_TRUE(plain.isValid());
  envEdit->setText(plain.path());
  EXPECT_TRUE(QTest::qWaitFor(
    [&]() { return status->text().contains("not a Python environment"); },
    15000))
    << status->text().toStdString();
  EXPECT_EQ(envEdit->text(), plain.path());

  // Clearing the path clears the verdict.
  envEdit->clear();
  EXPECT_TRUE(QTest::qWaitFor([&]() { return status->isHidden(); },
                              5000));

  delete editor;
  QSettings().remove("externalEnvPaths/SAM2Segment3D");
}

TEST(PythonEnvironmentCheckTest, ApplyStoresEnvironmentRoot)
{
  QCoreApplication::setOrganizationName("tomviz-tests");
  QCoreApplication::setApplicationName("tomviz-tests");
  tomviz_test::ensureQApp();
  QSettings().remove("externalEnvPaths/SAM2Segment3D");

  FakeEnv env;
  env.create();

  Pipeline pipeline;
  auto* editor = makeSam2Editor(pipeline);
  auto* envEdit = editor->findChild<QLineEdit*>("executorEnvPathEdit");
  ASSERT_NE(envEdit, nullptr);
  // Apply before the debounced check gets to run.
  envEdit->setText(env.interpreter());
  editor->applyChangesToOperator();
  EXPECT_EQ(QSettings().value("externalEnvPaths/SAM2Segment3D").toString(),
            env.root());
  delete editor;
  QSettings().remove("externalEnvPaths/SAM2Segment3D");
}

#endif // !Q_OS_WIN
