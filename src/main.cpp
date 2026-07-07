#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nsl-linux"));
    QCoreApplication::setOrganizationName(QStringLiteral("NSL-Linux"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("network-workgroup")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("AnalogX NetStat Live style network monitor for Linux"));
    parser.addHelpOption();
    const QCommandLineOption minimizedOption(QStringLiteral("minimized"), QStringLiteral("Start hidden in the system tray"));
    parser.addOption(minimizedOption);
    parser.process(app);

    nsl::MainWindow window;
    if (!parser.isSet(minimizedOption) && !window.autoMinimizeEnabled()) {
        window.show();
    }
    return QApplication::exec();
}
