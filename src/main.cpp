#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QIcon>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nsl-linux"));
    QCoreApplication::setOrganizationName(QStringLiteral("NSL-Linux"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("network-workgroup")));

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

    nsl::MainWindow window(parser.isSet(simulateOption));
    if (parser.isSet(screenshotOption)) {
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

    if (!parser.isSet(minimizedOption) && !window.autoMinimizeEnabled()) {
        window.show();
    }
    return QApplication::exec();
}
