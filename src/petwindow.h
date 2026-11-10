// SPDX-License-Identifier: MIT
#pragma once

#include "settings.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPainterPath>
#include <QRect>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <array>

class QAction;
class QCloseEvent;
class QContextMenuEvent;
class QMenu;
class QMouseEvent;
class QMoveEvent;
class QPaintEvent;
class QScreen;
class QSystemTrayIcon;
#if BADOING_HAS_MULTIMEDIA
class QSoundEffect;
#endif

class PetWindow final : public QWidget {
public:
    explicit PetWindow(SettingsStore& store, PetSettings settings, QStringList loginArguments = {});
    ~PetWindow() override;
    static bool renderPreview(const QString& path, QString* error);
    void flushSettings();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    static std::array<QImage, 10> loadFrames();
    static QPainterPath makeOutline();
    void applySize();
    void makeMenu();
    void refreshLabels();
    void badoing();
    void advanceFrame();
    void resetPointer();
    void queueSave();
    void rememberPosition();
    void clampToScreen(QScreen* screen);
    void sitOnPanel(QScreen* screen);
    QScreen* currentScreen() const;
    void checkScreens();

    SettingsStore& store_;
    PetSettings state_;
    QStringList loginArguments_;
    std::array<QImage, 10> frames_;
    QPainterPath outline_;
    QTimer animationTimer_;
    QTimer screenTimer_;
    QTimer saveTimer_;
    QElapsedTimer clock_;
    QMenu* menu_ = nullptr;
    QAction* countAction_ = nullptr;
    QAction* muteAction_ = nullptr;
    QAction* loginAction_ = nullptr;
    QAction* saveStatusAction_ = nullptr;
    QSystemTrayIcon* tray_ = nullptr;
#if BADOING_HAS_MULTIMEDIA
    QSoundEffect* sound_ = nullptr;
#endif
    bool positionsSupported_ = true;
    bool pointerDown_ = false;
    bool dragging_ = false;
    bool ready_ = false;
    bool dirty_ = false;
    bool warnedSaveFailure_ = false;
    bool warnedDragFailure_ = false;
    QPoint pointerOrigin_;
    QPoint windowOrigin_;
    QRect lastAvailable_;
    int frame_ = 0;
};
