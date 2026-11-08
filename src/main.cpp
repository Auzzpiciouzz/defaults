// defaults — choose which app opens each kind of file on Omarchy.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "iconprovider.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // Commands for scripts and agents run without a display.
    if (Cli::handles(arguments)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("defaults");
        app.setApplicationVersion("0.1.0");
        return Cli::run(arguments);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("defaults");
    app.setApplicationVersion("0.1.0");

    // Matches the window to defaults.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("defaults");
    IconProvider::configureTheme();
    app.setWindowIcon(QIcon::fromTheme("defaults"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    engine.addImageProvider("icon", new IconProvider);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    // Category names come from C++, so rebuild them in the new language too.
    QObject::connect(&translator, &Translator::changed, &backend, &Backend::reload);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
