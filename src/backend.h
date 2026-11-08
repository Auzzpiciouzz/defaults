#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "associations.h"

// The window's view of Associations: lists QML can show, actions it can
// call, and an undo history of what was written.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList categories READ categories NOTIFY modelChanged)
    Q_PROPERTY(QVariantList types READ types NOTIFY modelChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString userFile READ userFile CONSTANT)

public:
    explicit Backend(const MimePaths &paths = MimePaths::fromEnvironment(), QObject *parent = nullptr);

    QVariantList categories() const { return m_categories; }
    QVariantList types() const { return m_types; }
    QString status() const { return m_status; }
    bool statusIsError() const { return m_statusIsError; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString userFile() const { return m_associations.paths().userFile(); }

    Q_INVOKABLE QVariantList categoryCandidates(const QString &categoryId) const;
    Q_INVOKABLE QVariantList typeCandidates(const QString &mime) const;
    Q_INVOKABLE bool setCategory(const QString &categoryId, const QString &appId);
    Q_INVOKABLE bool setType(const QString &mime, const QString &appId);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void reload();
    // Browser, editor and terminal live in Omarchy's own menu; open it there.
    Q_INVOKABLE bool openOmarchyDefaults();
    Q_INVOKABLE bool omarchyAvailable() const;

signals:
    void modelChanged();
    void statusChanged();
    void historyChanged();

private:
    void rebuild();
    void watch();
    bool commit(const QList<Change> &changes, const QString &message);
    void setStatus(const QString &message, bool error = false);
    QVariantMap appMap(const DesktopApp *app) const;
    QVariantList candidateList(const QStringList &mimes, const QString &current) const;
    QString appName(const QString &id) const;

    Associations m_associations;
    QVariantList m_categories;
    QVariantList m_types;
    QString m_status;
    bool m_statusIsError = false;
    QList<QList<Change>> m_undo, m_redo;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    bool m_writing = false;
};
