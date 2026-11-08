#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "associations.h"
#include "backend.h"
#include "desktopapps.h"
#include "mimeapps.h"
#include "theme.h"
#include "translator.h"

// A throwaway XDG tree: ~/.config, ~/.local/share and one system data dir.
struct Sandbox {
    QTemporaryDir dir;
    MimePaths paths;

    Sandbox() {
        paths.configHome = dir.filePath("config");
        paths.configDirs = {dir.filePath("etc")};
        paths.dataHome = dir.filePath("local");
        paths.dataDirs = {dir.filePath("usr")};
        paths.desktops = {"hyprland"};
        QDir().mkpath(paths.configHome);
        QDir().mkpath(paths.dataHome + "/applications");
        QDir().mkpath(dir.filePath("usr/applications"));
    }

    QString systemApp(const QString &id, const QString &name, const QString &mimes, const QString &extra = {}) {
        const QString path = dir.filePath("usr/applications/" + id);
        put(path, "[Desktop Entry]\nType=Application\nName=" + name.toUtf8() + "\nExec=" + name.toLower().toUtf8()
                      + "\nMimeType=" + mimes.toUtf8() + "\n" + extra.toUtf8());
        return path;
    }

    static void put(const QString &path, const QByteArray &contents) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents);
    }

    static QString read(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }
};

class DefaultsTests : public QObject {
    Q_OBJECT

private slots:
    void parsesDesktopEntries()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("viewer.desktop");
        Sandbox::put(path, "[Desktop Entry]\nType=Application\nName=Viewer\nName[da]=Fremviser\n"
                           "Icon=viewer\nMimeType=image/png;image/jpeg;\nNoDisplay=true\n"
                           "[Desktop Action new]\nName=Ignored\n");
        DesktopApp app;
        QVERIFY(DesktopApps::parse(path, {"da"}, &app));
        QCOMPARE(app.name, QString("Fremviser"));
        QCOMPARE(app.icon, QString("viewer"));
        QCOMPARE(app.mimeTypes, QStringList({"image/png", "image/jpeg"}));
        QVERIFY(app.noDisplay);

        QVERIFY(DesktopApps::parse(path, {"pt_BR", "pt"}, &app));
        QCOMPARE(app.name, QString("Viewer"));
    }

    void skipsHiddenAndMissingPrograms()
    {
        Sandbox box;
        box.systemApp("gone.desktop", "Gone", "image/png;", "TryExec=/nonexistent/program\n");
        box.systemApp("masked.desktop", "Masked", "image/png;");
        Sandbox::put(box.paths.dataHome + "/applications/masked.desktop", "[Desktop Entry]\nType=Application\nName=Masked\nHidden=true\n");
        box.systemApp("kept.desktop", "Kept", "image/png;");

        DesktopApps apps;
        apps.scan(box.paths.applicationDirectories());
        QVERIFY(!apps.contains("gone.desktop"));
        QVERIFY(!apps.contains("masked.desktop"));
        QVERIFY(apps.contains("kept.desktop"));
    }

    void subdirectoriesMakeDesktopIds()
    {
        Sandbox box;
        box.systemApp("kde/okular.desktop", "Okular", "application/pdf;");
        DesktopApps apps;
        apps.scan(box.paths.applicationDirectories());
        QVERIFY(apps.contains("kde-okular.desktop"));
    }

    void resolvesInSpecOrder()
    {
        Sandbox box;
        box.systemApp("a.desktop", "A", "image/png;");
        box.systemApp("b.desktop", "B", "image/png;");
        Sandbox::put(box.dir.filePath("usr/applications/mimeapps.list"), "[Default Applications]\nimage/png=a.desktop\n");

        Associations associations(box.paths);
        QCOMPARE(associations.resolve("image/png").appId, QString("a.desktop"));
        QVERIFY(!associations.resolve("image/png").locked);

        // The user's list beats the system one, and an uninstalled first
        // choice passes to the next app on the same line.
        Sandbox::put(box.paths.userFile(), "[Default Applications]\nimage/png=missing.desktop;b.desktop;\n");
        associations.reload();
        QCOMPARE(associations.resolve("image/png").appId, QString("b.desktop"));
    }

    void desktopSpecificListLocksTypes()
    {
        Sandbox box;
        box.systemApp("browser.desktop", "Browser", "x-scheme-handler/https;");
        box.systemApp("router.desktop", "Router", "x-scheme-handler/https;");
        Sandbox::put(box.paths.configHome + "/hyprland-mimeapps.list",
                     "[Default Applications]\nx-scheme-handler/https=router.desktop\n");
        Sandbox::put(box.paths.userFile(), "[Default Applications]\nx-scheme-handler/https=browser.desktop\n");

        Associations associations(box.paths);
        const Resolved resolved = associations.resolve("x-scheme-handler/https");
        QCOMPARE(resolved.appId, QString("router.desktop"));
        QVERIFY(resolved.locked);
        QVERIFY(associations.plan({"x-scheme-handler/https"}, "browser.desktop").isEmpty());
    }

    void fallsBackLikeXdgOpen()
    {
        Sandbox box;
        box.systemApp("viewer.desktop", "Viewer", "image/png;");
        Sandbox::put(box.dir.filePath("usr/applications/mimeinfo.cache"), "[MIME Cache]\nimage/png=viewer.desktop;\n");

        Associations associations(box.paths);
        const Resolved resolved = associations.resolve("image/png");
        QCOMPARE(resolved.appId, QString("viewer.desktop"));
        QVERIFY(resolved.automatic);
        // An automatic pick is still worth writing down.
        QCOMPARE(associations.plan({"image/png"}, "viewer.desktop").size(), 1);
    }

    void supportCountsAliasesWildcardsAndParents()
    {
        Sandbox box;
        box.systemApp("player.desktop", "Player", "audio/x-flac;");
        box.systemApp("gallery.desktop", "Gallery", "image/*;");
        box.systemApp("editor.desktop", "Editor", "text/plain;");
        Associations associations(box.paths);
        const auto &apps = associations.apps();

        QCOMPARE(associations.support(*apps.find("player.desktop"), "audio/flac"), 2);
        QCOMPARE(associations.support(*apps.find("gallery.desktop"), "image/webp"), 2);
        QCOMPARE(associations.support(*apps.find("gallery.desktop"), "video/mp4"), 0);
        QCOMPARE(associations.support(*apps.find("editor.desktop"), "text/x-csrc"), 1);
        QCOMPARE(associations.support(*apps.find("editor.desktop"), "image/png"), 0);
    }

    void candidatesRankByCoverage()
    {
        Sandbox box;
        box.systemApp("small.desktop", "Small", "image/png;");
        box.systemApp("big.desktop", "Big", "image/png;image/jpeg;image/gif;");
        Associations associations(box.paths);
        const auto list = associations.candidates({"image/png", "image/jpeg", "image/gif"});
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(0).app->id, QString("big.desktop"));
        QCOMPARE(list.at(0).covered, 3);
        QCOMPARE(list.at(1).covered, 1);
    }

    void writesOnlyDefaultsAndKeepsTheRest()
    {
        Sandbox box;
        box.systemApp("viewer.desktop", "Viewer", "image/png;video/x-matroska;");
        Sandbox::put(box.paths.userFile(),
                     "# my settings\n[Added Associations]\nimage/png=other.desktop;\n\n"
                     "[Default Applications]\ntext/html=zen.desktop\nvideo/x-matroska=old.desktop\n\n"
                     "[Removed Associations]\nimage/gif=foo.desktop;\n");

        Associations associations(box.paths);
        QString error;
        QVERIFY2(associations.apply(associations.plan({"image/png", "video/matroska"}, "viewer.desktop"), false, &error),
                 qPrintable(error));

        const QString text = Sandbox::read(box.paths.userFile());
        QVERIFY(text.startsWith("# my settings\n[Added Associations]\nimage/png=other.desktop;\n"));
        QVERIFY(text.contains("text/html=zen.desktop\n"));
        QVERIFY(text.contains("image/png=viewer.desktop\n"));
        // Both spellings of Matroska, so xdg-mime finds it whichever name it asks for.
        QVERIFY(text.contains("video/matroska=viewer.desktop\n"));
        QVERIFY(text.contains("video/x-matroska=viewer.desktop\n"));
        QVERIFY(!text.contains("old.desktop"));
        QVERIFY(text.contains("\n\n[Removed Associations]\nimage/gif=foo.desktop;\n"));
        QCOMPARE(associations.resolve("image/png").appId, QString("viewer.desktop"));
    }

    void createsTheFileWhenMissing()
    {
        Sandbox box;
        box.systemApp("viewer.desktop", "Viewer", "application/pdf;");
        QDir(box.paths.configHome).removeRecursively();
        Associations associations(box.paths);
        QString error;
        QVERIFY(associations.apply(associations.plan({"application/pdf"}, "viewer.desktop"), false, &error));
        const QString text = Sandbox::read(box.paths.userFile());
        QVERIFY(text.startsWith("[Default Applications]\napplication/pdf=viewer.desktop\n"));
        QVERIFY(text.contains("application/x-pdf=viewer.desktop\n"));
    }

    void undoRestoresExactly()
    {
        Sandbox box;
        box.systemApp("a.desktop", "A", "image/png;image/jpeg;");
        Sandbox::put(box.paths.userFile(), "[Default Applications]\nimage/png=old.desktop;fallback.desktop;\n");
        Associations associations(box.paths);
        const auto changes = associations.plan({"image/png", "image/jpeg"}, "a.desktop");
        QCOMPARE(changes.size(), 2);
        QString error;
        QVERIFY(associations.apply(changes, false, &error));
        QVERIFY(associations.apply(changes, true, &error));
        QCOMPARE(Sandbox::read(box.paths.userFile()),
                 QString("[Default Applications]\nimage/png=old.desktop;fallback.desktop;\n"));
    }

    void followsSymlinkedLists()
    {
        Sandbox box;
        box.systemApp("a.desktop", "A", "application/pdf;");
        const QString real = box.dir.filePath("dotfiles/mimeapps.list");
        Sandbox::put(real, "[Default Applications]\n");
        QVERIFY(QFile::link(real, box.paths.userFile()));
        Associations associations(box.paths);
        QString error;
        QVERIFY(associations.apply(associations.plan({"application/pdf"}, "a.desktop"), false, &error));
        QVERIFY(QFileInfo(box.paths.userFile()).isSymLink());
        QVERIFY(Sandbox::read(real).contains("application/pdf=a.desktop"));
    }

    void backendSetsCategoriesAndUndoes()
    {
        Sandbox box;
        box.systemApp("viewer.desktop", "Viewer", "image/png;image/jpeg;");
        Backend backend(box.paths);
        QSignalSpy history(&backend, &Backend::historyChanged);

        QVERIFY(backend.setCategory("images", "viewer.desktop"));
        QVERIFY(backend.canUndo());
        QVERIFY(backend.status().contains("Viewer"));
        QVariantMap images;
        for (const auto &row : backend.categories())
            if (row.toMap().value("id") == "images")
                images = row.toMap();
        QCOMPARE(images.value("appId").toString(), QString("viewer.desktop"));
        QVERIFY(!images.value("automatic").toBool());

        backend.undo();
        QVERIFY(!backend.canUndo());
        QVERIFY(backend.canRedo());
        QVERIFY(!Sandbox::read(box.paths.userFile()).contains("viewer.desktop"));
        backend.redo();
        QVERIFY(Sandbox::read(box.paths.userFile()).contains("image/png=viewer.desktop"));
        QCOMPARE(history.count(), 3);
    }

    void backendRefusesLockedTypes()
    {
        Sandbox box;
        box.systemApp("browser.desktop", "Browser", "x-scheme-handler/https;");
        Sandbox::put(box.paths.configHome + "/hyprland-mimeapps.list",
                     "[Default Applications]\nx-scheme-handler/https=browser.desktop\n");
        Backend backend(box.paths);
        QVERIFY(!backend.setType("x-scheme-handler/https", "browser.desktop"));
        QVERIFY(backend.statusIsError());
        QVERIFY(!QFile::exists(box.paths.userFile()));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        Sandbox::put(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        Sandbox::put(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Sandbox::put(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        Sandbox::put(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        Sandbox::put(dir.path() + "/language", "pt-BR\n");
        Sandbox::put(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        Sandbox::put(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void englishCatalogHasPlurals()
    {
        QTemporaryDir dir;
        QFile::copy(QStringLiteral(SRCDIR) + "/../src/i18n/en.json", dir.path() + "/en.json");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n types", nullptr, 1), QString("%n type"));
        QCOMPARE(translator.translate("Main", "%n types", nullptr, 4), QString("%n types"));
    }
};

QTEST_MAIN(DefaultsTests)
#include "defaults_tests.moc"
