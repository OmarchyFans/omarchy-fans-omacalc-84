#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <qqml.h>
#include <QQmlContext>
#include <QQmlError>
#include <QDebug>
#include <QQuickStyle>
#include <QUrl>
#include <QWindow>
#include <QFile>
#include <QQuickWindow>
#include <QTimer>

#include "backend.h"
#include "graphview.h"
#include "systemtheme.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omagraph"));
    app.setDesktopFileName(QStringLiteral("omagraph"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omagraph")));

    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-Bold.ttf"));
    app.setOrganizationName(QStringLiteral("omarchy.fans"));
    app.setOrganizationDomain(QStringLiteral("omarchy.fans"));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    qmlRegisterType<GraphView>("Omacalc", 1, 0, "GraphView");

    Backend backend(&app);
    SystemTheme systemTheme(&app);
    backend.setDarkMode(systemTheme.darkMode());
    QObject::connect(&systemTheme, &SystemTheme::darkModeChanged, &backend,
                     &Backend::setDarkMode);

    // Carry the desktop's text scale into the default font so the chrome that
    // inherits it (menus, dialogs) grows along with the keypad.
    const QFont interfaceFont(QStringLiteral("iA Writer Mono S"));
    const qreal basePointSize = interfaceFont.pointSizeF() > 0
        ? interfaceFont.pointSizeF()
        : app.font().pointSizeF();
    const auto applyInterfaceFont = [&app, interfaceFont, basePointSize](qreal textScale) {
        QFont scaled = interfaceFont;
        scaled.setPointSizeF(basePointSize * textScale);
        app.setFont(scaled);
    };
    applyInterfaceFont(systemTheme.textScale());

    backend.setTextScale(systemTheme.textScale());
    QObject::connect(&systemTheme, &SystemTheme::textScaleChanged, &backend,
                     [&backend, applyInterfaceFont](qreal textScale) {
        applyInterfaceFont(textScale);
        backend.setTextScale(textScale);
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning().noquote() << warning.toString();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    // --screenshot writes the interface to a file and exits, which is how the
    // look of the calculator is checked without a display attached.
    QString screenshotPath;
    const QStringList arguments = app.arguments();

    // --screen opens straight onto one of the screens, for a launcher binding
    // that goes right to the graph.
    int initialScreen = 0;
    const QStringList screenNames = {QStringLiteral("home"),   QStringLiteral("equations"),
                                     QStringLiteral("window"), QStringLiteral("graph"),
                                     QStringLiteral("table"),  QStringLiteral("lists"),
                                     QStringLiteral("stats"),  QStringLiteral("matrix"),
                                     QStringLiteral("mode"),  QStringLiteral("tests")};
    const int screenFlag = arguments.indexOf(QStringLiteral("--screen"));
    if (screenFlag >= 0 && screenFlag + 1 < arguments.size()) {
        const int named = screenNames.indexOf(arguments.at(screenFlag + 1));
        if (named >= 0)
            initialScreen = named;
    }
    engine.rootContext()->setContextProperty(QStringLiteral("initialScreen"), initialScreen);

    // --eval works the calculator from the command line: each expression is
    // entered as if it had been typed, and the answers are waiting on the home
    // screen when the window opens.
    for (int i = 0; i + 1 < arguments.size(); ++i) {
        if (arguments.at(i) != QStringLiteral("--eval"))
            continue;
        const QVariantMap entry = backend.submit(arguments.at(i + 1));
        qInfo().noquote() << entry.value(QStringLiteral("answer")).toString();
    }
    const int screenshotFlag = arguments.indexOf(QStringLiteral("--screenshot"));
    if (screenshotFlag >= 0 && screenshotFlag + 1 < arguments.size())
        screenshotPath = arguments.at(screenshotFlag + 1);

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Could not load the Omagraph interface; resource available:"
                    << QFile::exists(QStringLiteral(":/Main.qml"));
        return -1;
    }

    if (!screenshotPath.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(900, &app, [window, screenshotPath, &app]() {
            if (window && window->grabWindow().save(screenshotPath))
                qInfo().noquote() << "wrote" << screenshotPath;
            else
                qWarning() << "could not write" << screenshotPath;
            app.quit();
        });
    }

    return app.exec();
}
