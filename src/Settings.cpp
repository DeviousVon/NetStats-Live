// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DeviousVon

#include "Settings.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace nsl {
namespace {

QString configBaseDir() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
}

QString configDir() {
    return QDir(configBaseDir()).filePath(QStringLiteral("netstats-live"));
}

QString legacyConfigPath() {
    return QDir(QDir(configBaseDir()).filePath(QStringLiteral("nsl-linux"))).filePath(QStringLiteral("nsl-linux.conf"));
}

QString autostartDir() {
    return QDir(configBaseDir()).filePath(QStringLiteral("autostart"));
}

QString newAutoStartPath() {
    return QDir(autostartDir()).filePath(QStringLiteral("netstats-live.desktop"));
}

void appendError(StorageResult& result, const QString& error) {
    if (error.isEmpty()) {
        return;
    }
    result.ok = false;
    if (!result.error.isEmpty()) {
        result.error += QLatin1Char('\n');
    }
    result.error += error;
}

void mergeResult(StorageResult& result, const StorageResult& other) {
    if (!other.ok) {
        appendError(result, other.error);
    }
}

QString systemError(int errorNumber) {
    return QString::fromLocal8Bit(std::strerror(errorNumber));
}

QFile::Permissions permissionsFromMode(mode_t mode);
mode_t modeFromPermissions(QFile::Permissions permissions);
QDateTime dateTimeFromTimespec(const timespec& value);
qint64 nanosecondsFromTimespec(const timespec& value);

struct FileIdentity {
    quint64 device = 0;
    quint64 inode = 0;
    qint64 fileSize = -1;
    QByteArray contentHash;
    mode_t mode = 0;
};

FileIdentity identityFromPublished(const struct stat& status,
                                   const QByteArray& contents,
                                   QFile::Permissions permissions) {
    return {static_cast<quint64>(status.st_dev),
            static_cast<quint64>(status.st_ino),
            static_cast<qint64>(status.st_size),
            QCryptographicHash::hash(contents, QCryptographicHash::Sha256),
            modeFromPermissions(permissions)};
}

FileIdentity identityFromSnapshot(const AutoStartSnapshot& snapshot) {
    return {snapshot.device,
            snapshot.inode,
            snapshot.fileSize,
            QCryptographicHash::hash(snapshot.contents, QCryptographicHash::Sha256),
            modeFromPermissions(snapshot.permissions)};
}

FileIdentity publishedIdentityFromSnapshot(const AutoStartSnapshot& snapshot) {
    return {snapshot.publishedDevice,
            snapshot.publishedInode,
            snapshot.publishedFileSize,
            snapshot.publishedContentHash,
            modeFromPermissions(snapshot.publishedPermissions)};
}

bool identitiesMatch(const FileIdentity& left, const FileIdentity& right) {
    return left.device == right.device && left.inode == right.inode &&
           left.fileSize == right.fileSize && left.contentHash == right.contentHash && left.mode == right.mode;
}

StorageResult pinAutoStartDirectory(AutoStartSnapshot& authority) {
    const QString directoryPath = autostartDir();
    if (!QDir().mkpath(directoryPath)) {
        return {false, QStringLiteral("Unable to create autostart directory %1").arg(directoryPath)};
    }
    const QByteArray encodedPath = QFile::encodeName(directoryPath);
    const int descriptor = ::open(encodedPath.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor == -1) {
        return {false, QStringLiteral("Unable to pin autostart directory %1: %2").arg(directoryPath, systemError(errno))};
    }
    struct stat status{};
    if (::fstat(descriptor, &status) != 0) {
        const int errorNumber = errno;
        ::close(descriptor);
        return {false, QStringLiteral("Unable to inspect autostart directory %1: %2")
                           .arg(directoryPath, systemError(errorNumber))};
    }
    authority.directoryDescriptor = QSharedPointer<int>(new int(descriptor), [](int* value) {
        ::close(*value);
        delete value;
    });
    authority.directoryPath = directoryPath;
    authority.directoryDevice = static_cast<quint64>(status.st_dev);
    authority.directoryInode = static_cast<quint64>(status.st_ino);
    return {};
}

void copyDirectoryAuthority(const AutoStartSnapshot& authority, AutoStartSnapshot& snapshot) {
    snapshot.directoryDescriptor = authority.directoryDescriptor;
    snapshot.directoryPath = authority.directoryPath;
    snapshot.directoryDevice = authority.directoryDevice;
    snapshot.directoryInode = authority.directoryInode;
}

QString pinnedAutoStartPath(const AutoStartSnapshot& authority, const QString& fileName) {
    if (authority.directoryDescriptor.isNull()) {
        return {};
    }
    return QStringLiteral("/proc/self/fd/%1/%2").arg(*authority.directoryDescriptor).arg(fileName);
}

StorageResult validateAutoStartDirectory(const AutoStartSnapshot& authority, const QString& operation) {
    if (authority.directoryDescriptor.isNull() || authority.directoryPath.isEmpty()) {
        return {false, QStringLiteral("Unable to %1 without pinned autostart directory authority").arg(operation)};
    }
    const QByteArray encodedPath = QFile::encodeName(authority.directoryPath);
    const int descriptor = ::open(encodedPath.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor == -1) {
        return {false, QStringLiteral("Unable to %1 because autostart directory authority is unavailable: %2")
                           .arg(operation, systemError(errno))};
    }
    struct stat status{};
    const int statResult = ::fstat(descriptor, &status);
    const int errorNumber = errno;
    ::close(descriptor);
    if (statResult != 0) {
        return {false, QStringLiteral("Unable to %1 because autostart directory authority cannot be inspected: %2")
                           .arg(operation, systemError(errorNumber))};
    }
    if (static_cast<quint64>(status.st_dev) != authority.directoryDevice ||
        static_cast<quint64>(status.st_ino) != authority.directoryInode) {
        return {false, QStringLiteral("Refusing to %1 because the autostart directory changed").arg(operation)};
    }
    return {};
}

mode_t modeFromPermissions(QFile::Permissions permissions) {
    mode_t mode = 0;
    if (permissions.testFlag(QFileDevice::ReadOwner)) mode |= S_IRUSR;
    if (permissions.testFlag(QFileDevice::WriteOwner)) mode |= S_IWUSR;
    if (permissions.testFlag(QFileDevice::ExeOwner)) mode |= S_IXUSR;
    if (permissions.testFlag(QFileDevice::ReadGroup)) mode |= S_IRGRP;
    if (permissions.testFlag(QFileDevice::WriteGroup)) mode |= S_IWGRP;
    if (permissions.testFlag(QFileDevice::ExeGroup)) mode |= S_IXGRP;
    if (permissions.testFlag(QFileDevice::ReadOther)) mode |= S_IROTH;
    if (permissions.testFlag(QFileDevice::WriteOther)) mode |= S_IWOTH;
    if (permissions.testFlag(QFileDevice::ExeOther)) mode |= S_IXOTH;
    return mode;
}

qint64 nanosecondsFromTimespec(const timespec& value) {
    return static_cast<qint64>(value.tv_sec) * 1000000000 + value.tv_nsec;
}

timespec timespecFromNanoseconds(qint64 nanoseconds) {
    if (nanoseconds < 0) {
        return {0, UTIME_OMIT};
    }
    return {static_cast<time_t>(nanoseconds / 1000000000),
            static_cast<long>(nanoseconds % 1000000000)};
}

StorageResult snapshotRegularFile(const QString& path, AutoStartSnapshot& snapshot) {
    snapshot = {};
    const QByteArray encodedPath = QFile::encodeName(path);
    bool restoreTimesAfterRead = false;
    int descriptor = ::open(encodedPath.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NOATIME);
    if (descriptor == -1 && errno == EPERM) {
        descriptor = ::open(encodedPath.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        restoreTimesAfterRead = descriptor != -1;
    }
    if (descriptor == -1) {
        if (errno == ENOENT) {
            return {};
        }
        return {false, QStringLiteral("Unable to snapshot regular file %1: %2").arg(path, systemError(errno))};
    }
    struct stat status{};
    if (::fstat(descriptor, &status) != 0) {
        const int errorNumber = errno;
        ::close(descriptor);
        return {false, QStringLiteral("Unable to inspect regular file %1: %2").arg(path, systemError(errorNumber))};
    }
    if (!S_ISREG(status.st_mode)) {
        ::close(descriptor);
        return {false, QStringLiteral("Unable to snapshot regular file %1: expected a regular file").arg(path)};
    }
    QFile file;
    if (!file.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(descriptor);
        return {false, QStringLiteral("Unable to snapshot regular file %1: %2").arg(path, file.errorString())};
    }
    snapshot.existed = true;
    snapshot.permissions = permissionsFromMode(status.st_mode);
    snapshot.accessTime = dateTimeFromTimespec(status.st_atim);
    snapshot.modificationTime = dateTimeFromTimespec(status.st_mtim);
    snapshot.device = static_cast<quint64>(status.st_dev);
    snapshot.inode = static_cast<quint64>(status.st_ino);
    snapshot.fileSize = static_cast<qint64>(status.st_size);
    snapshot.accessTimeNanoseconds = nanosecondsFromTimespec(status.st_atim);
    snapshot.modificationTimeNanoseconds = nanosecondsFromTimespec(status.st_mtim);
    snapshot.contents = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return {false, QStringLiteral("Unable to read regular file %1: %2").arg(path, file.errorString())};
    }
    if (restoreTimesAfterRead) {
        const timespec times[2] = {status.st_atim, status.st_mtim};
        if (::futimens(descriptor, times) != 0) {
            return {false, QStringLiteral("Unable to restore timestamps after snapshotting %1: %2")
                               .arg(path, systemError(errno))};
        }
    }
    return {};
}

int renameNoReplace(const QString& source, const QString& destination) {
#ifdef SYS_renameat2
    const QByteArray encodedSource = QFile::encodeName(source);
    const QByteArray encodedDestination = QFile::encodeName(destination);
    return static_cast<int>(::syscall(SYS_renameat2,
                                      AT_FDCWD,
                                      encodedSource.constData(),
                                      AT_FDCWD,
                                      encodedDestination.constData(),
                                      RENAME_NOREPLACE));
#else
    Q_UNUSED(source);
    Q_UNUSED(destination);
    errno = ENOSYS;
    return -1;
#endif
}

QString privateSiblingPath(const QString& path, const QString& purpose) {
    return QStringLiteral("%1.%2-%3")
        .arg(path, purpose, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

StorageResult stageRemoval(const QString& path,
                           const FileIdentity& expectedIdentity,
                           QString& backupPath) {
    backupPath = privateSiblingPath(path, QStringLiteral("nsl-rollback"));
    if (renameNoReplace(path, backupPath) != 0) {
        const int errorNumber = errno;
        backupPath.clear();
        return {false, QStringLiteral("Unable to stage %1 for removal: %2").arg(path, systemError(errorNumber))};
    }

    AutoStartSnapshot moved;
    const StorageResult inspection = snapshotRegularFile(backupPath, moved);
    if (inspection.ok && moved.existed && identitiesMatch(identityFromSnapshot(moved), expectedIdentity)) {
        return {};
    }

    StorageResult result{false,
                         inspection.ok
                             ? QStringLiteral("Refusing to remove %1 because its identity changed").arg(path)
                             : inspection.error};
    if (renameNoReplace(backupPath, path) != 0) {
        appendError(result,
                    QStringLiteral("The substituted file was preserved at %1 because it could not be restored to %2: %3")
                        .arg(backupPath, path, systemError(errno)));
    } else {
        backupPath.clear();
    }
    return result;
}

StorageResult validateExpectedPath(const QString& path,
                                   const FileIdentity& expectedIdentity,
                                   const QString& operation) {
    AutoStartSnapshot current;
    const StorageResult inspected = snapshotRegularFile(path, current);
    if (!inspected.ok) {
        return inspected;
    }
    if (!current.existed || !identitiesMatch(identityFromSnapshot(current), expectedIdentity)) {
        return {false, QStringLiteral("Refusing to %1 because %2 no longer has the expected authority")
                           .arg(operation, path)};
    }
    return {};
}

StorageResult validateAbsentPath(const QString& path, const QString& operation) {
    AutoStartSnapshot current;
    const StorageResult inspected = snapshotRegularFile(path, current);
    if (!inspected.ok) {
        return inspected;
    }
    return current.existed
        ? StorageResult{false, QStringLiteral("Refusing to %1 because a concurrent file exists at %2").arg(operation, path)}
        : StorageResult{};
}

StorageResult restoreStagedPath(const QString& backupPath,
                                const QString& destination,
                                const FileIdentity& expectedIdentity) {
    const StorageResult validated = validateExpectedPath(backupPath,
                                                         expectedIdentity,
                                                         QStringLiteral("restore staged file"));
    if (!validated.ok) {
        return validated;
    }
    if (renameNoReplace(backupPath, destination) != 0) {
        return {false,
                QStringLiteral("Unable to restore %1 without overwriting %2: %3")
                    .arg(backupPath, destination, systemError(errno))};
    }
    return {};
}

StorageResult deleteExpectedFile(const QString& path, const FileIdentity& expectedIdentity) {
    QString quarantinePath;
    const StorageResult staged = stageRemoval(path, expectedIdentity, quarantinePath);
    if (!staged.ok) {
        return staged;
    }
    const QByteArray encodedQuarantine = QFile::encodeName(quarantinePath);
    if (::unlink(encodedQuarantine.constData()) != 0) {
        return {false, QStringLiteral("Unable to delete staged file %1: %2").arg(quarantinePath, systemError(errno))};
    }
    return {};
}

StorageResult publishRegularFileNoReplace(const QString& path,
                                          const QByteArray& contents,
                                          QFile::Permissions permissions,
                                          qint64 accessTimeNanoseconds,
                                          qint64 modificationTimeNanoseconds,
                                          FileIdentity& publishedIdentity) {
    const QString directoryPath = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directoryPath)) {
        return {false, QStringLiteral("Unable to create directory for %1").arg(path)};
    }

    const QByteArray encodedDirectory = QFile::encodeName(directoryPath);
    const int descriptor = ::open(encodedDirectory.constData(), O_TMPFILE | O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (descriptor == -1) {
        return {false, QStringLiteral("Unable to create unnamed temporary file for %1: %2").arg(path, systemError(errno))};
    }
    QFile temporary;
    if (!temporary.open(descriptor, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle)) {
        ::close(descriptor);
        return {false, QStringLiteral("Unable to adopt unnamed temporary file for %1: %2").arg(path, temporary.errorString())};
    }
    if (temporary.write(contents) != contents.size() || !temporary.flush()) {
        return {false, QStringLiteral("Unable to write all bytes for %1: %2").arg(path, temporary.errorString())};
    }
    if (::fchmod(descriptor, modeFromPermissions(permissions)) != 0) {
        return {false, QStringLiteral("Unable to set permissions before publishing %1: %2").arg(path, systemError(errno))};
    }
    if (accessTimeNanoseconds >= 0 || modificationTimeNanoseconds >= 0) {
        const timespec times[2] = {timespecFromNanoseconds(accessTimeNanoseconds),
                                  timespecFromNanoseconds(modificationTimeNanoseconds)};
        if (::futimens(descriptor, times) != 0) {
            return {false, QStringLiteral("Unable to set timestamps before publishing %1: %2").arg(path, systemError(errno))};
        }
    }
    if (::fsync(descriptor) != 0) {
        return {false, QStringLiteral("Unable to sync temporary file for %1: %2").arg(path, systemError(errno))};
    }
    struct stat publishedStatus{};
    if (::fstat(descriptor, &publishedStatus) != 0) {
        return {false, QStringLiteral("Unable to inspect publication identity for %1: %2").arg(path, systemError(errno))};
    }
    publishedIdentity = identityFromPublished(publishedStatus, contents, permissions);
    const QByteArray encodedSource = QByteArrayLiteral("/proc/self/fd/") + QByteArray::number(descriptor);
    const QByteArray encodedPath = QFile::encodeName(path);
    // AT_EMPTY_PATH requires privilege on some filesystems. Linking the same
    // O_TMPFILE through procfs is the documented unprivileged equivalent.
    if (::linkat(AT_FDCWD,
                 encodedSource.constData(),
                 AT_FDCWD,
                 encodedPath.constData(),
                 AT_SYMLINK_FOLLOW) != 0) {
        return {false, QStringLiteral("Unable to publish %1 without replacement: %2").arg(path, systemError(errno))};
    }
    return {};
}

StorageResult migrateLegacyConfigIfNeeded(const QString& settingsPath) {
    if (QFile::exists(settingsPath)) {
        return {};
    }
    const QString legacyPath = legacyConfigPath();
    if (QFile::exists(legacyPath)) {
        QFile legacyFile(legacyPath);
        if (!legacyFile.copy(settingsPath)) {
            const QString error = QStringLiteral("Unable to migrate legacy config %1 to %2: %3")
                                      .arg(legacyPath, settingsPath, legacyFile.errorString());
            qWarning().noquote() << error;
            return {false, error};
        }
    }
    return {};
}

StorageResult migrateLegacyAutoStartIfNeeded() {
    const auto reportFailure = [](StorageResult result) {
        if (!result.ok) {
            qWarning().noquote() << result.error;
        }
        return result;
    };

    AutoStartSnapshot directoryAuthority;
    StorageResult result = pinAutoStartDirectory(directoryAuthority);
    if (!result.ok) {
        return reportFailure(result);
    }
    const QString newPath = pinnedAutoStartPath(directoryAuthority, QStringLiteral("netstats-live.desktop"));
    const QString legacyPath = pinnedAutoStartPath(directoryAuthority, QStringLiteral("nsl-linux.desktop"));

    AutoStartSnapshot newEntry;
    result = snapshotRegularFile(newPath, newEntry);
    if (!result.ok) {
        return reportFailure(result);
    }
    AutoStartSnapshot legacyEntry;
    result = snapshotRegularFile(legacyPath, legacyEntry);
    if (!result.ok) {
        return reportFailure(result);
    }
    if (!legacyEntry.existed) {
        return reportFailure(validateAutoStartDirectory(directoryAuthority,
                                                        QStringLiteral("finish Auto Start migration")));
    }

    if (newEntry.existed) {
        QString stagedLegacy;
        result = stageRemoval(legacyPath, identityFromSnapshot(legacyEntry), stagedLegacy);
        if (!result.ok) {
            return reportFailure(result);
        }
        result = validateExpectedPath(newPath,
                                      identityFromSnapshot(newEntry),
                                      QStringLiteral("retire legacy Auto Start entry"));
        if (!result.ok) {
            const StorageResult rollback = restoreStagedPath(stagedLegacy,
                                                             legacyPath,
                                                             identityFromSnapshot(legacyEntry));
            mergeResult(result, rollback);
            return reportFailure(result);
        }
        return reportFailure(validateAutoStartDirectory(directoryAuthority,
                                                        QStringLiteral("finish Auto Start migration")));
    }

    QString migrated = QString::fromUtf8(legacyEntry.contents);
    migrated.replace(QStringLiteral("AnalogX NetStat Live style network monitor for Linux"), QStringLiteral("Live network throughput and CPU monitor widget for Linux"));
    migrated.replace(QStringLiteral("NSL-Linux"), QStringLiteral("NetStats-Live"));
    migrated.replace(QStringLiteral("nsl-linux"), QStringLiteral("netstats-live"));

    FileIdentity publishedIdentity;
    const QFile::Permissions permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                           QFileDevice::ReadGroup | QFileDevice::ReadOther;
    result = publishRegularFileNoReplace(newPath,
                                         migrated.toUtf8(),
                                         permissions,
                                         -1,
                                         -1,
                                         publishedIdentity);
    if (!result.ok) {
        return reportFailure(result);
    }

    QString stagedLegacy;
    result = stageRemoval(legacyPath, identityFromSnapshot(legacyEntry), stagedLegacy);
    if (!result.ok) {
        StorageResult rollback = deleteExpectedFile(newPath, publishedIdentity);
        mergeResult(result, rollback);
        return reportFailure(result);
    }
    result = validateExpectedPath(newPath,
                                  publishedIdentity,
                                  QStringLiteral("retire legacy Auto Start entry"));
    if (!result.ok) {
        const StorageResult rollback = restoreStagedPath(stagedLegacy,
                                                         legacyPath,
                                                         identityFromSnapshot(legacyEntry));
        mergeResult(result, rollback);
        return reportFailure(result);
    }
    return reportFailure(validateAutoStartDirectory(directoryAuthority,
                                                    QStringLiteral("finish Auto Start migration")));
}

QString boolKey(PaneId id) {
    return QStringLiteral("panes/%1").arg(paneConfigKey(id));
}

void archiveMonthlyTotals(QSettings& settings, const QString& monthKey, std::uint64_t rxBytes, std::uint64_t txBytes) {
    if (monthKey.isEmpty()) {
        return;
    }
    settings.setValue(QStringLiteral("history/%1/rxMonth").arg(monthKey), QVariant::fromValue<qulonglong>(rxBytes));
    settings.setValue(QStringLiteral("history/%1/txMonth").arg(monthKey), QVariant::fromValue<qulonglong>(txBytes));
}

// "Last Month" is derived from archived history so a calendar rollover can
// reset active totals without losing the just-finished month in the UI.
QString previousMonthKey(const QString& monthKey) {
    const QDate firstOfMonth = QDate::fromString(monthKey + QStringLiteral("-01"), QStringLiteral("yyyy-MM-dd"));
    return firstOfMonth.isValid() ? firstOfMonth.addMonths(-1).toString(QStringLiteral("yyyy-MM")) : QString();
}

std::uint64_t archivedMonthTotal(QSettings& settings, const QString& monthKey, const QString& direction) {
    if (monthKey.isEmpty()) {
        return 0;
    }
    return settings.value(QStringLiteral("history/%1/%2Month").arg(monthKey, direction), 0).toULongLong();
}

QString resolvedExecutablePath(const QString& executablePath) {
    const QFileInfo info(executablePath);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    const QString found = QStandardPaths::findExecutable(executablePath);
    return found.isEmpty() ? executablePath : found;
}

QString quoteExecArgument(QString argument) {
    argument.replace(QLatin1Char('%'), QStringLiteral("%%"));
    argument.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    argument.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(argument);
}

StorageResult settingsStatusResult(const QSettings& settings, const QString& operation, const QString& path) {
    switch (settings.status()) {
    case QSettings::NoError:
        return {};
    case QSettings::AccessError:
        return {false, QStringLiteral("Unable to %1 in %2: access error").arg(operation, path)};
    case QSettings::FormatError:
        return {false, QStringLiteral("Unable to %1 in %2: format error").arg(operation, path)};
    }
    return {false, QStringLiteral("Unable to %1 in %2: unknown settings error").arg(operation, path)};
}

StorageResult settingsResult(QSettings& settings, const QString& operation, const QString& path) {
    settings.sync();
    return settingsStatusResult(settings, operation, path);
}

QFile::Permissions permissionsFromMode(mode_t mode) {
    QFile::Permissions permissions;
    if ((mode & S_IRUSR) != 0) permissions |= QFileDevice::ReadOwner;
    if ((mode & S_IWUSR) != 0) permissions |= QFileDevice::WriteOwner;
    if ((mode & S_IXUSR) != 0) permissions |= QFileDevice::ExeOwner;
    if ((mode & S_IRGRP) != 0) permissions |= QFileDevice::ReadGroup;
    if ((mode & S_IWGRP) != 0) permissions |= QFileDevice::WriteGroup;
    if ((mode & S_IXGRP) != 0) permissions |= QFileDevice::ExeGroup;
    if ((mode & S_IROTH) != 0) permissions |= QFileDevice::ReadOther;
    if ((mode & S_IWOTH) != 0) permissions |= QFileDevice::WriteOther;
    if ((mode & S_IXOTH) != 0) permissions |= QFileDevice::ExeOther;
    return permissions;
}

QDateTime dateTimeFromTimespec(const timespec& value) {
    const qint64 milliseconds = static_cast<qint64>(value.tv_sec) * 1000 + value.tv_nsec / 1000000;
    return QDateTime::fromMSecsSinceEpoch(milliseconds).toUTC();
}

} // namespace

QString paneConfigKey(PaneId id) {
    switch (id) {
    case PaneId::LocalMachine: return QStringLiteral("localMachine");
    case PaneId::RemoteMachine: return QStringLiteral("remoteMachine");
    case PaneId::IncomingTotals: return QStringLiteral("incomingTotals");
    case PaneId::Incoming: return QStringLiteral("incoming");
    case PaneId::OutgoingTotals: return QStringLiteral("outgoingTotals");
    case PaneId::Outgoing: return QStringLiteral("outgoing");
    case PaneId::Threads: return QStringLiteral("threads");
    case PaneId::Cpu: return QStringLiteral("cpu");
    case PaneId::Count: break;
    }
    return QStringLiteral("unknown");
}

QString paneDisplayName(PaneId id) {
    switch (id) {
    case PaneId::LocalMachine: return QStringLiteral("Local Machine");
    case PaneId::RemoteMachine: return QStringLiteral("Remote Machine");
    case PaneId::IncomingTotals: return QStringLiteral("Incoming Totals");
    case PaneId::Incoming: return QStringLiteral("Incoming");
    case PaneId::OutgoingTotals: return QStringLiteral("Outgoing Totals");
    case PaneId::Outgoing: return QStringLiteral("Outgoing");
    case PaneId::Threads: return QStringLiteral("Threads");
    case PaneId::Cpu: return QStringLiteral("CPU");
    case PaneId::Count: break;
    }
    return QStringLiteral("Unknown");
}

AppConfig preserveMonthOnFailedRollover(AppConfig transitioned,
                                        const StorageResult& persistence,
                                        const QString& storedMonth,
                                        std::uint64_t storedRxMonth,
                                        std::uint64_t storedTxMonth) {
    if (!persistence.ok) {
        transitioned.monthKey = storedMonth;
        transitioned.rxMonth = storedRxMonth;
        transitioned.txMonth = storedTxMonth;
    }
    return transitioned;
}

AppSettings::AppSettings(bool initializeStorage)
    : settingsPath_(QDir(configDir()).filePath(QStringLiteral("netstats-live.conf"))) {
    if (!initializeStorage) {
        return;
    }
    if (!QDir().mkpath(configDir())) {
        const QString error = QStringLiteral("Unable to create config directory %1").arg(configDir());
        qWarning().noquote() << error;
        appendError(initializationResult_, error);
        appendError(storageAuthorityResult_, error);
    }
    const StorageResult configMigration = migrateLegacyConfigIfNeeded(settingsPath_);
    mergeResult(initializationResult_, configMigration);
    mergeResult(storageAuthorityResult_, configMigration);
    mergeResult(initializationResult_, migrateLegacyAutoStartIfNeeded());
}

QString AppSettings::currentMonthKey() {
    const QString fakeDate = QString::fromLocal8Bit(qgetenv("NSL_FAKE_DATE")).trimmed();
    if (!fakeDate.isEmpty()) {
        if (fakeDate.size() == 7) {
            const QDate month = QDate::fromString(fakeDate + QStringLiteral("-01"), QStringLiteral("yyyy-MM-dd"));
            if (month.isValid()) {
                return month.toString(QStringLiteral("yyyy-MM"));
            }
        }
        const QDate date = QDate::fromString(fakeDate, QStringLiteral("yyyy-MM-dd"));
        if (date.isValid()) {
            return date.toString(QStringLiteral("yyyy-MM"));
        }
    }
    return QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
}

AppConfigLoadResult AppSettings::load() const {
    AppConfigLoadResult result;
    AppConfig& config = result.config;
    config.panes.fill(true);

    QSettings settings(settingsPath_, QSettings::IniFormat);
    for (int i = 0; i < PaneCount; ++i) {
        const auto id = static_cast<PaneId>(i);
        config.panes[static_cast<std::size_t>(i)] = settings.value(boolKey(id), true).toBool();
    }
    config.autoMinimize = settings.value(QStringLiteral("config/autoMinimize"), false).toBool();
    config.autoStart = settings.value(QStringLiteral("config/autoStart"), false).toBool();
    config.urlClipCap = settings.value(QStringLiteral("config/urlClipCap"), false).toBool();
    config.alwaysOnTop = settings.value(QStringLiteral("config/alwaysOnTop"), false).toBool();
    config.unitMode = settings.value(QStringLiteral("config/unitMode"), QStringLiteral("bytes")).toString() == QStringLiteral("bits")
        ? UnitMode::Bits
        : UnitMode::Bytes;
    config.selectedInterface = settings.value(QStringLiteral("config/interface"), QStringLiteral("ALL")).toString();
    if (config.selectedInterface.isEmpty()) {
        config.selectedInterface = QStringLiteral("ALL");
    }
    config.remoteTarget = settings.value(QStringLiteral("remote/target")).toString();
    config.windowPos = settings.value(QStringLiteral("window/pos"), QPoint()).toPoint();

    const QString currentMonth = currentMonthKey();
    const QString previousMonth = previousMonthKey(currentMonth);
    const QString storedMonth = settings.value(QStringLiteral("totals/monthKey"), currentMonth).toString();
    const std::uint64_t storedRxMonth = settings.value(QStringLiteral("totals/rxMonth"), 0).toULongLong();
    const std::uint64_t storedTxMonth = settings.value(QStringLiteral("totals/txMonth"), 0).toULongLong();
    config.monthKey = currentMonth;
    config.lastRxMonth = archivedMonthTotal(settings, previousMonth, QStringLiteral("rx"));
    config.lastTxMonth = archivedMonthTotal(settings, previousMonth, QStringLiteral("tx"));
    result.storage = settingsStatusResult(settings, QStringLiteral("load configuration"), settingsPath_);
    if (!result.storage.ok) {
        return result;
    }
    if (storedMonth == currentMonth) {
        config.rxMonth = storedRxMonth;
        config.txMonth = storedTxMonth;
    } else {
        archiveMonthlyTotals(settings, storedMonth, storedRxMonth, storedTxMonth);
        if (storedMonth == previousMonth) {
            config.lastRxMonth = storedRxMonth;
            config.lastTxMonth = storedTxMonth;
        }
        settings.setValue(QStringLiteral("totals/monthKey"), currentMonth);
        settings.setValue(QStringLiteral("totals/rxMonth"), QVariant::fromValue<qulonglong>(0));
        settings.setValue(QStringLiteral("totals/txMonth"), QVariant::fromValue<qulonglong>(0));
        result.storage = settingsResult(settings, QStringLiteral("roll over monthly totals"), settingsPath_);
        config = preserveMonthOnFailedRollover(config,
                                               result.storage,
                                               storedMonth,
                                               storedRxMonth,
                                               storedTxMonth);
    }
    return result;
}

const StorageResult& AppSettings::initializationResult() const {
    return initializationResult_;
}

const StorageResult& AppSettings::storageAuthorityResult() const {
    return storageAuthorityResult_;
}

StorageResult AppSettings::saveConfig(const AppConfig& config) const {
    QSettings settings(settingsPath_, QSettings::IniFormat);
    for (int i = 0; i < PaneCount; ++i) {
        const auto id = static_cast<PaneId>(i);
        settings.setValue(boolKey(id), config.panes[static_cast<std::size_t>(i)]);
    }
    settings.setValue(QStringLiteral("config/autoMinimize"), config.autoMinimize);
    settings.setValue(QStringLiteral("config/autoStart"), config.autoStart);
    settings.setValue(QStringLiteral("config/urlClipCap"), config.urlClipCap);
    settings.setValue(QStringLiteral("config/alwaysOnTop"), config.alwaysOnTop);
    settings.setValue(QStringLiteral("config/unitMode"), config.unitMode == UnitMode::Bits ? QStringLiteral("bits") : QStringLiteral("bytes"));
    settings.setValue(QStringLiteral("config/interface"), config.selectedInterface);
    settings.setValue(QStringLiteral("remote/target"), config.remoteTarget);
    settings.setValue(QStringLiteral("window/pos"), config.windowPos);
    settings.setValue(QStringLiteral("totals/monthKey"), config.monthKey);
    settings.setValue(QStringLiteral("totals/rxMonth"), QVariant::fromValue<qulonglong>(config.rxMonth));
    settings.setValue(QStringLiteral("totals/txMonth"), QVariant::fromValue<qulonglong>(config.txMonth));
    return settingsResult(settings, QStringLiteral("save configuration"), settingsPath_);
}

StorageResult AppSettings::saveMonthlyTotals(const QString& monthKey, std::uint64_t rxBytes, std::uint64_t txBytes) const {
    QSettings settings(settingsPath_, QSettings::IniFormat);
    archiveMonthlyTotals(settings, monthKey, rxBytes, txBytes);
    settings.setValue(QStringLiteral("totals/monthKey"), monthKey);
    settings.setValue(QStringLiteral("totals/rxMonth"), QVariant::fromValue<qulonglong>(rxBytes));
    settings.setValue(QStringLiteral("totals/txMonth"), QVariant::fromValue<qulonglong>(txBytes));
    return settingsResult(settings, QStringLiteral("save monthly totals"), settingsPath_);
}

StorageResult AppSettings::saveWindowPosition(const QPoint& pos) const {
    QSettings settings(settingsPath_, QSettings::IniFormat);
    settings.setValue(QStringLiteral("window/pos"), pos);
    return settingsResult(settings, QStringLiteral("save window position"), settingsPath_);
}

QString AppSettings::configPath() const {
    return settingsPath_;
}

QString AppSettings::autoStartPath() const {
    return newAutoStartPath();
}

StorageResult AppSettings::snapshotAutoStart(AutoStartSnapshot& snapshot) const {
    AutoStartSnapshot authority;
    StorageResult result = pinAutoStartDirectory(authority);
    if (!result.ok) {
        return result;
    }
    AutoStartSnapshot fileSnapshot;
    result = snapshotRegularFile(pinnedAutoStartPath(authority, QStringLiteral("netstats-live.desktop")),
                                 fileSnapshot);
    if (!result.ok) {
        return result;
    }
    copyDirectoryAuthority(authority, fileSnapshot);
    snapshot = fileSnapshot;
    return validateAutoStartDirectory(snapshot, QStringLiteral("snapshot Auto Start entry"));
}

StorageResult AppSettings::restoreAutoStart(AutoStartSnapshot& snapshot) const {
    const QString path = pinnedAutoStartPath(snapshot, QStringLiteral("netstats-live.desktop"));
    if (path.isEmpty()) {
        return {false, QStringLiteral("Unable to restore Auto Start without pinned directory authority")};
    }
    if (snapshot.mutation == AutoStartSnapshot::Mutation::DisabledAbsent) {
        const StorageResult absent = validateAbsentPath(path, QStringLiteral("rollback Auto Start disable"));
        if (absent.ok) {
            snapshot.mutation = AutoStartSnapshot::Mutation::None;
        }
        return absent;
    }
    if (snapshot.mutation == AutoStartSnapshot::Mutation::Replaced) {
        AutoStartSnapshot current;
        const StorageResult inspected = snapshotRegularFile(path, current);
        if (!inspected.ok) {
            return inspected;
        }
        if (current.existed) {
            if (!identitiesMatch(identityFromSnapshot(current), publishedIdentityFromSnapshot(snapshot))) {
                return {false, QStringLiteral("Refusing to remove a substituted destination at %1 during rollback").arg(path)};
            }
            const StorageResult removed = deleteExpectedFile(path, publishedIdentityFromSnapshot(snapshot));
            if (!removed.ok) {
                return removed;
            }
        }
        snapshot.mutation = AutoStartSnapshot::Mutation::StagedRemoval;
    }
    if (snapshot.mutation == AutoStartSnapshot::Mutation::StagedRemoval) {
        const StorageResult restored = restoreStagedPath(snapshot.backupPath,
                                                         path,
                                                         identityFromSnapshot(snapshot));
        if (!restored.ok) {
            return {false, QStringLiteral("Original retained at %1: %2").arg(snapshot.backupPath, restored.error)};
        }
        snapshot.backupPath.clear();
        snapshot.mutation = AutoStartSnapshot::Mutation::None;
        return {};
    }
    if (snapshot.mutation == AutoStartSnapshot::Mutation::Created) {
        AutoStartSnapshot current;
        const StorageResult inspected = snapshotRegularFile(path, current);
        if (!inspected.ok) {
            return inspected;
        }
        if (!current.existed) {
            snapshot.mutation = AutoStartSnapshot::Mutation::None;
            return {};
        }
        if (!identitiesMatch(identityFromSnapshot(current), publishedIdentityFromSnapshot(snapshot))) {
            return {false, QStringLiteral("Refusing to remove a substituted created entry at %1 during rollback").arg(path)};
        }
        const StorageResult removed = deleteExpectedFile(path, publishedIdentityFromSnapshot(snapshot));
        if (removed.ok) {
            snapshot.mutation = AutoStartSnapshot::Mutation::None;
        }
        return removed;
    }

    AutoStartSnapshot current;
    const StorageResult currentResult = snapshotRegularFile(path, current);
    if (!currentResult.ok) {
        return currentResult;
    }
    if (!snapshot.existed) {
        return current.existed
            ? StorageResult{false, QStringLiteral("Refusing to restore absence over a concurrent file at %1").arg(path)}
            : StorageResult{};
    }
    if (current.existed) {
        return identitiesMatch(identityFromSnapshot(current), identityFromSnapshot(snapshot))
            ? StorageResult{}
            : StorageResult{false, QStringLiteral("Refusing to overwrite a different regular file at %1 during rollback").arg(path)};
    }

    FileIdentity publishedIdentity;
    return publishRegularFileNoReplace(path,
                                       snapshot.contents,
                                       snapshot.permissions,
                                       snapshot.accessTimeNanoseconds,
                                       snapshot.modificationTimeNanoseconds,
                                       publishedIdentity);
}

StorageResult AppSettings::commitAutoStart(AutoStartSnapshot& snapshot) const {
    const StorageResult directoryAuthority =
        validateAutoStartDirectory(snapshot, QStringLiteral("commit Auto Start transaction"));
    if (!directoryAuthority.ok) {
        return directoryAuthority;
    }
    const QString path = pinnedAutoStartPath(snapshot, QStringLiteral("netstats-live.desktop"));
    if (snapshot.mutation == AutoStartSnapshot::Mutation::DisabledAbsent ||
        snapshot.mutation == AutoStartSnapshot::Mutation::StagedRemoval) {
        const StorageResult destinationAbsent = validateAbsentPath(path,
                                                                  QStringLiteral("commit Auto Start disable"));
        if (!destinationAbsent.ok) {
            return destinationAbsent;
        }
    } else if (snapshot.mutation == AutoStartSnapshot::Mutation::Replaced) {
        const StorageResult destinationValid = validateExpectedPath(path,
                                                                   publishedIdentityFromSnapshot(snapshot),
                                                                   QStringLiteral("commit Auto Start replacement"));
        if (!destinationValid.ok) {
            return destinationValid;
        }
    } else if (snapshot.mutation == AutoStartSnapshot::Mutation::Created) {
        const StorageResult destinationValid = validateExpectedPath(path,
                                                                   publishedIdentityFromSnapshot(snapshot),
                                                                   QStringLiteral("commit Auto Start creation"));
        if (!destinationValid.ok) {
            return destinationValid;
        }
    }
    const StorageResult finalDirectoryAuthority =
        validateAutoStartDirectory(snapshot, QStringLiteral("finish Auto Start transaction"));
    if (!finalDirectoryAuthority.ok) {
        return finalDirectoryAuthority;
    }
    snapshot.backupPath.clear();
    snapshot.mutation = AutoStartSnapshot::Mutation::None;
    return {};
}

StorageResult AppSettings::setAutoStart(bool enabled, const QString& executablePath) const {
    AutoStartSnapshot transaction;
    StorageResult result = snapshotAutoStart(transaction);
    if (!result.ok) {
        return result;
    }
    result = setAutoStart(enabled, executablePath, transaction);
    if (!result.ok) {
        const StorageResult rollback = restoreAutoStart(transaction);
        mergeResult(result, rollback);
        return result;
    }
    return commitAutoStart(transaction);
}

StorageResult AppSettings::setAutoStart(bool enabled,
                                        const QString& executablePath,
                                        AutoStartSnapshot& transaction) const {
    const StorageResult directoryAuthority =
        validateAutoStartDirectory(transaction, QStringLiteral("change Auto Start"));
    if (!directoryAuthority.ok) {
        return directoryAuthority;
    }
    const QString path = pinnedAutoStartPath(transaction, QStringLiteral("netstats-live.desktop"));
    if (enabled) {
        if (transaction.existed) {
            const StorageResult staged = stageRemoval(path,
                                                      identityFromSnapshot(transaction),
                                                      transaction.backupPath);
            if (!staged.ok) {
                return staged;
            }
            transaction.mutation = AutoStartSnapshot::Mutation::StagedRemoval;
        }
        const QString execPath = resolvedExecutablePath(executablePath);
        const QString desktop = QStringLiteral("[Desktop Entry]\n"
                                               "Type=Application\n"
                                               "Name=NetStats-Live\n"
                                               "Comment=Live network throughput and CPU monitor widget for Linux\n"
                                               "Exec=%1 --minimized\n"
                                               "Icon=netstats-live\n"
                                               "Terminal=false\n"
                                               "Categories=Network;Monitor;Qt;\n"
                                               "StartupWMClass=netstats-live\n"
                                               "X-KDE-autostart-after=panel\n")
                                    .arg(quoteExecArgument(execPath));
        const QFile::Permissions permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                               QFileDevice::ReadGroup | QFileDevice::ReadOther;
        FileIdentity publishedIdentity;
        const StorageResult published = publishRegularFileNoReplace(path,
                                                                    desktop.toUtf8(),
                                                                    permissions,
                                                                    -1,
                                                                    -1,
                                                                    publishedIdentity);
        if (published.ok) {
            transaction.publishedDevice = publishedIdentity.device;
            transaction.publishedInode = publishedIdentity.inode;
            transaction.publishedFileSize = publishedIdentity.fileSize;
            transaction.publishedContentHash = publishedIdentity.contentHash;
            transaction.publishedPermissions = permissions;
            transaction.mutation = transaction.existed
                ? AutoStartSnapshot::Mutation::Replaced
                : AutoStartSnapshot::Mutation::Created;
        }
        if (!published.ok) {
            return published;
        }
        return validateAutoStartDirectory(transaction, QStringLiteral("publish Auto Start entry"));
    }

    if (!transaction.existed) {
        struct stat status{};
        const QByteArray encodedPath = QFile::encodeName(path);
        if (::lstat(encodedPath.constData(), &status) == 0) {
            return {false, QStringLiteral("Refusing to disable Auto Start because %1 appeared after absence snapshot").arg(path)};
        }
        if (errno == ENOENT) {
            transaction.mutation = AutoStartSnapshot::Mutation::DisabledAbsent;
            return {};
        }
        return {false, QStringLiteral("Unable to inspect %1 before disabling Auto Start: %2").arg(path, systemError(errno))};
    }

    const StorageResult staged = stageRemoval(path, identityFromSnapshot(transaction), transaction.backupPath);
    if (staged.ok) {
        transaction.mutation = AutoStartSnapshot::Mutation::StagedRemoval;
    }
    if (!staged.ok) {
        return staged;
    }
    return validateAutoStartDirectory(transaction, QStringLiteral("stage Auto Start removal"));
}

} // namespace nsl
