// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include <qimageiohandler.h>
#include <qstringlist.h>

#ifdef QT_NO_IMAGEFORMAT_JPEG
#undef QT_NO_IMAGEFORMAT_JPEG
#endif
#include <qjpeghandler_p.h>

QT_BEGIN_NAMESPACE

class VantariftJpegPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "jpeg.json")
public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

QImageIOPlugin::Capabilities VantariftJpegPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    Q_UNUSED(device);
    return format == "vantarift-jpeg" ? Capabilities(CanRead) : Capabilities{};
}

QImageIOHandler *VantariftJpegPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new VantariftJpegHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

QT_END_NAMESPACE

#include "main.moc"
