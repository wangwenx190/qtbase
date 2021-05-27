/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2021 Yuhang Zhao, 2546789017@qq.com
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwindowchrome_p.h"
#include "qwindow.h"

QT_BEGIN_NAMESPACE

static const char _qt_native_parent_flag[] = "_q_windowChrome_nativeParent";
static const char _qt_unix_code_path_flag[] = "_q_windowChrome_unixCodePath";

bool WindowChrome::useUnixCodePath()
{
#ifdef Q_OS_WINDOWS
    return qEnvironmentVariableIsSet(_qt_unix_code_path_flag);
#else
    return true;
#endif
}

QPointF WindowChrome::mapOriginPointToWindow(const QObject *object)
{
    Q_ASSERT(object);
    if (!object)
        return {};
    if (!object->isWidgetType() && !object->inherits("QQuickItem")) {
        qWarning() << object << "is not a QWidget or QQuickItem.";
        return {};
    }
    QPointF point = {object->property("x").toReal(), object->property("y").toReal()};
    for (QObject *parent = object->parent(); parent; parent = parent->parent()) {
        point += {parent->property("x").toReal(), parent->property("y").toReal()};
        if (parent->isWindowType())
            break;
    }
    return point;
}

bool WindowChrome::isHitTestVisibleInChrome(const QWindow *window)
{
    Q_ASSERT(window);
    if (!window)
        return false;
    const auto nativeParent = qvariant_cast<const QObject *>(window->property(_qt_native_parent_flag));
    const QObjectList objs = nativeParent ? nativeParent->findChildren<QObject *>() : window->findChildren<QObject *>();
    if (objs.isEmpty())
        return false;
    for (auto &&obj : qAsConst(objs)) {
        if (!obj || !(obj->isWidgetType() || obj->inherits("QQuickItem")))
            continue;
        if (!obj->property("hitTestVisibleInChrome").toBool() || !obj->property("visible").toBool())
            continue;
        const QPointF originPoint = mapOriginPointToWindow(obj);
        const qreal width = obj->property("width").toReal();
        const qreal height = obj->property("height").toReal();
        const QRectF rect = {originPoint.x(), originPoint.y(), width, height};
        if (rect.contains(QCursor::pos(window->screen())))
            return true;
    }
    return false;
}

bool WindowChrome::isWindowFixedSize(const QWindow *window)
{
    Q_ASSERT(window);
    if (!window)
        return false;
#ifdef Q_OS_WINDOWS
    if (window->flags().testFlag(Qt::MSWindowsFixedSizeDialogHint))
        return true;
#endif
    const QSize minSize = window->minimumSize();
    const QSize maxSize = window->maximumSize();
    if (!minSize.isEmpty() && !maxSize.isEmpty() && (minSize == maxSize))
        return true;
    return false;
}

void WindowChrome::setHitTestNativeParent(QWindow *window, const QObject *parent)
{
    Q_ASSERT(window);
    Q_ASSERT(parent);
    if (!window || !parent)
        return;
    window->setProperty(_qt_native_parent_flag, QVariant::fromValue(parent));
}

static inline Qt::Edges getWindowEdges(const QWindow *window, const QPointF &point)
{
    Q_ASSERT(window);
    if (!window)
        return {};
    const int resizeBorderWidth = window->resizeBorderThickness();
    const int windowWidth = window->width();
    const int windowHeight = window->height();
    if (point.y() <= resizeBorderWidth) {
        if (point.x() <= resizeBorderWidth)
            return Qt::Edge::TopEdge | Qt::Edge::LeftEdge;
        if (point.x() >= (windowWidth - resizeBorderWidth))
            return Qt::Edge::TopEdge | Qt::Edge::RightEdge;
        return Qt::Edge::TopEdge;
    }
    if (point.y() >= (windowHeight - resizeBorderWidth)) {
        if (point.x() <= resizeBorderWidth)
            return Qt::Edge::BottomEdge | Qt::Edge::LeftEdge;
        if (point.x() >= (windowWidth - resizeBorderWidth))
            return Qt::Edge::BottomEdge | Qt::Edge::RightEdge;
        return Qt::Edge::BottomEdge;
    }
    if (point.x() <= resizeBorderWidth)
        return Qt::Edge::LeftEdge;
    if (point.x() >= (windowWidth - resizeBorderWidth))
        return Qt::Edge::RightEdge;
    return {};
}

static inline Qt::CursorShape getCursorShape(const Qt::Edges edges)
{
    if ((edges.testFlag(Qt::Edge::TopEdge) && edges.testFlag(Qt::Edge::LeftEdge))
            || (edges.testFlag(Qt::Edge::BottomEdge) && edges.testFlag(Qt::Edge::RightEdge)))
        return Qt::CursorShape::SizeFDiagCursor;
    if ((edges.testFlag(Qt::Edge::TopEdge) && edges.testFlag(Qt::Edge::RightEdge))
            || (edges.testFlag(Qt::Edge::BottomEdge) && edges.testFlag(Qt::Edge::LeftEdge)))
        return Qt::CursorShape::SizeBDiagCursor;
    if (edges.testFlag(Qt::Edge::TopEdge) || edges.testFlag(Qt::Edge::BottomEdge))
        return Qt::CursorShape::SizeVerCursor;
    if (edges.testFlag(Qt::Edge::LeftEdge) || edges.testFlag(Qt::Edge::RightEdge))
        return Qt::CursorShape::SizeHorCursor;
    return Qt::CursorShape::ArrowCursor;
}

static inline bool isInTitleBarArea(const QWindow *window, const QPointF &point)
{
    Q_ASSERT(window);
    if (!window)
        return false;
    return (point.y() <= window->titleBarHeight()) && !WindowChrome::isHitTestVisibleInChrome(window);
}

static inline void moveOrResize(QWindow *window, const QPointF &point)
{
    Q_ASSERT(window);
    if (!window) {
        return;
    }
    const Qt::Edges edges = getWindowEdges(window, point);
    if (edges == Qt::Edges{}) {
        if (isInTitleBarArea(window, point)) {
            if (!window->startSystemMove())
                qWarning("Current OS doesn't support QWindow::startSystemMove().");
        }
    } else {
        if ((window->windowState() == Qt::WindowState::WindowNoState)
                && !WindowChrome::isHitTestVisibleInChrome(window) && !WindowChrome::isWindowFixedSize(window)) {
            if (!window->startSystemResize(edges))
                qWarning("Current OS doesn't support QWindow::startSystemResize().");
        }
    }
}

/*!
    Override this to handle mouse press events (\a ev).

    \sa mouseReleaseEvent()
*/
void QWindow::mousePressEvent(QMouseEvent *ev)
{
    if (!(flags() & Qt::FramelessWindowHint) || !WindowChrome::useUnixCodePath()) {
        ev->ignore();
        return;
    }
    if (ev->button() != Qt::MouseButton::LeftButton)
        return;
    moveOrResize(this, ev->scenePosition());
}

/*!
    Override this to handle mouse double click events (\a ev).

    \sa mousePressEvent(), QStyleHints::mouseDoubleClickInterval()
*/
void QWindow::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if (!(flags() & Qt::FramelessWindowHint) || !WindowChrome::useUnixCodePath()) {
        ev->ignore();
        return;
    }
    if (ev->button() != Qt::MouseButton::LeftButton)
        return;
    if (isInTitleBarArea(this, ev->scenePosition())) {
        if (windowState() == Qt::WindowState::WindowFullScreen)
            return;
        if (windowState() == Qt::WindowState::WindowMaximized)
            showNormal();
        else
            showMaximized();
        setCursor(Qt::CursorShape::ArrowCursor);
    }
}

/*!
    Override this to handle mouse move events (\a ev).
*/
void QWindow::mouseMoveEvent(QMouseEvent *ev)
{
    if (!(flags() & Qt::FramelessWindowHint) || !WindowChrome::useUnixCodePath()) {
        ev->ignore();
        return;
    }
    if ((windowState() == Qt::WindowState::WindowNoState) && !WindowChrome::isWindowFixedSize(this))
        setCursor(getCursorShape(getWindowEdges(this, ev->scenePosition())));
}

QT_END_NAMESPACE
