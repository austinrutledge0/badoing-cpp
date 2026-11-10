// SPDX-License-Identifier: MIT
#include "petwindow.h"
#include "petlogic.h"
#include "loginstartup.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QImageWriter>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QRegion>
#include <QSaveFile>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTransform>
#include <QUrl>
#include <QWindow>
#if BADOING_HAS_MULTIMEDIA
#include <QSoundEffect>
#endif
#include <stdexcept>
#include <utility>

namespace {
QPoint globalMousePosition(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

badoing::Rect toRect(const QRect& rect)
{
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}
}

std::array<QImage, 10> PetWindow::loadFrames()
{
    const QImage sheet(QStringLiteral(":/assets/frames.png"));
    if (sheet.isNull() || sheet.size() != QSize(badoing::frameWidth * badoing::frameCount,
                                              badoing::frameHeight))
        throw std::runtime_error("The embedded animation resource is missing or invalid.");
    std::array<QImage, 10> frames;
    for (int i = 0; i < badoing::frameCount; ++i) {
        frames[static_cast<std::size_t>(i)] = sheet.copy(
            i * badoing::frameWidth + badoing::cropX, badoing::cropY,
            badoing::cropWidth, badoing::cropHeight);
    }
    return frames;
}

QPainterPath PetWindow::makeOutline()
{
    // Original geometric outline, expressed in source-sheet coordinates.
    const QPoint points[] = {
        {33,174},{41,172},{48,172},{57,174},{69,174},{80,171},
        {91,169},{112,170},{131,170},{135,167},{144,164},{153,159},
        {149,154},{139,147},{128,140},{116,135},{100,131},
        {99,128},{94,124},{107,127},{122,128},{140,138},{157,148},
        {165,155},{174,146},{179,141},{176,133},{175,126},{178,118},
        {186,112},{194,107},{201,108},{210,111},{219,115},{229,122},
        {237,130},{249,135},{261,142},{270,150},{277,158},{278,166},
        {294,171},{309,176},{324,184},{339,192},{348,198},
        {62,198},{63,194},{61,190},{54,188},{59,185},{60,183},
        {57,180},{51,177},{43,175}
    };
    QPolygonF polygon;
    for (const QPoint& point : points)
        polygon << QPointF(point.x() - badoing::cropX, point.y() - badoing::cropY);
    QPainterPath path;
    path.addPolygon(polygon);
    path.closeSubpath();
    return path;
}

PetWindow::PetWindow(SettingsStore& store, PetSettings settings, QStringList loginArguments)
    : store_(store), state_(std::move(settings)), loginArguments_(std::move(loginArguments)),
      frames_(loadFrames()), outline_(makeOutline())
{
    positionsSupported_ = !QGuiApplication::platformName().startsWith(QStringLiteral("wayland"));
    setWindowTitle(QStringLiteral("BADOING desktop pet"));
    setObjectName(QStringLiteral("BADOINGPet"));
    setAccessibleName(QStringLiteral("BADOING desktop pet"));
    setAccessibleDescription(QStringLiteral("Click for badoing. Drag to move. Right-click for options and quit."));
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus;
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    flags |= Qt::Tool;
#else
    flags |= Qt::Window;
#endif
    setWindowFlags(flags);
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true); // Receive no-button moves so stale drag state can be cancelled.
    // QtGui always supports the embedded PNG; no QtSvg plugin is needed for the tray.
    QImage iconImage(badoing::cropWidth, badoing::cropHeight, QImage::Format_ARGB32_Premultiplied);
    iconImage.fill(Qt::transparent);
    {
        QPainter painter(&iconImage);
        painter.setClipPath(outline_);
        painter.drawImage(QPoint(0,0), frames_[0]);
    }
    setWindowIcon(QIcon(QPixmap::fromImage(iconImage)));
    setFocusPolicy(Qt::NoFocus);
    applySize();
    makeMenu();

    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(350);
    connect(&saveTimer_, &QTimer::timeout, this, &PetWindow::flushSettings);
    animationTimer_.setTimerType(Qt::PreciseTimer);
    animationTimer_.setInterval(15);
    connect(&animationTimer_, &QTimer::timeout, this, &PetWindow::advanceFrame);
    screenTimer_.setInterval(1500);
    connect(&screenTimer_, &QTimer::timeout, this, &PetWindow::checkScreens);

#if BADOING_HAS_MULTIMEDIA
    sound_ = new QSoundEffect(this);
    sound_->setLoopCount(1);
    sound_->setVolume(0.8f);
    sound_->setMuted(state_.muted);
    connect(sound_, &QSoundEffect::statusChanged, this, [this] {
        if (sound_->status() == QSoundEffect::Error)
            qWarning() << "BADOING: audio is unavailable; the animation will still work.";
    });
    sound_->setSource(QUrl(QStringLiteral("qrc:/assets/badoing.wav")));
#endif

    if (positionsSupported_) {
        QScreen* preferred = nullptr;
        for (QScreen* screen : QGuiApplication::screens()) {
            if (screen->name() == state_.screenName) preferred = screen;
        }
        if (state_.hasPosition) move(state_.position);
        QScreen* screen = preferred ? preferred : currentScreen();
        if (state_.docked || !state_.hasPosition) sitOnPanel(screen);
        else clampToScreen(screen);
    }
    // On native Wayland, preserve the last X11 position/docking preference;
    // XDG shell supplies no meaningful global position to replace it with.
    ready_ = true;
    queueSave();
    refreshLabels();
    screenTimer_.start();
}

PetWindow::~PetWindow()
{
    flushSettings();
}

void PetWindow::makeMenu()
{
    menu_ = new QMenu(this);
    countAction_ = menu_->addAction(QString());
    countAction_->setEnabled(false);
    saveStatusAction_ = menu_->addAction(QStringLiteral("Settings could not be saved"));
    saveStatusAction_->setEnabled(false);
    saveStatusAction_->setVisible(false);
    menu_->addSeparator();
    muteAction_ = menu_->addAction(QStringLiteral("Mute"));
    muteAction_->setCheckable(true);
    muteAction_->setChecked(state_.muted);
#if BADOING_HAS_MULTIMEDIA
    connect(muteAction_, &QAction::toggled, this, [this](bool muted) {
        state_.muted = muted;
        sound_->setMuted(muted);
        if (muted) sound_->stop();
        queueSave();
    });
#else
    muteAction_->setText(QStringLiteral("Sound unavailable in this build"));
    muteAction_->setEnabled(false);
#endif
    loginAction_ = menu_->addAction(QStringLiteral("Start at login"));
    loginAction_->setCheckable(true);
    loginAction_->setChecked(LoginStartup::isEnabled());
    connect(menu_, &QMenu::aboutToShow, this, [this] {
        loginAction_->setChecked(LoginStartup::isEnabled());
    });
    connect(loginAction_, &QAction::triggered, this, [this](bool enabled) {
        QString error;
        if (!LoginStartup::setEnabled(enabled, loginArguments_, &error))
            QMessageBox::warning(this, QStringLiteral("BADOING login startup"), error);
        loginAction_->setChecked(LoginStartup::isEnabled());
    });
    auto* sizeMenu = menu_->addMenu(QStringLiteral("Size"));
    auto* sizes = new QActionGroup(this);
    const std::pair<const char*, int> choices[] = {{"Small",260}, {"Normal",355}, {"Large",470}};
    for (const auto& choice : choices) {
        auto* action = sizeMenu->addAction(QString::fromLatin1(choice.first));
        action->setCheckable(true);
        action->setChecked(state_.width == choice.second);
        sizes->addAction(action);
        const int width = choice.second;
        connect(action, &QAction::triggered, this, [this, width] {
            state_.width = width;
            applySize();
            if (positionsSupported_) {
                if (state_.docked) sitOnPanel(currentScreen());
                else clampToScreen(currentScreen());
            }
            queueSave();
        });
    }
    auto* dock = menu_->addAction(QStringLiteral("Sit near panel"));
    dock->setEnabled(positionsSupported_);
    if (!positionsSupported_)
        dock->setToolTip(QStringLiteral("Native Wayland controls window placement. Use XWayland for panel docking."));
    connect(dock, &QAction::triggered, this, [this] { sitOnPanel(currentScreen()); queueSave(); });
    menu_->addSeparator();
    auto* quit = menu_->addAction(QStringLiteral("Quit"));
    connect(quit, &QAction::triggered, this, &QWidget::close);

    tray_ = new QSystemTrayIcon(windowIcon(), this);
    tray_->setContextMenu(menu_);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            showNormal();
            if (positionsSupported_) sitOnPanel(currentScreen());
            queueSave();
        }
    });
    if (QSystemTrayIcon::isSystemTrayAvailable()) tray_->show();
}

void PetWindow::applySize()
{
    state_.width = badoing::normalizedWidth(state_.width);
    setFixedSize(state_.width, badoing::heightForWidth(state_.width));
    QTransform transform;
    transform.scale(width() / double(badoing::cropWidth), height() / double(badoing::cropHeight));
    if (QGuiApplication::platformName() != QStringLiteral("offscreen"))
        setMask(QRegion(transform.map(outline_).toFillPolygon().toPolygon()));
    update();
}

void PetWindow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.scale(width() / double(badoing::cropWidth), height() / double(badoing::cropHeight));
    painter.setClipPath(outline_);
    painter.drawImage(QPoint(0,0), frames_[static_cast<std::size_t>(frame_)]);
}

void PetWindow::refreshLabels()
{
    const QString label = QLocale().toString(state_.count) + QStringLiteral(" badoings");
    countAction_->setText(label);
    setToolTip(label);
    tray_->setToolTip(QStringLiteral("BADOING — ") + label);
}

void PetWindow::badoing()
{
    state_.count = badoing::incrementCount(state_.count);
    refreshLabels();
    queueSave();
    frame_ = 0;
    clock_.restart();
    animationTimer_.start();
    update();
#if BADOING_HAS_MULTIMEDIA
    if (!state_.muted) { sound_->stop(); sound_->play(); }
#endif
}

void PetWindow::advanceFrame()
{
    const auto elapsed = clock_.elapsed();
    const int next = badoing::frameAtElapsed(elapsed);
    if (elapsed >= badoing::animationDuration) animationTimer_.stop();
    if (next != frame_) { frame_ = next; update(); }
}

void PetWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }
    pointerDown_ = true;
    dragging_ = false;
    pointerOrigin_ = globalMousePosition(event);
    windowOrigin_ = pos();
    event->accept();
}

void PetWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (!pointerDown_) { QWidget::mouseMoveEvent(event); return; }
    if (!(event->buttons() & Qt::LeftButton)) { resetPointer(); return; }
    const QPoint delta = globalMousePosition(event) - pointerOrigin_;
    if (!dragging_ && delta.manhattanLength() < QApplication::startDragDistance()) return;
    dragging_ = true;
    if (positionsSupported_) {
        state_.docked = false;
        move(windowOrigin_ + delta);
    } else {
        // Hand the trusted pointer gesture to the Wayland compositor.
        const bool started = windowHandle() && windowHandle()->startSystemMove();
        resetPointer(); // The compositor can consume the corresponding release.
        if (!started && !warnedDragFailure_) {
            qWarning() << "BADOING: this compositor did not start a window move. Try XWayland.";
            warnedDragFailure_ = true;
        }
    }
    event->accept();
}

void PetWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !pointerDown_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const bool wasDragging = dragging_ || (globalMousePosition(event) - pointerOrigin_).manhattanLength()
                                      >= QApplication::startDragDistance();
    resetPointer();
    if (wasDragging) {
        if (positionsSupported_) {
            QScreen* screen = currentScreen();
            clampToScreen(screen);
            if (screen && badoing::nearBottom({x(),y()}, height(), toRect(screen->availableGeometry())))
                sitOnPanel(screen);
        }
        queueSave();
    } else {
        badoing();
    }
    event->accept();
}

void PetWindow::resetPointer()
{
    pointerDown_ = false;
    dragging_ = false;
}

bool PetWindow::event(QEvent* event)
{
    if (event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate
        || event->type() == QEvent::Hide)
        resetPointer();
    return QWidget::event(event);
}

void PetWindow::contextMenuEvent(QContextMenuEvent* event)
{
    resetPointer();
    menu_->exec(event->globalPos());
    event->accept();
}

QScreen* PetWindow::currentScreen() const
{
    if (!positionsSupported_ && windowHandle() && windowHandle()->screen())
        return windowHandle()->screen();
    if (QScreen* screen = QGuiApplication::screenAt(frameGeometry().center())) return screen;
    QScreen* best = nullptr;
    qint64 largest = 0;
    for (QScreen* screen : QGuiApplication::screens()) {
        const QRect intersect = screen->availableGeometry().intersected(frameGeometry());
        const qint64 area = qint64(intersect.width()) * intersect.height();
        if (area > largest) { largest = area; best = screen; }
    }
    return best ? best : QGuiApplication::primaryScreen();
}

void PetWindow::clampToScreen(QScreen* screen)
{
    if (!positionsSupported_ || !screen) return;
    const QRect available = screen->availableGeometry();
    const auto bounded = badoing::clampPosition({x(),y()}, width(), height(), toRect(available));
    move(bounded.x, bounded.y);
    lastAvailable_ = available;
}

void PetWindow::sitOnPanel(QScreen* screen)
{
    if (!positionsSupported_ || !screen) return;
    const QRect available = screen->availableGeometry();
    const auto docked = badoing::dockPosition({x(),y()}, width(), height(),
                                             toRect(available), state_.hasPosition);
    move(docked.x, docked.y);
    state_.hasPosition = true;
    state_.docked = true;
    lastAvailable_ = available;
}

void PetWindow::rememberPosition()
{
    if (!positionsSupported_) return;
    state_.position = pos();
    state_.hasPosition = true;
    if (QScreen* screen = currentScreen()) state_.screenName = screen->name();
}

void PetWindow::checkScreens()
{
    if (!positionsSupported_ || pointerDown_) return;
    QScreen* screen = currentScreen();
    if (screen && screen->availableGeometry() != lastAvailable_) {
        if (state_.docked) sitOnPanel(screen); else clampToScreen(screen);
        queueSave();
    }
}

void PetWindow::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    if (ready_ && positionsSupported_) { rememberPosition(); queueSave(); }
}

void PetWindow::queueSave()
{
    if (!ready_) return;
    rememberPosition();
    dirty_ = true;
    saveTimer_.start();
}

void PetWindow::flushSettings()
{
    saveTimer_.stop();
    if (!dirty_) return;
    rememberPosition();
    QString error;
    if (store_.save(state_, &error)) {
        dirty_ = false;
        warnedSaveFailure_ = false;
        saveStatusAction_->setVisible(false);
        return;
    }
    saveStatusAction_->setVisible(true);
    if (!warnedSaveFailure_) {
        qWarning().noquote() << "BADOING:" << error;
        if (tray_->isVisible())
            tray_->showMessage(QStringLiteral("BADOING settings"), error, QSystemTrayIcon::Warning);
        warnedSaveFailure_ = true;
    }
}

void PetWindow::closeEvent(QCloseEvent* event)
{
    resetPointer();
    // Also persist an initial --mute or a newly selected default position.
    dirty_ = true;
    flushSettings();
    if (tray_) tray_->hide();
    event->accept();
    QApplication::quit();
}

bool PetWindow::renderPreview(const QString& path, QString* error)
{
    const auto frames = loadFrames();
    QImage preview(badoing::cropWidth, badoing::cropHeight, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::transparent);
    {
        QPainter painter(&preview);
        painter.setClipPath(makeOutline());
        painter.drawImage(QPoint(0,0), frames[0]);
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) *error = output.errorString();
        return false;
    }
    QImageWriter writer(&output, "png");
    if (!writer.write(preview)) {
        if (error) *error = writer.errorString();
        return false;
    }
    if (!output.commit()) {
        if (error) *error = output.errorString();
        return false;
    }
    return true;
}
