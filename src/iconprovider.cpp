#include "iconprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) {
    const QSize wanted = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(48, 48);
    const QString name = QUrl::fromPercentEncoding(id.toUtf8());

    QIcon icon;
    if (QDir::isAbsolutePath(name) && QFileInfo::exists(name))
        icon = QIcon(name);
    else if (!name.isEmpty())
        icon = QIcon::fromTheme(name);
    if (icon.isNull())
        icon = QIcon::fromTheme("application-x-executable");

    QPixmap pixmap = icon.isNull() ? QPixmap() : icon.pixmap(wanted);
    if (pixmap.isNull()) {
        pixmap = QPixmap(wanted);
        pixmap.fill(Qt::transparent);
    }
    if (size)
        *size = pixmap.size();
    return pixmap;
}

void IconProvider::configureTheme() {
    QIcon::setFallbackThemeName("hicolor");
    QStringList paths = QIcon::fallbackSearchPaths();
    paths << "/usr/share/pixmaps";
    QIcon::setFallbackSearchPaths(paths);

    const QString current = QIcon::themeName();
    if (!current.isEmpty() && current != QLatin1String("hicolor"))
        return;
    const QString gsettings = QStandardPaths::findExecutable("gsettings");
    if (gsettings.isEmpty())
        return;
    QProcess process;
    process.start(gsettings, {"get", "org.gnome.desktop.interface", "icon-theme"});
    if (!process.waitForFinished(1000) || process.exitCode() != 0) {
        process.kill();
        return;
    }
    QString theme = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (theme.size() >= 2 && (theme.startsWith('\'') || theme.startsWith('"')))
        theme = theme.mid(1, theme.size() - 2);
    if (!theme.isEmpty())
        QIcon::setThemeName(theme);
}
