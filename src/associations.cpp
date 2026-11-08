#include "associations.h"

#include <QCoreApplication>
#include <QLocale>
#include <QMimeDatabase>
#include <QSet>
#include <algorithm>

namespace {

QStringList canonical(const QStringList &mimes) {
    QStringList out;
    for (const auto &mime : mimes)
        out << canonicalMime(mime);
    out.removeDuplicates();
    return out;
}

}  // namespace

Associations::Associations(const MimePaths &paths) : m_paths(paths) { reload(); }

void Associations::reload() {
    m_files = m_paths.listFiles();
    m_userIndex = m_files.indexOf(m_paths.userFile());
    m_tables.clear();
    m_added.clear();
    m_caches.clear();
    for (const auto &file : m_files) {
        m_tables << MimeAppsFile::readDefaults(file);
        m_added << MimeAppsFile::readGroup(file, "Added Associations");
    }
    for (const auto &directory : m_paths.applicationDirectories())
        m_caches << MimeAppsFile::readGroup(directory + "/mimeinfo.cache", "MIME Cache");
    m_apps.scan(m_paths.applicationDirectories(), DesktopApps::languagesFor(QLocale().name()));
}

const QList<Category> &Associations::categories() {
    // The common types of each kind. Picking an app for a category sets it
    // for the ones it can open; the All file types view reaches the rest.
    static const QList<Category> list = [] {
        QList<Category> raw{
            {"images", QT_TRANSLATE_NOOP("Category", "Images"), "image-x-generic",
             {"image/png", "image/jpeg", "image/gif", "image/webp", "image/bmp", "image/tiff",
              "image/svg+xml", "image/avif", "image/heif", "image/jxl", "image/vnd.microsoft.icon"}},
            {"video", QT_TRANSLATE_NOOP("Category", "Video"), "video-x-generic",
             {"video/mp4", "video/x-matroska", "video/webm", "video/quicktime", "video/x-msvideo",
              "video/mpeg", "video/ogg", "video/x-flv", "video/x-ms-wmv", "video/3gpp", "video/mp2t"}},
            {"music", QT_TRANSLATE_NOOP("Category", "Music"), "audio-x-generic",
             {"audio/mpeg", "audio/flac", "audio/ogg", "audio/x-vorbis+ogg", "audio/x-opus+ogg",
              "audio/vnd.wave", "audio/mp4", "audio/aac", "audio/webm", "audio/x-aiff", "audio/x-ms-wma"}},
            {"pdf", QT_TRANSLATE_NOOP("Category", "PDF documents"), "application-pdf", {"application/pdf"}},
            {"ebooks", QT_TRANSLATE_NOOP("Category", "E-books"), "x-office-document",
             {"application/epub+zip", "application/x-mobipocket-ebook", "application/vnd.amazon.mobi8-ebook",
              "image/vnd.djvu", "application/x-cbz", "application/x-cbr"}},
            {"text", QT_TRANSLATE_NOOP("Category", "Text files"), "text-x-generic",
             {"text/plain", "text/markdown", "application/json", "application/x-yaml", "application/toml",
              "text/csv", "application/x-shellscript", "text/x-log"}},
            {"office", QT_TRANSLATE_NOOP("Category", "Office documents"), "x-office-document",
             {"application/vnd.openxmlformats-officedocument.wordprocessingml.document",
              "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
              "application/vnd.openxmlformats-officedocument.presentationml.presentation",
              "application/vnd.oasis.opendocument.text", "application/vnd.oasis.opendocument.spreadsheet",
              "application/vnd.oasis.opendocument.presentation", "application/msword",
              "application/vnd.ms-excel", "application/vnd.ms-powerpoint", "application/rtf"}},
            {"archives", QT_TRANSLATE_NOOP("Category", "Archives"), "package-x-generic",
             {"application/zip", "application/x-tar", "application/x-compressed-tar", "application/gzip",
              "application/x-7z-compressed", "application/vnd.rar", "application/x-xz-compressed-tar",
              "application/x-xz", "application/x-bzip2-compressed-tar", "application/x-zstd-compressed-tar",
              "application/zstd"}},
            {"folders", QT_TRANSLATE_NOOP("Category", "Folders"), "folder", {"inode/directory"}},
            {"email", QT_TRANSLATE_NOOP("Category", "Email links"), "mail-message-new", {"x-scheme-handler/mailto"}},
        };
        for (auto &category : raw)
            category.mimes = canonical(category.mimes);
        return raw;
    }();
    return list;
}

const Category *Associations::category(const QString &id) {
    for (const auto &category : categories())
        if (category.id == id)
            return &category;
    return nullptr;
}

Resolved Associations::resolve(const QString &mime) const {
    const QString type = canonicalMime(mime);
    // The spec's order: the first file that names an installed app decides;
    // a file naming only uninstalled apps passes the question on.
    for (int i = 0; i < m_tables.size(); ++i) {
        const auto it = m_tables.at(i).constFind(type);
        if (it == m_tables.at(i).constEnd())
            continue;
        for (const auto &id : it.value())
            if (m_apps.contains(id))
                return {id, m_files.at(i), m_userIndex >= 0 && i < m_userIndex, false};
    }
    // No default: xdg-open (through xdg-mime) takes the first installed app
    // from the added associations, then from each mimeinfo.cache.
    auto firstInstalled = [&](const QList<DefaultsTable> &tables, const QStringList &sources) -> Resolved {
        for (int i = 0; i < tables.size(); ++i) {
            const auto it = tables.at(i).constFind(type);
            if (it == tables.at(i).constEnd())
                continue;
            for (const auto &id : it.value())
                if (m_apps.contains(id))
                    return {id, sources.value(i), false, true};
        }
        return {};
    };
    Resolved fallback = firstInstalled(m_added, m_files);
    if (fallback.appId.isEmpty()) {
        QStringList caches;
        for (const auto &directory : m_paths.applicationDirectories())
            caches << directory + "/mimeinfo.cache";
        fallback = firstInstalled(m_caches, caches);
    }
    return fallback;
}

int Associations::support(const DesktopApp &app, const QString &mime) const {
    static const QMimeDatabase database;
    const QString type = canonicalMime(mime);
    const QString group = type.section('/', 0, 0) + "/*";
    for (const auto &declared : app.mimeTypes)
        if (declared == group || canonicalMime(declared) == type)
            return 2;

    // shared-mime-info's parents: an editor for text/plain opens a C file.
    // Everything descends from application/octet-stream, which says nothing.
    const QMimeType info = database.mimeTypeForName(type);
    if (!info.isValid())
        return 0;
    QSet<QString> ancestors;
    for (const auto &parent : info.allAncestors())
        if (parent != QLatin1String("application/octet-stream"))
            ancestors.insert(parent);
    if (ancestors.isEmpty())
        return 0;
    for (const auto &declared : app.mimeTypes)
        if (ancestors.contains(canonicalMime(declared)))
            return 1;
    return 0;
}

QList<Candidate> Associations::candidates(const QStringList &mimes) const {
    QList<Candidate> list;
    for (const auto &app : m_apps.all()) {
        Candidate candidate{&app, 0, 0};
        for (const auto &mime : mimes) {
            const int level = support(app, mime);
            if (level > 0)
                ++candidate.covered;
            if (level == 2)
                ++candidate.direct;
        }
        if (candidate.covered > 0)
            list << candidate;
    }
    std::sort(list.begin(), list.end(), [](const Candidate &a, const Candidate &b) {
        if (a.covered != b.covered) return a.covered > b.covered;
        if (a.direct != b.direct) return a.direct > b.direct;
        // Apps hidden from the launcher are usually helpers; list them last.
        if (a.app->noDisplay != b.app->noDisplay) return !a.app->noDisplay;
        return QString::localeAwareCompare(a.app->name, b.app->name) < 0;
    });
    return list;
}

QStringList Associations::knownTypes() const {
    QSet<QString> types;
    for (const auto &app : m_apps.all())
        for (const auto &declared : app.mimeTypes)
            if (!declared.endsWith("/*") && declared.contains('/'))
                types.insert(canonicalMime(declared));
    if (m_userIndex >= 0)
        for (auto it = m_tables.at(m_userIndex).constBegin(); it != m_tables.at(m_userIndex).constEnd(); ++it)
            types.insert(it.key());
    QStringList list(types.begin(), types.end());
    list.sort();
    return list;
}

QList<Change> Associations::plan(const QStringList &mimes, const QString &appId) const {
    QList<Change> changes;
    const DesktopApp *app = m_apps.find(appId);
    if (!app || m_userIndex < 0)
        return changes;
    for (const auto &mime : canonical(mimes)) {
        if (support(*app, mime) == 0)
            continue;
        const Resolved current = resolve(mime);
        if (current.locked || (current.appId == appId && !current.automatic))
            continue;
        changes << Change{mime, MimeAppsFile::rawValue(m_paths.userFile(), mime), appId};
    }
    return changes;
}

bool Associations::apply(const QList<Change> &changes, bool undo, QString *error) {
    QList<MimeAppsFile::Edit> edits;
    for (const auto &change : changes)
        edits << MimeAppsFile::Edit{change.mime, undo ? change.before : change.after};
    const bool ok = MimeAppsFile::write(m_paths.userFile(), edits, error);
    reload();
    return ok;
}

QString Associations::describe(const QString &mime) {
    static const QMimeDatabase database;
    if (mime.startsWith(QLatin1String("x-scheme-handler/")))
        return QCoreApplication::translate("Category", "%1 links").arg(mime.section('/', 1));
    const QMimeType type = database.mimeTypeForName(mime);
    if (type.isValid() && !type.comment().isEmpty())
        return type.comment();
    return mime;
}

QStringList Associations::patterns(const QString &mime) {
    static const QMimeDatabase database;
    const QMimeType type = database.mimeTypeForName(mime);
    return type.isValid() ? type.globPatterns() : QStringList();
}
