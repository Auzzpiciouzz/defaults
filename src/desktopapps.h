#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// One installed application, read from its .desktop file.
struct DesktopApp {
    QString id;          // "org.gnome.Nautilus.desktop", the name mimeapps.list uses
    QString name;        // in the user's language when the file has it
    QString icon;        // a theme icon name or an absolute path
    QString path;        // where the .desktop file lives
    QStringList mimeTypes;
    bool noDisplay = false;
};

// Finds applications the way the XDG desktop entry spec does: the first
// directory that has a given desktop id wins, so a copy in
// ~/.local/share/applications hides (or, with Hidden=true, removes) the
// system one.
class DesktopApps {
public:
    // Directories in priority order, each an ".../applications" folder.
    // Languages are the locale suffixes to prefer for Name, such as
    // {"pt_BR", "pt"}.
    void scan(const QStringList &applicationDirectories, const QStringList &languages = {});

    const QList<DesktopApp> &all() const { return m_apps; }
    const DesktopApp *find(const QString &id) const;
    bool contains(const QString &id) const { return m_index.contains(id); }

    // Parses one file. Returns false for anything that isn't a usable
    // application: not Type=Application, Hidden, or a TryExec that isn't
    // installed.
    static bool parse(const QString &path, const QStringList &languages, DesktopApp *app);

    static QStringList languagesFor(const QString &localeName);

private:
    QList<DesktopApp> m_apps;
    QHash<QString, int> m_index;
};
