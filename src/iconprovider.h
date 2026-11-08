#pragma once

#include <QQuickImageProvider>

// Serves "image://icon/<name>" from the desktop's icon theme, so QML can show
// the same app icons the launcher does. A name that is an absolute path
// loads that file.
class IconProvider : public QQuickImageProvider {
public:
    IconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

    // Picks the icon theme the desktop uses (GTK's setting on Omarchy), since
    // Qt has no platform theme to ask under Hyprland.
    static void configureTheme();
};
