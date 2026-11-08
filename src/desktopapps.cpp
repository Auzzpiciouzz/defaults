#include "desktopapps.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>

namespace {

// Desktop entry values escape \s, \n, \t, \r and \\; lists also escape \;.
QString unescape(const QString &value) {
    QString out;
    out.reserve(value.size());
    for (int i = 0; i < value.size(); ++i) {
        const QChar c = value.at(i);
        if (c != '\\' || i + 1 == value.size()) {
            out += c;
            continue;
        }
        const QChar next = value.at(++i);
        if (next == 's') out += ' ';
        else if (next == 'n') out += '\n';
        else if (next == 't') out += '\t';
        else if (next == 'r') out += '\r';
        else if (next == '\\') out += '\\';
        else if (next == ';') out += QStringLiteral("\\;");  // kept for splitList
        else { out += '\\'; out += next; }
    }
    return out;
}

QStringList splitList(const QString &value) {
    QStringList items;
    QString current;
    for (int i = 0; i < value.size(); ++i) {
        if (value.at(i) == '\\' && i + 1 < value.size() && value.at(i + 1) == ';') {
            current += ';';
            ++i;
        } else if (value.at(i) == ';') {
            if (!current.trimmed().isEmpty())
                items << current.trimmed();
            current.clear();
        } else {
            current += value.at(i);
        }
    }
    if (!current.trimmed().isEmpty())
        items << current.trimmed();
    return items;
}

bool isTrue(const QString &value) { return value.trimmed() == QLatin1String("true"); }

}  // namespace

QStringList DesktopApps::languagesFor(const QString &localeName) {
    // "pt_BR" or "pt-BR" → {"pt_BR", "pt"}; "C" and English need nothing.
    QString name = localeName;
    name.replace('-', '_');
    if (name.isEmpty() || name == QLatin1String("C"))
        return {};
    QStringList languages{name};
    const QString base = name.section('_', 0, 0);
    if (base != name)
        languages << base;
    return languages;
}

bool DesktopApps::parse(const QString &path, const QStringList &languages, DesktopApp *app) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QHash<QString, QString> keys;
    bool inEntry = false;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        if (line.startsWith('[')) {
            // Only the main group describes the app; actions come after it.
            if (inEntry)
                break;
            inEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inEntry)
            continue;
        const int equals = line.indexOf('=');
        if (equals <= 0)
            continue;
        const QString key = line.left(equals).trimmed();
        if (!keys.contains(key))
            keys.insert(key, line.mid(equals + 1).trimmed());
    }

    if (keys.value("Type") != QLatin1String("Application") || isTrue(keys.value("Hidden")))
        return false;

    // TryExec names a program that must be installed for the entry to count.
    const QString tryExec = unescape(keys.value("TryExec"));
    if (!tryExec.isEmpty()) {
        const QFileInfo info(tryExec);
        const bool found = info.isAbsolute() ? info.isExecutable()
                                             : !QStandardPaths::findExecutable(tryExec).isEmpty();
        if (!found)
            return false;
    }

    QString name;
    for (const auto &language : languages) {
        name = keys.value(QStringLiteral("Name[%1]").arg(language));
        if (!name.isEmpty())
            break;
    }
    if (name.isEmpty())
        name = keys.value("Name");

    app->path = path;
    app->name = unescape(name).trimmed();
    app->icon = unescape(keys.value("Icon")).trimmed();
    app->mimeTypes = splitList(unescape(keys.value("MimeType")));
    app->noDisplay = isTrue(keys.value("NoDisplay"));
    if (app->name.isEmpty())
        app->name = QFileInfo(path).completeBaseName();
    return true;
}

void DesktopApps::scan(const QStringList &applicationDirectories, const QStringList &languages) {
    m_apps.clear();
    m_index.clear();

    // A desktop id is claimed by the first directory that has it, even when
    // that copy is hidden, so a user's Hidden=true override removes the app.
    QSet<QString> claimed;
    for (const auto &directory : applicationDirectories) {
        const QDir root(directory);
        if (!root.exists())
            continue;
        QDirIterator it(directory, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        QStringList files;
        while (it.hasNext())
            files << it.next();
        files.sort();
        for (const auto &path : files) {
            const QString id = root.relativeFilePath(path).replace('/', '-');
            if (claimed.contains(id))
                continue;
            claimed.insert(id);
            DesktopApp app;
            if (!parse(path, languages, &app))
                continue;
            app.id = id;
            m_index.insert(id, m_apps.size());
            m_apps << app;
        }
    }
}

const DesktopApp *DesktopApps::find(const QString &id) const {
    const auto it = m_index.constFind(id);
    return it == m_index.constEnd() ? nullptr : &m_apps.at(it.value());
}
