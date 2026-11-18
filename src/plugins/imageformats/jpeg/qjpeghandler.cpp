// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:critical reason:data-parser

#include "qjpeghandler_p.h"
#include "qt_jpeg_decode_feedback.h"
#include <QDataStream>

#include <qbuffer.h>
#include <qcolorspace.h>
#include <qcolortransform.h>
#include <qdebug.h>
#include <qimage.h>
#include <qimagereader.h>
#include <qlist.h>
#include <qloggingcategory.h>
#include <qmath.h>
#include <qvariant.h>

#include <limits>
#include <stdio.h>      // jpeglib needs this to be pre-included
#include <setjmp.h>

#ifdef FAR
#undef FAR
#endif

// including jpeglib.h seems to be a little messy
extern "C" {
#define XMD_H           // shut JPEGlib up
#include <jpeglib.h>
#ifdef const
#  undef const          // remove crazy C hackery in jconfig.h
#endif
}

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcFeedbackJpeg, "qt.gui.imageio.jpeg")

QT_WARNING_DISABLE_GCC("-Wclobbered")

struct my_error_mgr : public jpeg_error_mgr {
    jmp_buf setjmp_buffer;
    bool cancelled = false;
};

extern "C" {

static void feedback_cancel(j_common_ptr cinfo)
{
    if (QtJpegFeedback::cancelled()) {
        auto *error = static_cast<my_error_mgr *>(cinfo->err);
        error->cancelled = true;
        longjmp(error->setjmp_buffer, 1);
    }
}

static void my_error_exit (j_common_ptr cinfo)
{
    (*cinfo->err->output_message)(cinfo);
    my_error_mgr* myerr = (my_error_mgr*) cinfo->err;
    longjmp(myerr->setjmp_buffer, 1);
}

static void my_output_message(j_common_ptr cinfo)
{
    char buffer[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, buffer);
    qCWarning(lcFeedbackJpeg,"%s", buffer);
}

}


static const int max_buf = 4096;

struct my_jpeg_source_mgr : public jpeg_source_mgr {
    // Nothing dynamic - cannot rely on destruction over longjump
    QIODevice *device;
    JOCTET buffer[max_buf];
    const QBuffer *memDevice;

public:
    my_jpeg_source_mgr(QIODevice *device);
};

extern "C" {

static void qt_init_source(j_decompress_ptr)
{
}

static boolean qt_fill_input_buffer(j_decompress_ptr cinfo)
{
    feedback_cancel(j_common_ptr(cinfo));
    my_jpeg_source_mgr* src = (my_jpeg_source_mgr*)cinfo->src;
    qint64 num_read = 0;
    if (src->memDevice) {
        src->next_input_byte = (const JOCTET *)(src->memDevice->data().constData() + src->memDevice->pos());
        num_read = src->memDevice->data().size() - src->memDevice->pos();
        src->device->seek(src->memDevice->data().size());
    } else {
        src->next_input_byte = src->buffer;
        num_read = src->device->read((char*)src->buffer, max_buf);
    }
    if (num_read <= 0) {
        // Insert a fake EOI marker - as per jpeglib recommendation
        src->next_input_byte = src->buffer;
        src->buffer[0] = (JOCTET) 0xFF;
        src->buffer[1] = (JOCTET) JPEG_EOI;
        src->bytes_in_buffer = 2;
    } else {
        src->bytes_in_buffer = num_read;
    }
    return TRUE;
}

static void qt_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    feedback_cancel(j_common_ptr(cinfo));
    my_jpeg_source_mgr* src = (my_jpeg_source_mgr*)cinfo->src;

    // `dumb' implementation from jpeglib

    /* Just a dumb implementation for now.  Could use fseek() except
     * it doesn't work on pipes.  Not clear that being smart is worth
     * any trouble anyway --- large skips are infrequent.
     */
    if (num_bytes > 0) {
        while (num_bytes > (long) src->bytes_in_buffer) {  // Should not happen in case of memDevice
            num_bytes -= (long) src->bytes_in_buffer;
            (void) qt_fill_input_buffer(cinfo);
            /* note we assume that qt_fill_input_buffer will never return false,
            * so suspension need not be handled.
            */
        }
        src->next_input_byte += (size_t) num_bytes;
        src->bytes_in_buffer -= (size_t) num_bytes;
    }
}

static void qt_term_source(j_decompress_ptr cinfo)
{
    feedback_cancel(j_common_ptr(cinfo));
    my_jpeg_source_mgr* src = (my_jpeg_source_mgr*)cinfo->src;
    if (!src->device->isSequential())
        src->device->seek(src->device->pos() - src->bytes_in_buffer);
}

}

inline my_jpeg_source_mgr::my_jpeg_source_mgr(QIODevice *device)
{
    jpeg_source_mgr::init_source = qt_init_source;
    jpeg_source_mgr::fill_input_buffer = qt_fill_input_buffer;
    jpeg_source_mgr::skip_input_data = qt_skip_input_data;
    jpeg_source_mgr::resync_to_restart = jpeg_resync_to_restart;
    jpeg_source_mgr::term_source = qt_term_source;
    this->device = device;
    memDevice = qobject_cast<QBuffer *>(device);
    bytes_in_buffer = 0;
    next_input_byte = buffer;
}


inline static bool read_jpeg_size(int &w, int &h, j_decompress_ptr cinfo)
{
    (void) jpeg_calc_output_dimensions(cinfo);

    w = cinfo->output_width;
    h = cinfo->output_height;
    return true;
}

#define HIGH_QUALITY_THRESHOLD 50

inline static bool read_jpeg_format(QImage::Format &format, j_decompress_ptr cinfo)
{

    bool result = true;
    switch (cinfo->output_components) {
    case 1:
        format = QImage::Format_Grayscale8;
        break;
    case 3:
        format = QImage::Format_RGB32;
        break;
    case 4:
        if (cinfo->out_color_space == JCS_CMYK)
            format = QImage::Format_CMYK8888;
        else
            format = QImage::Format_RGB32;
        break;
    default:
        result = false;
        break;
    }
    cinfo->output_scanline = cinfo->output_height;
    return result;
}

static bool ensureValidImage(QImage *dest, struct jpeg_decompress_struct *info,
                             const QSize& size)
{
    QImage::Format format;
    switch (info->output_components) {
    case 1:
        format = QImage::Format_Grayscale8;
        break;
    case 3:
        format = QImage::Format_RGB32;
        break;
    case 4:
        if (info->out_color_space == JCS_CMYK)
            format = QImage::Format_CMYK8888;
        else
            format = QImage::Format_RGB32;
        break;
    default:
        return false; // unsupported format
    }

    return QImageIOHandler::allocateImage(size, format, dest);
}

static bool read_jpeg_image(QImage *outImage,
                            QSize scaledSize, QRect scaledClipRect,
                            QRect clipRect, int quality,
                            j_decompress_ptr info, struct my_error_mgr* err, bool invertCMYK)
{
    if (!setjmp(err->setjmp_buffer)) {
        // -1 means default quality.
        if (quality < 0)
            quality = 75;

        // If possible, merge the scaledClipRect into either scaledSize
        // or clipRect to avoid doing a separate scaled clipping pass.
        // Best results are achieved by clipping before scaling, not after.
        if (!scaledClipRect.isEmpty()) {
            if (scaledSize.isEmpty() && clipRect.isEmpty()) {
                // No clipping or scaling before final clip.
                clipRect = scaledClipRect;
                scaledClipRect = QRect();
            } else if (scaledSize.isEmpty()) {
                // Clipping, but no scaling: combine the clip regions.
                scaledClipRect.translate(clipRect.topLeft());
                clipRect = scaledClipRect.intersected(clipRect);
                scaledClipRect = QRect();
            } else if (clipRect.isEmpty()) {
                // No clipping, but scaling: if we can map back to an
                // integer pixel boundary, then clip before scaling.
                if ((info->image_width % scaledSize.width()) == 0 &&
                        (info->image_height % scaledSize.height()) == 0) {
                    int x = scaledClipRect.x() * info->image_width /
                            scaledSize.width();
                    int y = scaledClipRect.y() * info->image_height /
                            scaledSize.height();
                    int width = (scaledClipRect.right() + 1) *
                                info->image_width / scaledSize.width() - x;
                    int height = (scaledClipRect.bottom() + 1) *
                                 info->image_height / scaledSize.height() - y;
                    clipRect = QRect(x, y, width, height);
                    scaledSize = scaledClipRect.size();
                    scaledClipRect = QRect();
                }
            } else {
                // Clipping and scaling: too difficult to figure out,
                // and not a likely use case, so do it the long way.
            }
        }

        // Determine the scale factor to pass to libjpeg for quick downscaling.
        if (!scaledSize.isEmpty() && info->image_width && info->image_height) {
            if (clipRect.isEmpty()) {
                double f = qMin(double(info->image_width) / scaledSize.width(),
                                double(info->image_height) / scaledSize.height());

                // libjpeg supports M/8 scaling with M=[1,16]. All downscaling factors
                // are a speed improvement, but upscaling during decode is slower.
                info->scale_num   = qBound(1, qCeil(8/f), 8);
                info->scale_denom = 8;
            } else {
                info->scale_denom = qMin(clipRect.width() / scaledSize.width(),
                                         clipRect.height() / scaledSize.height());

                // Only scale by powers of two when clipping so we can
                // keep the exact pixel boundaries
                if (info->scale_denom < 2)
                    info->scale_denom = 1;
                else if (info->scale_denom < 4)
                    info->scale_denom = 2;
                else if (info->scale_denom < 8)
                    info->scale_denom = 4;
                else
                    info->scale_denom = 8;
                info->scale_num = 1;

                // Correct the scale factor so that we clip accurately.
                // It is recommended that the clip rectangle be aligned
                // on an 8-pixel boundary for best performance.
                while (info->scale_denom > 1 &&
                       ((clipRect.x() % info->scale_denom) != 0 ||
                        (clipRect.y() % info->scale_denom) != 0 ||
                        (clipRect.width() % info->scale_denom) != 0 ||
                        (clipRect.height() % info->scale_denom) != 0)) {
                    info->scale_denom /= 2;
                }
            }
        }

        // If high quality not required, use fast decompression
        if ( quality < HIGH_QUALITY_THRESHOLD ) {
            info->dct_method = JDCT_IFAST;
            info->do_fancy_upsampling = FALSE;
        }

        if (info->out_color_space == JCS_RGB) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            info->out_color_space = JCS_EXT_BGRA;
#else
            info->out_color_space = JCS_EXT_ARGB;
#endif
        }
        (void) jpeg_calc_output_dimensions(info);

        // Determine the clip region to extract.
        QRect imageRect(0, 0, info->output_width, info->output_height);
        QRect clip;
        if (clipRect.isEmpty()) {
            clip = imageRect;
        } else if (info->scale_denom == info->scale_num) {
            clip = clipRect.intersected(imageRect);
        } else {
            // The scale factor was corrected above to ensure that
            // we don't miss pixels when we scale the clip rectangle.
            clip = QRect(clipRect.x() / int(info->scale_denom),
                         clipRect.y() / int(info->scale_denom),
                         clipRect.width() / int(info->scale_denom),
                         clipRect.height() / int(info->scale_denom));
            clip = clip.intersected(imageRect);
        }

        // Allocate memory for the clipped QImage.
        if (!ensureValidImage(outImage, info, clip.size()))
            return false;

        // Avoid memcpy() overhead if grayscale with no clipping.
        bool quickGray = (info->output_components == 1 &&
                          clip == imageRect);
        if (!quickGray) {
            // Ask the jpeg library to allocate a temporary row.
            // The library will automatically delete it for us later.
            // The libjpeg docs say we should do this before calling
            // jpeg_start_decompress().  We can't use "new" here
            // because we are inside the setjmp() block and an error
            // in the jpeg input stream would cause a memory leak.
            JSAMPARRAY rows = (info->mem->alloc_sarray)
                              ((j_common_ptr)info, JPOOL_IMAGE,
                               info->output_width * info->output_components, 1);

            (void) jpeg_start_decompress(info);

            while (info->output_scanline < info->output_height) {
                int y = int(info->output_scanline) - clip.y();
                if (y >= clip.height())
                    break;      // We've read the entire clip region, so abort.

                (void) jpeg_read_scanlines(info, rows, 1);

                if (y < 0)
                    continue;   // Haven't reached the starting line yet.

                if (info->out_color_space == JCS_CMYK) {
                    uchar *in = rows[0] + clip.x() * 4;
                    quint32 *out = (quint32*)outImage->scanLine(y);
                    if (invertCMYK) {
                        for (int i = 0; i < clip.width(); ++i) {
                            *out++ = 0xffffffffu - (in[0] | in[1] << 8 | in[2] << 16 | in[3] << 24);
                            in += 4;
                        }
                    } else {
                        memcpy(out, in, clip.width() * 4);
                    }
                } else if (info->output_components == 4) {
                    memcpy(outImage->scanLine(y), rows[0] + clip.x() * 4, clip.width() * 4);
                } else if (info->output_components == 1) {
                    // Grayscale.
                    memcpy(outImage->scanLine(y),
                           rows[0] + clip.x(), clip.width());
                }
            }
        } else {
            // Load unclipped grayscale data directly into the QImage.
            (void) jpeg_start_decompress(info);
            while (info->output_scanline < info->output_height) {
                uchar *row = outImage->scanLine(info->output_scanline);
                (void) jpeg_read_scanlines(info, &row, 1);
            }
        }

        if (info->output_scanline == info->output_height)
            (void) jpeg_finish_decompress(info);

        if (info->density_unit == 1) {
            outImage->setDotsPerMeterX(int(100. * info->X_density / 2.54));
            outImage->setDotsPerMeterY(int(100. * info->Y_density / 2.54));
        } else if (info->density_unit == 2) {
            outImage->setDotsPerMeterX(int(100. * info->X_density));
            outImage->setDotsPerMeterY(int(100. * info->Y_density));
        }

        if (scaledSize.isValid() && scaledSize != clip.size()) {
            *outImage = outImage->scaled(scaledSize, Qt::IgnoreAspectRatio, quality >= HIGH_QUALITY_THRESHOLD ? Qt::SmoothTransformation : Qt::FastTransformation);
        }

        if (!scaledClipRect.isEmpty())
            *outImage = outImage->copy(scaledClipRect);
        return !outImage->isNull();
    }
    else {
        if (!err->cancelled)
            my_output_message(j_common_ptr(info));
        return false;
    }
}

class VantariftJpegHandlerPrivate
{
public:
    enum State {
        Ready,
        ReadHeader,
        ReadingEnd,
        Error
    };

    VantariftJpegHandlerPrivate(VantariftJpegHandler *qq)
        : quality(75), transformation(QImageIOHandler::TransformationNone), iod_src(nullptr),
          state(Ready), optimize(false), progressive(false), q(qq)
    {}

    ~VantariftJpegHandlerPrivate()
    {
        if (iod_src)
        {
            jpeg_destroy_decompress(&info);
            delete iod_src;
            iod_src = nullptr;
        }
    }

    bool readJpegHeader(QIODevice*);
    bool read(QImage *image);

    int quality;
    QImageIOHandler::Transformations transformation;
    QVariant size;
    QImage::Format format;
    QSize scaledSize;
    QRect scaledClipRect;
    QRect clipRect;
    QString description;
    QStringList readTexts;
    QByteArray iccProfile;

    // Photoshop historically invertes the quantities in CMYK JPEG files:
    // 0 means 100% ink, 255 means no ink. Every reader does the same,
    // for compatibility reasons.
    // Use such an interpretation by default, but also offer the alternative
    // of not inverting the channels.
    // This is just a "fancy" API; it could be reduced to a boolean setting
    // for CMYK files.
    enum class SubType {
        Automatic,
        Inverted_CMYK,
        CMYK,
        NSubTypes
    };
    SubType subType = SubType::Automatic;

    struct jpeg_decompress_struct info;
    struct my_jpeg_source_mgr * iod_src;
    struct my_error_mgr err;

    jpeg_progress_mgr progress{};

    State state;

    bool optimize;
    bool progressive;

    VantariftJpegHandler *q;
};

static const char SupportedJPEGSubtypes[][14] = {
    "Automatic",
    "Inverted_CMYK",
    "CMYK"
};

static_assert(std::size(SupportedJPEGSubtypes) == size_t(VantariftJpegHandlerPrivate::SubType::NSubTypes));

static bool readExifHeader(QDataStream &stream)
{
    char prefix[6];
    if (stream.readRawData(prefix, sizeof(prefix)) != sizeof(prefix))
        return false;
    static const char exifMagic[6] = {'E', 'x', 'i', 'f', 0, 0};
    return memcmp(prefix, exifMagic, 6) == 0;
}

/*
 * Returns -1 on error
 * Returns 0 if no Exif orientation was found
 * Returns 1 orientation is horizontal (normal)
 * Returns 2 mirror horizontal
 * Returns 3 rotate 180
 * Returns 4 mirror vertical
 * Returns 5 mirror horizontal and rotate 270 CCW
 * Returns 6 rotate 90 CW
 * Returns 7 mirror horizontal and rotate 90 CW
 * Returns 8 rotate 270 CW
 */
static int getExifOrientation(QByteArray &exifData)
{
    // Current EXIF version (2.3) says there can be at most 5 IFDs,
    // byte we allow for 10 so we're able to deal with future extensions.
    const int maxIfdCount = 10;

    QDataStream stream(&exifData, QIODevice::ReadOnly);

    if (!readExifHeader(stream))
        return -1;

    quint16 val;
    quint32 offset;
    const qint64 headerStart = 6;   // the EXIF header has a constant size
    Q_ASSERT(headerStart == stream.device()->pos());

    // read byte order marker
    stream >> val;
    if (val == 0x4949) // 'II' == Intel
        stream.setByteOrder(QDataStream::LittleEndian);
    else if (val == 0x4d4d) // 'MM' == Motorola
        stream.setByteOrder(QDataStream::BigEndian);
    else
        return -1; // unknown byte order

    // confirm byte order
    stream >> val;
    if (val != 0x2a)
        return -1;

    stream >> offset;

    // read IFD
    for (int n = 0; n < maxIfdCount; ++n) {
        quint16 numEntries;

        const qint64 bytesToSkip = offset - (stream.device()->pos() - headerStart);
        if (bytesToSkip < 0 || (offset + headerStart >= exifData.size())) {
            // disallow going backwards, though it's permitted in the spec
            return -1;
        } else if (bytesToSkip != 0) {
            // seek to the IFD
            if (!stream.device()->seek(offset + headerStart))
                return -1;
        }

        stream >> numEntries;

        for (; numEntries > 0 && stream.status() == QDataStream::Ok; --numEntries) {
            quint16 tag;
            quint16 type;
            quint32 components;
            quint16 value;
            quint16 dummy;

            stream >> tag >> type >> components >> value >> dummy;
            if (tag == 0x0112) { // Tag Exif.Image.Orientation
                if (components != 1)
                    return -1;
                if (type != 3) // we are expecting it to be an unsigned short
                    return -1;
                if (value < 1 || value > 8) // check for valid range
                    return -1;

                // It is possible to include the orientation multiple times.
                // Right now the first value is returned.
                return value;
            }
        }

        // read offset to next IFD
        if (!(stream >> offset))
            return -1;
        if (offset == 0) // this is the last IFD
            return 0;   // No Exif orientation was found
    }

    // too many IFDs
    return -1;
}

static QImageIOHandler::Transformations exif2Qt(int exifOrientation)
{
    switch (exifOrientation) {
    case 1: // normal
        return QImageIOHandler::TransformationNone;
    case 2: // mirror horizontal
        return QImageIOHandler::TransformationMirror;
    case 3: // rotate 180
        return QImageIOHandler::TransformationRotate180;
    case 4: // mirror vertical
        return QImageIOHandler::TransformationFlip;
    case 5: // mirror horizontal and rotate 270 CW
        return QImageIOHandler::TransformationFlipAndRotate90;
    case 6: // rotate 90 CW
        return QImageIOHandler::TransformationRotate90;
    case 7: // mirror horizontal and rotate 90 CW
        return QImageIOHandler::TransformationMirrorAndRotate90;
    case 8: // rotate 270 CW
        return QImageIOHandler::TransformationRotate270;
    }
    qCWarning(lcFeedbackJpeg, "Invalid EXIF orientation");
    return QImageIOHandler::TransformationNone;
}

/*!
    \internal
*/
bool VantariftJpegHandlerPrivate::readJpegHeader(QIODevice *device)
{
    if (state == Ready)
    {
        state = Error;
        iod_src = new my_jpeg_source_mgr(device);

        info.err = jpeg_std_error(&err);
        err.error_exit = my_error_exit;
        err.output_message = my_output_message;

        jpeg_create_decompress(&info);
        info.src = iod_src;
        progress.progress_monitor = feedback_cancel;
        info.progress = &progress;
        if (const int mbLimit = QImageReader::allocationLimit()) {
            const qint64 bMax = 2 * qint64(mbLimit) * 1024 * 1024; // Some overhead for temp alloc
            info.mem->max_memory_to_use = long(qMin(bMax, qint64(std::numeric_limits<long>::max())));
        }

        if (!setjmp(err.setjmp_buffer)) {
            jpeg_save_markers(&info, JPEG_COM, 0xFFFF);
            jpeg_save_markers(&info, JPEG_APP0 + 1, 0xFFFF); // Exif uses APP1 marker
            jpeg_save_markers(&info, JPEG_APP0 + 2, 0xFFFF); // ICC uses APP2 marker

            (void) jpeg_read_header(&info, TRUE);

            int width = 0;
            int height = 0;
            read_jpeg_size(width, height, &info);
            size = QSize(width, height);

            format = QImage::Format_Invalid;
            read_jpeg_format(format, &info);

            QByteArray exifData;

            for (jpeg_saved_marker_ptr marker = info.marker_list; marker != nullptr; marker = marker->next) {
                if (marker->marker == JPEG_COM) {
#ifndef QT_NO_IMAGEIO_TEXT_LOADING
                    QString key, value;
                    QString s = QString::fromUtf8((const char *)marker->data, marker->data_length);
                    int index = s.indexOf(QLatin1String(": "));
                    if (index == -1 || s.indexOf(QLatin1Char(' ')) < index) {
                        key = QLatin1String("Description");
                        value = s;
                    } else {
                        key = s.left(index);
                        value = s.mid(index + 2);
                    }
                    if (!description.isEmpty())
                        description += QLatin1String("\n\n");
                    description += key + QLatin1String(": ") + value.simplified();
                    readTexts.append(key);
                    readTexts.append(value);
#endif
                } else if (marker->marker == JPEG_APP0 + 1) {
                    exifData.append((const char*)marker->data, marker->data_length);
                } else if (marker->marker == JPEG_APP0 + 2) {
                    if (marker->data_length > 128 + 4 + 14 && strcmp((const char *)marker->data, "ICC_PROFILE") == 0) {
                        iccProfile.append((const char*)marker->data + 14, marker->data_length - 14);
                    }
                }
            }

            if (!exifData.isEmpty()) {
                // Exif data present
                int exifOrientation = getExifOrientation(exifData);
                if (exifOrientation > 0)
                    transformation = exif2Qt(exifOrientation);
            }

            state = ReadHeader;
            return true;
        }
        else {
            my_output_message(j_common_ptr(&info));
            return false;
        }
    }
    else if (state == Error)
        return false;
    return true;
}

bool VantariftJpegHandlerPrivate::read(QImage *image)
{
    if (state == Ready)
        readJpegHeader(q->device());

    if (state == ReadHeader)
    {
        const bool invertCMYK = subType != VantariftJpegHandlerPrivate::SubType::CMYK;
        bool success = read_jpeg_image(image, scaledSize, scaledClipRect, clipRect, quality, &info, &err, invertCMYK);
        if (success) {
            for (int i = 0; i < readTexts.size()-1; i+=2)
                image->setText(readTexts.at(i), readTexts.at(i+1));

            if (!iccProfile.isEmpty())
                image->setColorSpace(QColorSpace::fromIccProfile(iccProfile));

            state = ReadingEnd;
            return true;
        }

        state = Error;
    }

    return false;
}

VantariftJpegHandler::VantariftJpegHandler()
    : d(new VantariftJpegHandlerPrivate(this))
{}

VantariftJpegHandler::~VantariftJpegHandler()
{
    delete d;
}

bool VantariftJpegHandler::canRead() const
{
    if (d->state == VantariftJpegHandlerPrivate::Ready && !canRead(device()))
        return false;

    if (d->state != VantariftJpegHandlerPrivate::Error && d->state != VantariftJpegHandlerPrivate::ReadingEnd) {
        setFormat("jpeg");
        return true;
    }

    return false;
}

bool VantariftJpegHandler::canRead(QIODevice *device)
{
    if (!device) {
        qCWarning(lcFeedbackJpeg, "VantariftJpegHandler::canRead() called with no device");
        return false;
    }

    char buffer[2];
    if (device->peek(buffer, 2) != 2)
        return false;
    return uchar(buffer[0]) == 0xff && uchar(buffer[1]) == 0xd8;
}

bool VantariftJpegHandler::read(QImage *image)
{
    if (!canRead())
        return false;
    return d->read(image);
}

bool VantariftJpegHandler::write(const QImage &)
{
    return false;
}

bool VantariftJpegHandler::supportsOption(ImageOption option) const
{
    return option == Quality
        || option == ScaledSize
        || option == ScaledClipRect
        || option == ClipRect
        || option == Description
        || option == Size
        || option == SubType
        || option == SupportedSubTypes
        || option == ImageFormat
        || option == OptimizedWrite
        || option == ProgressiveScanWrite
        || option == ImageTransformation;
}

QVariant VantariftJpegHandler::option(ImageOption option) const
{
    switch(option) {
    case Quality:
        return d->quality;
    case ScaledSize:
        return d->scaledSize;
    case ScaledClipRect:
        return d->scaledClipRect;
    case ClipRect:
        return d->clipRect;
    case Description:
        d->readJpegHeader(device());
        return d->description;
    case Size:
        d->readJpegHeader(device());
        return d->size;
    case SubType:
        return QByteArray(SupportedJPEGSubtypes[int(d->subType)]);
    case SupportedSubTypes: {
        QByteArrayList list(std::begin(SupportedJPEGSubtypes),
                            std::end(SupportedJPEGSubtypes));
        return QVariant::fromValue(list);
    }
    case ImageFormat:
        d->readJpegHeader(device());
        return d->format;
    case OptimizedWrite:
        return d->optimize;
    case ProgressiveScanWrite:
        return d->progressive;
    case ImageTransformation:
        d->readJpegHeader(device());
        return int(d->transformation);
    default:
        break;
    }

    return QVariant();
}

void VantariftJpegHandler::setOption(ImageOption option, const QVariant &value)
{
    switch(option) {
    case Quality:
        d->quality = value.toInt();
        break;
    case ScaledSize:
        d->scaledSize = value.toSize();
        break;
    case ScaledClipRect:
        d->scaledClipRect = value.toRect();
        break;
    case ClipRect:
        d->clipRect = value.toRect();
        break;
    case Description:
        d->description = value.toString();
        break;
    case SubType: {
        const QByteArray subType = value.toByteArray();
        for (size_t i = 0; i < std::size(SupportedJPEGSubtypes); ++i) {
            if (subType == SupportedJPEGSubtypes[i]) {
                d->subType = VantariftJpegHandlerPrivate::SubType(i);
                break;
            }
        }
        break;
    }
    case OptimizedWrite:
        d->optimize = value.toBool();
        break;
    case ProgressiveScanWrite:
        d->progressive = value.toBool();
        break;
    case ImageTransformation: {
        int transformation = value.toInt();
        if (transformation > 0 && transformation < 8)
            d->transformation = QImageIOHandler::Transformations(transformation);
        break;
    }
    default:
        break;
    }
}

QT_END_NAMESPACE
