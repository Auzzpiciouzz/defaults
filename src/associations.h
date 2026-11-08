#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "desktopapps.h"
#include "mimeapps.h"

// A kind of file people think of as one thing ("Images"), standing for the
// MIME types that make it up.
struct Category {
    QString id;
    const char *title;  // English; translated where it's shown
    QString icon;       // a theme icon for the kind of file
    QStringList mimes;  // canonical names
};

// What opens a type now and where that came from.
struct Resolved {
    QString appId;    // empty when no file sets an installed app
    QString source;   // the mimeapps.list it came from
    bool locked = false;  // set by a file read before the user's own, so writing there won't change it
    bool automatic = false;  // no default is set; this is the app xdg-open falls back to
};

// An app that can open some of the types asked about.
struct Candidate {
    const DesktopApp *app = nullptr;
    int direct = 0;     // types it lists itself
    int covered = 0;    // types it can open, also through a parent type
};

// One change to the user's mimeapps.list. Values are what the line held,
// a null string meaning no line, so a change can be undone exactly.
struct Change {
    QString mime;
    QString before;
    QString after;
};

// Reads installed apps and every mimeapps.list, and answers what opens what.
// The GUI and the command line share it.
class Associations {
public:
    explicit Associations(const MimePaths &paths = MimePaths::fromEnvironment());

    void reload();

    const MimePaths &paths() const { return m_paths; }
    const DesktopApps &apps() const { return m_apps; }

    Resolved resolve(const QString &mime) const;

    // 2 when the app lists the type (an alias or a "image/*" wildcard counts),
    // 1 when it lists a parent type (text/plain for a C file), 0 otherwise.
    int support(const DesktopApp &app, const QString &mime) const;

    // Apps that can open at least one of the types, the ones covering the
    // most first, then by name.
    QList<Candidate> candidates(const QStringList &mimes) const;

    // Every type an installed app says it opens, plus any the user's file
    // already sets, sorted.
    QStringList knownTypes() const;

    // The changes that make the app open each of these types it can open
    // and that isn't locked. Types it already opens are left out.
    QList<Change> plan(const QStringList &mimes, const QString &appId) const;
    bool apply(const QList<Change> &changes, bool undo, QString *error);

    static const QList<Category> &categories();
    static const Category *category(const QString &id);

    // A short name for a type: its shared-mime-info description, or
    // "mailto links" for a URL scheme.
    static QString describe(const QString &mime);
    static QStringList patterns(const QString &mime);

private:
    MimePaths m_paths;
    QStringList m_files;
    QList<DefaultsTable> m_tables;
    QList<DefaultsTable> m_added;   // [Added Associations], same order as m_files
    QList<DefaultsTable> m_caches;  // mimeinfo.cache, in application directory order
    int m_userIndex = 0;
    DesktopApps m_apps;
};
