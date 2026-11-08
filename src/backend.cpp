#include "backend.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QStandardPaths>
#include <algorithm>
#include <numeric>

Backend::Backend(const MimePaths &paths, QObject *parent) : QObject(parent), m_associations(paths) {
    // Other tools (Omarchy's own commands, a text editor, a package install)
    // change these files too; follow them so the window never shows stale
    // defaults.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { if (!m_writing) m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { if (!m_writing) m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        m_associations.reload();
        rebuild();
        watch();
    });
    rebuild();
    watch();
}

void Backend::watch() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    QStringList wanted = m_associations.paths().applicationDirectories();
    wanted << m_associations.paths().configHome;
    for (const auto &file : m_associations.paths().listFiles())
        wanted << file;
    for (const auto &path : wanted)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);
}

QString Backend::appName(const QString &id) const {
    const DesktopApp *app = m_associations.apps().find(id);
    return app ? app->name : id;
}

QVariantMap Backend::appMap(const DesktopApp *app) const {
    if (!app)
        return {{"id", QString()}, {"name", QString()}, {"icon", QString()}};
    return {{"id", app->id}, {"name", app->name}, {"icon", app->icon}};
}

void Backend::rebuild() {
    m_categories.clear();
    for (const auto &category : Associations::categories()) {
        // A category shows the app that opens most of its types, and says
        // when others open some of them.
        QHash<QString, int> tally, chosen;
        int locked = 0;
        for (const auto &mime : category.mimes) {
            const Resolved resolved = m_associations.resolve(mime);
            if (!resolved.appId.isEmpty())
                ++tally[resolved.appId];
            if (!resolved.appId.isEmpty() && !resolved.automatic)
                ++chosen[resolved.appId];
            if (resolved.locked)
                ++locked;
        }
        QString top;
        for (auto it = tally.constBegin(); it != tally.constEnd(); ++it)
            if (top.isEmpty() || it.value() > tally.value(top)
                || (it.value() == tally.value(top) && it.key() < top))
                top = it.key();
        const int set = std::accumulate(tally.cbegin(), tally.cend(), 0);
        QVariantMap row = appMap(m_associations.apps().find(top));
        row.insert("appId", row.take("id"));
        row.insert("appName", row.take("name"));
        row.insert("appIcon", row.take("icon"));
        row.insert("id", category.id);
        row.insert("title", tr(category.title));
        row.insert("icon", category.icon);
        row.insert("typeCount", int(category.mimes.size()));
        row.insert("otherApps", int(tally.size()) - (top.isEmpty() ? 0 : 1));
        row.insert("unset", int(category.mimes.size()) - set);
        row.insert("locked", locked == category.mimes.size());
        // Nobody picked it; it's what xdg-open falls back to.
        row.insert("automatic", !top.isEmpty() && chosen.value(top) == 0);
        row.insert("candidates", int(m_associations.candidates(category.mimes).size()));
        m_categories << row;
    }

    m_types.clear();
    for (const auto &mime : m_associations.knownTypes()) {
        const Resolved resolved = m_associations.resolve(mime);
        QVariantMap row = appMap(m_associations.apps().find(resolved.appId));
        row.insert("appId", row.take("id"));
        row.insert("appName", row.take("name"));
        row.insert("appIcon", row.take("icon"));
        row.insert("mime", mime);
        row.insert("description", Associations::describe(mime));
        row.insert("patterns", Associations::patterns(mime).join(' '));
        row.insert("locked", resolved.locked);
        row.insert("automatic", resolved.automatic);
        row.insert("lockedBy", resolved.locked ? QFileInfo(resolved.source).fileName() : QString());
        m_types << row;
    }
    emit modelChanged();
}

QVariantList Backend::candidateList(const QStringList &mimes, const QString &current) const {
    QVariantList list;
    for (const auto &candidate : m_associations.candidates(mimes)) {
        QVariantMap row = appMap(candidate.app);
        row.insert("covered", candidate.covered);
        row.insert("total", int(mimes.size()));
        row.insert("current", candidate.app->id == current);
        list << row;
    }
    return list;
}

QVariantList Backend::categoryCandidates(const QString &categoryId) const {
    const Category *category = Associations::category(categoryId);
    if (!category)
        return {};
    QString current;
    for (const auto &row : m_categories)
        if (row.toMap().value("id") == categoryId)
            current = row.toMap().value("appId").toString();
    return candidateList(category->mimes, current);
}

QVariantList Backend::typeCandidates(const QString &mime) const {
    return candidateList({mime}, m_associations.resolve(mime).appId);
}

bool Backend::setCategory(const QString &categoryId, const QString &appId) {
    const Category *category = Associations::category(categoryId);
    if (!category || !m_associations.apps().contains(appId)) {
        setStatus(tr("That app isn't installed anymore."), true);
        return false;
    }
    const auto changes = m_associations.plan(category->mimes, appId);
    int opens = 0, locked = 0;
    for (const auto &mime : category->mimes) {
        if (m_associations.support(*m_associations.apps().find(appId), mime) > 0)
            ++opens;
        if (m_associations.resolve(mime).locked)
            ++locked;
    }
    const QString name = appName(appId), title = tr(category->title);
    if (changes.isEmpty()) {
        setStatus(locked ? tr("%1 are set by a file Omarchy manages, so they can't be changed here.").arg(title)
                         : tr("%1 already open with %2.").arg(title, name));
        return true;
    }
    const QString message = opens < category->mimes.size()
        ? tr("%1: %2 now opens %3 of %4 types. It can't open the rest.")
              .arg(title, name).arg(opens).arg(category->mimes.size())
        : tr("%1 now open with %2.").arg(title, name);
    return commit(changes, message);
}

bool Backend::setType(const QString &mime, const QString &appId) {
    if (!m_associations.apps().contains(appId)) {
        setStatus(tr("That app isn't installed anymore."), true);
        return false;
    }
    const Resolved current = m_associations.resolve(mime);
    if (current.locked) {
        setStatus(tr("%1 is set in %2, which comes before your own list. Change it there.")
                      .arg(mime, QFileInfo(current.source).fileName()), true);
        return false;
    }
    const auto changes = m_associations.plan({mime}, appId);
    if (changes.isEmpty()) {
        setStatus(tr("%1 already opens with %2.").arg(Associations::describe(mime), appName(appId)));
        return true;
    }
    return commit(changes, tr("%1 now opens with %2.").arg(Associations::describe(mime), appName(appId)));
}

bool Backend::commit(const QList<Change> &changes, const QString &message) {
    QString error;
    m_writing = true;
    const bool ok = m_associations.apply(changes, false, &error);
    m_writing = false;
    rebuild();
    if (!ok) {
        setStatus(error, true);
        return false;
    }
    m_undo << changes;
    m_redo.clear();
    emit historyChanged();
    setStatus(message);
    return true;
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    const auto changes = m_undo.takeLast();
    QString error;
    m_writing = true;
    const bool ok = m_associations.apply(changes, true, &error);
    m_writing = false;
    rebuild();
    if (!ok) {
        m_undo << changes;
        setStatus(error, true);
        return;
    }
    m_redo << changes;
    emit historyChanged();
    setStatus(tr("Undone."));
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    const auto changes = m_redo.takeLast();
    QString error;
    m_writing = true;
    const bool ok = m_associations.apply(changes, false, &error);
    m_writing = false;
    rebuild();
    if (!ok) {
        m_redo << changes;
        setStatus(error, true);
        return;
    }
    m_undo << changes;
    emit historyChanged();
    setStatus(tr("Redone."));
}

void Backend::reload() {
    m_associations.reload();
    rebuild();
    watch();
    setStatus(tr("Reloaded."));
}

void Backend::setStatus(const QString &message, bool error) {
    m_status = message;
    m_statusIsError = error;
    emit statusChanged();
}

bool Backend::omarchyAvailable() const {
    return !QStandardPaths::findExecutable("omarchy").isEmpty();
}

bool Backend::openOmarchyDefaults() {
    if (!QProcess::startDetached("omarchy", {"menu", "summon", "setup.default"})) {
        setStatus(tr("Couldn't open the Omarchy menu."), true);
        return false;
    }
    return true;
}
