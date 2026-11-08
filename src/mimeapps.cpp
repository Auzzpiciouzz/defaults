#include "mimeapps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QTextStream>

namespace {

QStringList envList(const char *name, const QStringList &fallback) {
    const QString value = qEnvironmentVariable(name);
    QStringList list;
    for (const auto &part : value.split(':', Qt::SkipEmptyParts))
        list << QDir::cleanPath(part);
    return list.isEmpty() ? fallback : list;
}

QString envPath(const char *name, const QString &fallback) {
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() || !QDir::isAbsolutePath(value) ? fallback : QDir::cleanPath(value);
}

bool isDefaultsHeader(const QString &line) { return line.trimmed() == QLatin1String("[Default Applications]"); }
bool isGroupHeader(const QString &line) { return line.trimmed().startsWith('['); }

QString keyOf(const QString &line) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith('#'))
        return {};
    const int equals = trimmed.indexOf('=');
    return equals > 0 ? trimmed.left(equals).trimmed() : QString();
}

QStringList readLines(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QStringList lines;
    QTextStream stream(&file);
    while (!stream.atEnd())
        lines << stream.readLine();
    return lines;
}

}  // namespace

QString canonicalMime(const QString &mime) {
    static const QMimeDatabase database;
    const QString trimmed = mime.trimmed();
    const QMimeType type = database.mimeTypeForName(trimmed);
    return type.isValid() ? type.name() : trimmed;
}

QStringList mimeNames(const QString &mime) {
    static const QMimeDatabase database;
    const QString name = canonicalMime(mime);
    QStringList names{name};
    const QMimeType type = database.mimeTypeForName(name);
    if (type.isValid()) {
        QStringList aliases = type.aliases();
        aliases.sort();
        names << aliases;
    }
    names.removeDuplicates();
    return names;
}

MimePaths MimePaths::fromEnvironment() {
    MimePaths paths;
    paths.configHome = envPath("XDG_CONFIG_HOME", QDir::homePath() + "/.config");
    paths.configDirs = envList("XDG_CONFIG_DIRS", {"/etc/xdg"});
    paths.dataHome = envPath("XDG_DATA_HOME", QDir::homePath() + "/.local/share");
    paths.dataDirs = envList("XDG_DATA_DIRS", {"/usr/local/share", "/usr/share"});
    for (const auto &desktop : qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':', Qt::SkipEmptyParts))
        paths.desktops << desktop.toLower();
    return paths;
}

QStringList MimePaths::listFiles() const {
    QStringList files;
    auto add = [&](const QString &directory) {
        for (const auto &desktop : desktops)
            files << directory + "/" + desktop + "-mimeapps.list";
        files << directory + "/mimeapps.list";
    };
    add(configHome);
    for (const auto &directory : configDirs)
        add(directory);
    add(dataHome + "/applications");
    for (const auto &directory : dataDirs)
        add(directory + "/applications");
    files.removeDuplicates();
    return files;
}

QStringList MimePaths::applicationDirectories() const {
    QStringList directories{dataHome + "/applications"};
    for (const auto &directory : dataDirs)
        directories << directory + "/applications";
    directories.removeDuplicates();
    return directories;
}

DefaultsTable MimeAppsFile::readGroup(const QString &path, const QString &group) {
    DefaultsTable table;
    const QString header = "[" + group + "]";
    bool inDefaults = false;
    for (const auto &line : readLines(path)) {
        if (isGroupHeader(line)) {
            inDefaults = line.trimmed() == header;
            continue;
        }
        const QString key = keyOf(line);
        if (!inDefaults || key.isEmpty())
            continue;
        const QString mime = canonicalMime(key);
        // When a file lists a type twice (say, under an alias), the first wins.
        if (table.contains(mime))
            continue;
        QStringList apps;
        for (const auto &app : line.mid(line.indexOf('=') + 1).split(';', Qt::SkipEmptyParts))
            if (!app.trimmed().isEmpty())
                apps << app.trimmed();
        table.insert(mime, apps);
    }
    return table;
}

QString MimeAppsFile::rawValue(const QString &path, const QString &mime) {
    const QString wanted = canonicalMime(mime);
    bool inDefaults = false;
    for (const auto &line : readLines(path)) {
        if (isGroupHeader(line)) {
            inDefaults = isDefaultsHeader(line);
            continue;
        }
        const QString key = keyOf(line);
        if (inDefaults && !key.isEmpty() && canonicalMime(key) == wanted)
            return line.mid(line.indexOf('=') + 1).trimmed();
    }
    return {};
}

bool MimeAppsFile::write(const QString &path, const QList<Edit> &edits, QString *error) {
    QStringList lines = readLines(path);
    if (lines.isEmpty() && QFileInfo::exists(path) && QFileInfo(path).size() > 0) {
        if (error)
            *error = QObject::tr("Can't read %1.").arg(path);
        return false;
    }

    int header = -1;
    for (int i = 0; i < lines.size(); ++i)
        if (isDefaultsHeader(lines.at(i))) {
            header = i;
            break;
        }
    if (header < 0) {
        // A new group goes first, where people look for it.
        const bool separate = !lines.isEmpty();
        lines.prepend("[Default Applications]");
        if (separate)
            lines.insert(1, QString());
        header = 0;
    }

    for (const auto &edit : edits) {
        const QString mime = canonicalMime(edit.mime);
        // Drop every line in the group for this type, alias spellings included.
        int end = header + 1;
        while (end < lines.size() && !isGroupHeader(lines.at(end))) {
            const QString key = keyOf(lines.at(end));
            if (!key.isEmpty() && canonicalMime(key) == mime)
                lines.removeAt(end);
            else
                ++end;
        }
        if (edit.value.isNull())
            continue;
        // Append after the group's last non-blank line, so a blank line
        // separating it from the next group stays where it is.
        int insertAt = end;
        while (insertAt > header + 1 && lines.at(insertAt - 1).trimmed().isEmpty())
            --insertAt;
        // Tools name types differently (file(1) says video/x-matroska,
        // shared-mime-info says video/matroska), and xdg-mime matches names
        // exactly, so write the type under every name it goes by.
        const QStringList names = mimeNames(mime);
        for (int n = names.size() - 1; n >= 0; --n)
            lines.insert(insertAt, names.at(n) + "=" + edit.value);
    }

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error)
            *error = QObject::tr("Can't create %1.").arg(QFileInfo(path).absolutePath());
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QObject::tr("Can't write %1: %2").arg(path, file.errorString());
        return false;
    }
    const QByteArray contents = (lines.join('\n') + '\n').toUtf8();
    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error)
            *error = QObject::tr("Can't write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}
