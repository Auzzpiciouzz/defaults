#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// Where the XDG MIME applications spec looks, and in which order.
struct MimePaths {
    QString configHome;       // ~/.config
    QStringList configDirs;   // /etc/xdg
    QString dataHome;         // ~/.local/share
    QStringList dataDirs;     // /usr/local/share, /usr/share
    QStringList desktops;     // lowercased XDG_CURRENT_DESKTOP, such as {"hyprland"}

    static MimePaths fromEnvironment();

    // Every mimeapps.list that can hold a default, most important first.
    QStringList listFiles() const;
    // The one file this app writes. Anything above it in listFiles() wins
    // over what we write there.
    QString userFile() const { return configHome + "/mimeapps.list"; }
    QStringList applicationDirectories() const;
};

// The [Default Applications] group of one mimeapps.list, keyed by canonical
// MIME type, each with its apps in preference order.
using DefaultsTable = QHash<QString, QStringList>;

class MimeAppsFile {
public:
    static DefaultsTable readDefaults(const QString &path) { return readGroup(path, "Default Applications"); }
    // Any group with type=app;app lines: "Added Associations" in a
    // mimeapps.list, "MIME Cache" in an applications/mimeinfo.cache.
    static DefaultsTable readGroup(const QString &path, const QString &group);

    // The raw value stored for a type in the user's file (alias keys count),
    // or a null string when the file says nothing about it.
    static QString rawValue(const QString &path, const QString &mime);

    // Sets each type's value in [Default Applications], or removes the type
    // when the value is null. Keys that are aliases of the same type go too,
    // so only one line decides. Every other line and group is kept as it was.
    // Writes beside the file and renames it into place.
    struct Edit { QString mime; QString value; };
    static bool write(const QString &path, const QList<Edit> &edits, QString *error);
};

// The canonical name of a MIME type ("audio/x-flac" → "audio/flac"). Types
// shared-mime-info doesn't know, like x-scheme-handler/mailto, stay as given.
QString canonicalMime(const QString &mime);

// The canonical name followed by every alias, the names a type can be
// written under.
QStringList mimeNames(const QString &mime);
