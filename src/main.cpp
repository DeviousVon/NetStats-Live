#include "MainWindow.h"
#include "Lifecycle.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QIcon>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nsl-linux"));
    QCoreApplication::setOrganizationName(QStringLiteral("NSL-Linux"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("nsl-linux"), QIcon::fromTheme(QStringLiteral("network-workgroup"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("AnalogX NetStat Live style network monitor for Linux"));
    parser.addHelpOption();
    const QCommandLineOption minimizedOption(QStringLiteral("minimized"), QStringLiteral("Start hidden in the system tray"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("Render a deterministic visual-fidelity PNG and exit"),
                                              QStringLiteral("path"));
    QCommandLineOption simulateOption(QStringLiteral("simulate"), QStringLiteral("Run deterministic synthetic tray traffic"));
    simulateOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(minimizedOption);
    parser.addOption(screenshotOption);
    parser.addOption(simulateOption);
    parser.process(app);

    const bool screenshotMode = parser.isSet(screenshotOption);
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!screenshotMode && sessionBus.isConnected()) {
        if (!sessionBus.registerService(nsl::singleInstanceServiceName())) {
            QDBusInterface existing(nsl::singleInstanceServiceName(),
                                    nsl::singleInstanceObjectPath(),
                                    nsl::singleInstanceInterfaceName(),
                                    sessionBus);
            existing.call(QStringLiteral("activateFromInstanceRequest"));
            return 0;
        }
    }

    nsl::MainWindow window(parser.isSet(simulateOption));
    if (!screenshotMode && sessionBus.isConnected()) {
        sessionBus.registerObject(nsl::singleInstanceObjectPath(),
                                  nsl::singleInstanceInterfaceName(),
                                  &window,
                                  QDBusConnection::ExportScriptableSlots);
    }

    if (screenshotMode) {
        const QString outputPath = parser.value(screenshotOption);
        window.setPersistenceEnabled(false);
        window.populateScreenshotDemoData();
        window.show();
        QTimer::singleShot(150, &window, [&window, outputPath]() {
            const bool saved = !outputPath.isEmpty() && window.grab().save(outputPath, "PNG");
            QCoreApplication::exit(saved ? 0 : 2);
        });
        return QApplication::exec();
    }

    if (nsl::shouldShowMainWindow(parser.isSet(minimizedOption), window.autoMinimizeEnabled())) {
        window.show();
    }
    return QApplication::exec();
}
