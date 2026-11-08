#include "cli.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "associations.h"

namespace {

QTextStream &out() { static QTextStream stream(stdout); return stream; }
QTextStream &err() { static QTextStream stream(stderr); return stream; }

int fail(const QString &message) {
    err() << "defaults: " << message << Qt::endl;
    return 1;
}

void print(const QJsonValue &value) {
    out() << (value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject()))
                 .toJson(QJsonDocument::Indented);
    out().flush();
}

QString appName(const Associations &associations, const QString &id) {
    const DesktopApp *app = associations.apps().find(id);
    return app ? app->name : QString();
}

// A category id or a MIME type, as the types it stands for.
bool target(const QString &what, QStringList *mimes, QString *label) {
    if (const Category *category = Associations::category(what)) {
        *mimes = category->mimes;
        *label = category->id;
        return true;
    }
    if (what.contains('/')) {
        *mimes = {canonicalMime(what)};
        *label = mimes->first();
        return true;
    }
    return false;
}

QJsonObject typeJson(const Associations &associations, const QString &mime) {
    const Resolved resolved = associations.resolve(mime);
    return {{"mime", mime},
            {"description", Associations::describe(mime)},
            {"app", resolved.appId.isEmpty() ? QJsonValue() : QJsonValue(resolved.appId)},
            {"appName", resolved.appId.isEmpty() ? QJsonValue() : QJsonValue(appName(associations, resolved.appId))},
            {"source", resolved.source.isEmpty() ? QJsonValue() : QJsonValue(resolved.source)},
            {"locked", resolved.locked},
            {"automatic", resolved.automatic}};
}

QString line(const Associations &associations, const QString &mime) {
    const Resolved resolved = associations.resolve(mime);
    QString text = mime + "  " + (resolved.appId.isEmpty() ? QStringLiteral("(not set)") : resolved.appId);
    if (resolved.locked)
        text += "  (locked by " + resolved.source + ")";
    else if (resolved.automatic)
        text += "  (automatic)";
    return text;
}

}  // namespace

QString Cli::usage() {
    return QStringLiteral(
        "Usage: defaults [open]                 open the window\n"
        "       defaults list [--json]          each category and the app that opens it\n"
        "       defaults types [--json]         every known file type and its app\n"
        "       defaults get <what> [--json]    what opens a category or type now\n"
        "       defaults apps <what> [--json]   installed apps that can open it\n"
        "       defaults set <what> <app> [--json]\n"
        "                                       make <app> (a desktop id such as imv.desktop)\n"
        "                                       open it; only types the app supports change\n"
        "\n"
        "<what> is a category (images, video, music, pdf, ebooks, text, office,\n"
        "archives, folders, email) or a MIME type such as image/png.\n"
        "Changes go to ~/.config/mimeapps.list.\n");
}

bool Cli::handles(const QStringList &arguments) {
    if (arguments.size() < 2)
        return false;
    const QString command = arguments.at(1);
    return command != QLatin1String("open");
}

int Cli::run(const QStringList &arguments) {
    QStringList args = arguments.mid(1);
    const bool json = args.removeAll("--json") > 0;
    const QString command = args.value(0);

    if (command == "-h" || command == "--help" || command == "help") {
        out() << usage();
        return 0;
    }
    if (command == "-v" || command == "--version") {
        out() << "defaults 0.1.0" << Qt::endl;
        return 0;
    }

    Associations associations;

    if (command == "list" && args.size() == 1) {
        QJsonArray rows;
        for (const auto &category : Associations::categories()) {
            QJsonArray types;
            for (const auto &mime : category.mimes)
                types << typeJson(associations, mime);
            rows << QJsonObject{{"id", category.id}, {"title", category.title}, {"types", types}};
            if (!json) {
                out() << category.id << Qt::endl;
                for (const auto &mime : category.mimes)
                    out() << "  " << line(associations, mime) << Qt::endl;
            }
        }
        if (json)
            print(rows);
        return 0;
    }

    if (command == "types" && args.size() == 1) {
        QJsonArray rows;
        for (const auto &mime : associations.knownTypes()) {
            if (json)
                rows << typeJson(associations, mime);
            else
                out() << line(associations, mime) << Qt::endl;
        }
        if (json)
            print(rows);
        return 0;
    }

    if ((command == "get" || command == "apps") && args.size() == 2) {
        QStringList mimes;
        QString label;
        if (!target(args.at(1), &mimes, &label))
            return fail(QStringLiteral("unknown category or MIME type: %1").arg(args.at(1)));
        if (command == "get") {
            QJsonArray rows;
            for (const auto &mime : mimes) {
                if (json)
                    rows << typeJson(associations, mime);
                else
                    out() << line(associations, mime) << Qt::endl;
            }
            if (json)
                print(rows);
            return 0;
        }
        QJsonArray rows;
        for (const auto &candidate : associations.candidates(mimes)) {
            if (json)
                rows << QJsonObject{{"app", candidate.app->id}, {"name", candidate.app->name},
                                    {"covers", candidate.covered}, {"of", int(mimes.size())}};
            else
                out() << candidate.app->id << "  " << candidate.app->name << "  (" << candidate.covered
                      << "/" << mimes.size() << ")" << Qt::endl;
        }
        if (json)
            print(rows);
        return 0;
    }

    if (command == "set" && args.size() == 3) {
        QStringList mimes;
        QString label;
        if (!target(args.at(1), &mimes, &label))
            return fail(QStringLiteral("unknown category or MIME type: %1").arg(args.at(1)));
        QString appId = args.at(2);
        if (!appId.endsWith(".desktop"))
            appId += ".desktop";
        const DesktopApp *app = associations.apps().find(appId);
        if (!app)
            return fail(QStringLiteral("no installed app with the desktop id %1 (see: defaults apps %2)")
                            .arg(appId, args.at(1)));
        int supported = 0;
        QStringList locked;
        for (const auto &mime : mimes) {
            if (associations.support(*app, mime) > 0)
                ++supported;
            if (associations.resolve(mime).locked)
                locked << mime;
        }
        if (supported == 0)
            return fail(QStringLiteral("%1 can't open %2").arg(appId, label));
        if (mimes.size() == 1 && !locked.isEmpty())
            return fail(QStringLiteral("%1 is set by %2, which is read before ~/.config/mimeapps.list")
                            .arg(label, associations.resolve(label).source));

        const auto changes = associations.plan(mimes, appId);
        QString error;
        if (!changes.isEmpty() && !associations.apply(changes, false, &error))
            return fail(error);
        QJsonArray changed;
        for (const auto &change : changes)
            changed << change.mime;
        if (json) {
            print(QJsonObject{{"app", appId}, {"target", label}, {"changed", changed},
                              {"supported", supported}, {"of", int(mimes.size())},
                              {"locked", QJsonArray::fromStringList(locked)}});
        } else {
            out() << appId << " opens " << supported << " of " << mimes.size() << " types in " << label
                  << "; changed " << changes.size() << Qt::endl;
            if (!locked.isEmpty())
                out() << "locked, left alone: " << locked.join(", ") << Qt::endl;
        }
        return 0;
    }

    err() << usage();
    return 1;
}
