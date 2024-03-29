// ***************************************************************** -*- C++ -*-
/*
 * Copyright (C) 2004-2021 Exiv2 authors
 * This program is part of the Exiv2 distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301 USA.
 */

// *****************************************************************************

// included header files
#include "jp2image.hpp"

#include "basicio.hpp"
#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "futils.hpp"
#include "image.hpp"
#include "image_int.hpp"
#include "jp2image_int.hpp"
#include "safe_op.hpp"
#include "tiffimage.hpp"
#include "types.hpp"

// + standard includes
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

const size_t boxSize = sizeof(Exiv2::Internal::Jp2BoxHeader);
// JPEG-2000 box types
const uint32_t kJp2BoxTypeSignature = 0x6a502020;    // signature box, required,
const uint32_t kJp2BoxTypeFileTypeBox = 0x66747970;  // File type box, required
const uint32_t kJp2BoxTypeHeader = 0x6a703268;       // Jp2 Header Box, required, Superbox
const uint32_t kJp2BoxTypeImageHeader = 0x69686472;  // Image Header Box ('ihdr'), required,
const uint32_t kJp2BoxTypeColorSpec = 0x636f6c72;    // Color Specification box ('colr'), required
const uint32_t kJp2BoxTypeUuid = 0x75756964;         // 'uuid'
const uint32_t kJp2BoxTypeClose = 0x6a703263;        // 'jp2c'

// JPEG-2000 UUIDs for embedded metadata
//
// See http://www.jpeg.org/public/wg1n2600.doc for information about embedding IPTC-NAA data in JPEG-2000 files
// See http://www.adobe.com/devnet/xmp/pdfs/xmp_specification.pdf for information about embedding XMP data in JPEG-2000
// files
const unsigned char kJp2UuidExif[] = "JpgTiffExif->JP2";
const unsigned char kJp2UuidIptc[] = "\x33\xc7\xa4\xd2\xb8\x1d\x47\x23\xa0\xba\xf1\xa3\xe0\x97\xad\x38";
const unsigned char kJp2UuidXmp[] = "\xbe\x7a\xcf\xcb\x97\xa9\x42\xe8\x9c\x71\x99\x94\x91\xe3\xaf\xac";

// See section B.1.1 (JPEG 2000 Signature box) of JPEG-2000 specification
const unsigned char Jp2Signature[12] = {0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a};

const unsigned char Jp2Blank[] = {
    0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a, 0x00, 0x00, 0x00, 0x14, 0x66, 0x74,
    0x79, 0x70, 0x6a, 0x70, 0x32, 0x20, 0x00, 0x00, 0x00, 0x00, 0x6a, 0x70, 0x32, 0x20, 0x00, 0x00, 0x00, 0x2d,
    0x6a, 0x70, 0x32, 0x68, 0x00, 0x00, 0x00, 0x16, 0x69, 0x68, 0x64, 0x72, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x01, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x63, 0x6f, 0x6c, 0x72, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x6a, 0x70, 0x32, 0x63, 0xff, 0x4f, 0xff, 0x51, 0x00,
    0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x07, 0x01, 0x01, 0xff, 0x64, 0x00, 0x23, 0x00, 0x01, 0x43, 0x72, 0x65, 0x61, 0x74, 0x6f, 0x72, 0x3a,
    0x20, 0x4a, 0x61, 0x73, 0x50, 0x65, 0x72, 0x20, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x20, 0x31, 0x2e,
    0x39, 0x30, 0x30, 0x2e, 0x31, 0xff, 0x52, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05, 0x04, 0x04, 0x00,
    0x01, 0xff, 0x5c, 0x00, 0x13, 0x40, 0x40, 0x48, 0x48, 0x50, 0x48, 0x48, 0x50, 0x48, 0x48, 0x50, 0x48, 0x48,
    0x50, 0x48, 0x48, 0x50, 0xff, 0x90, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x01, 0xff, 0x5d,
    0x00, 0x14, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xff, 0x93, 0xcf, 0xb4, 0x04, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xd9};

// *****************************************************************************
// class member definitions
namespace Exiv2
{
    Jp2Image::Jp2Image(BasicIo::AutoPtr io, bool create) : Image(ImageType::jp2, mdExif | mdIptc | mdXmp, io)
    {
        if (create) {
            if (io_->open() == 0) {
#ifdef EXIV2_DEBUG_MESSAGES
                std::cerr << "Exiv2::Jp2Image:: Creating JPEG2000 image to memory" << std::endl;
#endif
                IoCloser closer(*io_);
                if (io_->write(Jp2Blank, sizeof(Jp2Blank)) != sizeof(Jp2Blank)) {
#ifdef EXIV2_DEBUG_MESSAGES
                    std::cerr << "Exiv2::Jp2Image:: Failed to create JPEG2000 image on memory" << std::endl;
#endif
                }
            }
        }
    }  // Jp2Image::Jp2Image

    std::string Jp2Image::mimeType() const
    {
        return "image/jp2";
    }

    void Jp2Image::setComment(const std::string& /*comment*/)
    {
        // Todo: implement me!
        throw(Error(kerInvalidSettingForImage, "Image comment", "JP2"));
    }  // Jp2Image::setComment

    static void lf(std::ostream& out, bool& bLF)
    {
        if (bLF) {
            out << std::endl;
            out.flush();
            bLF = false;
        }
    }

    static bool isBigEndian()
    {
        union {
            uint32_t i;
            char c[4];
        } e = {0x01000000};

        return e.c[0] ? true : false;
    }

    static std::string toAscii(long n)
    {
        const char* p = (const char*)&n;
        std::string result;
        bool bBigEndian = isBigEndian();
        for (int i = 0; i < 4; i++) {
            result += p[bBigEndian ? i : (3 - i)];
        }
        return result;
    }

    static void boxes_check(size_t b, size_t m)
    {
        if (b > m) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cout << "Exiv2::Jp2Image::readMetadata box maximum exceeded" << std::endl;
#endif
            throw Error(kerCorruptedMetadata);
        }
    }

    void Jp2Image::readMetadata()
    {
#ifdef EXIV2_DEBUG_MESSAGES
        std::cerr << "Exiv2::Jp2Image::readMetadata: Reading JPEG-2000 file " << io_->path() << std::endl;
#endif
        if (io_->open() != 0) {
            throw Error(kerDataSourceOpenFailed, io_->path(), strError());
        }
        IoCloser closer(*io_);
        // Ensure that this is the correct image type
        if (!isJp2Type(*io_, false)) {
            throw Error(kerNotAnImage, "JPEG-2000");
        }

        long position = 0;
        Internal::Jp2BoxHeader box = {0, 0};
        Internal::Jp2BoxHeader subBox = {0, 0};
        Internal::Jp2ImageHeaderBox ihdr = {0, 0, 0, 0, 0, 0, 0};
        Internal::Jp2UuidBox uuid = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
        size_t boxes = 0;
        size_t boxem = 1000;  // boxes max
        uint32_t lastBoxTypeRead = 0;
        bool boxSignatureFound = false;
        bool boxFileTypeFound = false;

        while (io_->read((byte*)&box, sizeof(box)) == sizeof(box)) {
            boxes_check(boxes++, boxem);
            position = io_->tell();
            box.length = getLong((byte*)&box.length, bigEndian);
            box.type = getLong((byte*)&box.type, bigEndian);
#ifdef EXIV2_DEBUG_MESSAGES
            std::cout << "Exiv2::Jp2Image::readMetadata: "
                      << "Position: " << position << " box type: " << toAscii(box.type) << " length: " << box.length
                      << std::endl;
#endif
            enforce(box.length <= sizeof(box) + io_->size() - io_->tell(), Exiv2::kerCorruptedMetadata);

            if (box.length == 0)
                return;

            if (box.length == 1) {
                /// \todo In this case, the real box size is given in XLBox (bytes 8-15)
            }

            switch (box.type) {
                case kJp2BoxTypeSignature: {
                    if (boxSignatureFound)  // Only one is allowed
                        throw Error(kerCorruptedMetadata);
                    boxSignatureFound = true;
                    break;
                }
                case kJp2BoxTypeFileTypeBox: {
                    // This box shall immediately follow the JPEG 2000 Signature box
                    if (boxFileTypeFound || lastBoxTypeRead != kJp2BoxTypeSignature) {  // Only one is allowed
                        throw Error(kerCorruptedMetadata);
                    }
                    boxFileTypeFound = true;
                    std::vector<byte> boxData(box.length - boxSize);
                    io_->read(boxData.data(), static_cast<long>(boxData.size()));
                    if (!Internal::isValidBoxFileType(boxData))
                        throw Error(kerCorruptedMetadata);
                    break;
                }
                case kJp2BoxTypeHeader: {
#ifdef EXIV2_DEBUG_MESSAGES
                    std::cout << "Exiv2::Jp2Image::readMetadata: JP2Header box found" << std::endl;
#endif
                    long restore = io_->tell();

                    while (io_->read((byte*)&subBox, sizeof(subBox)) == sizeof(subBox) && subBox.length) {
                        boxes_check(boxes++, boxem);
                        subBox.length = getLong((byte*)&subBox.length, bigEndian);
                        subBox.type = getLong((byte*)&subBox.type, bigEndian);
                        if (subBox.length > io_->size()) {
                            throw Error(kerCorruptedMetadata);
                        }
#ifdef EXIV2_DEBUG_MESSAGES
                        std::cout << "Exiv2::Jp2Image::readMetadata: "
                                  << "subBox = " << toAscii(subBox.type) << " length = " << subBox.length << std::endl;
#endif
                        if (subBox.type == kJp2BoxTypeColorSpec && subBox.length != 15) {
#ifdef EXIV2_DEBUG_MESSAGES
                            std::cout << "Exiv2::Jp2Image::readMetadata: "
                                      << "Color data found" << std::endl;
#endif

                            const long pad = 3;  // 3 padding bytes 2 0 0
                            const size_t data_length = Safe::add(subBox.length, static_cast<uint32_t>(8));
                            // data_length makes no sense if it is larger than the rest of the file
                            if (data_length > io_->size() - io_->tell()) {
                                throw Error(kerCorruptedMetadata);
                            }
                            DataBuf data(static_cast<long>(data_length));
                            io_->read(data.pData_, data.size_);
                            const long iccLength = getULong(data.pData_ + pad, bigEndian);
                            // subtracting pad from data.size_ is safe:
                            // size_ is at least 8 and pad = 3
                            if (iccLength > data.size_ - pad) {
                                throw Error(kerCorruptedMetadata);
                            }
                            DataBuf icc(iccLength);
                            ::memcpy(icc.pData_, data.pData_ + pad, icc.size_);
#ifdef EXIV2_DEBUG_MESSAGES
                            const char* iccPath = "/tmp/libexiv2_jp2.icc";
                            FILE* f = fopen(iccPath, "wb");
                            if (f) {
                                fwrite(icc.pData_, icc.size_, 1, f);
                                fclose(f);
                            }
                            std::cout << "Exiv2::Jp2Image::readMetadata: wrote iccProfile " << icc.size_ << " bytes to "
                                      << iccPath << std::endl;
#endif
                            setIccProfile(icc);
                        }

                        if (subBox.type == kJp2BoxTypeImageHeader) {
                            io_->read((byte*)&ihdr, sizeof(ihdr));
#ifdef EXIV2_DEBUG_MESSAGES
                            std::cout << "Exiv2::Jp2Image::readMetadata: Ihdr data found" << std::endl;
#endif
                            ihdr.imageHeight = getLong((byte*)&ihdr.imageHeight, bigEndian);
                            ihdr.imageWidth = getLong((byte*)&ihdr.imageWidth, bigEndian);
                            ihdr.componentCount = getShort((byte*)&ihdr.componentCount, bigEndian);
                            if (ihdr.c != 7) {
                                throw Error(kerCorruptedMetadata);
                            }

                            pixelWidth_ = ihdr.imageWidth;
                            pixelHeight_ = ihdr.imageHeight;
                        }

                        io_->seek(restore, BasicIo::beg);
                        if (io_->seek(subBox.length, Exiv2::BasicIo::cur) != 0) {
                            throw Error(kerCorruptedMetadata);
                        }
                        restore = io_->tell();
                    }
                    break;
                }
                case kJp2BoxTypeUuid: {
#ifdef EXIV2_DEBUG_MESSAGES
                    std::cout << "Exiv2::Jp2Image::readMetadata: UUID box found" << std::endl;
#endif

                    if (io_->read((byte*)&uuid, sizeof(uuid)) == sizeof(uuid)) {
                        DataBuf rawData;
                        long bufRead;
                        bool bIsExif = memcmp(uuid.uuid, kJp2UuidExif, sizeof(uuid)) == 0;
                        bool bIsIPTC = memcmp(uuid.uuid, kJp2UuidIptc, sizeof(uuid)) == 0;
                        bool bIsXMP = memcmp(uuid.uuid, kJp2UuidXmp, sizeof(uuid)) == 0;

                        if (bIsExif) {
#ifdef EXIV2_DEBUG_MESSAGES
                            std::cout << "Exiv2::Jp2Image::readMetadata: Exif data found" << std::endl;
#endif
                            enforce(box.length >= sizeof(box) + sizeof(uuid), kerCorruptedMetadata);
                            rawData.alloc(box.length - (sizeof(box) + sizeof(uuid)));
                            bufRead = io_->read(rawData.pData_, rawData.size_);
                            if (io_->error())
                                throw Error(kerFailedToReadImageData);
                            if (bufRead != rawData.size_)
                                throw Error(kerInputDataReadFailed);

                            if (rawData.size_ > 8)  // "II*\0long"
                            {
                                // Find the position of Exif header in bytes array.
                                long pos = ((rawData.pData_[0] == rawData.pData_[1]) &&
                                            (rawData.pData_[0] == 'I' || rawData.pData_[0] == 'M'))
                                               ? 0
                                               : -1;

                                // #1242  Forgive having Exif\0\0 in rawData.pData_
                                const byte exifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
                                for (long i = 0; pos < 0 && i < rawData.size_ - (long)sizeof(exifHeader); i++) {
                                    if (memcmp(exifHeader, &rawData.pData_[i], sizeof(exifHeader)) == 0) {
                                        pos = i + sizeof(exifHeader);
#ifndef SUPPRESS_WARNINGS
                                        EXV_WARNING << "Reading non-standard UUID-EXIF_bad box in " << io_->path()
                                                    << std::endl;
#endif
                                    }
                                }

                                // If found it, store only these data at from this place.
                                if (pos >= 0) {
#ifdef EXIV2_DEBUG_MESSAGES
                                    std::cout << "Exiv2::Jp2Image::readMetadata: Exif header found at position " << pos
                                              << std::endl;
#endif
                                    ByteOrder bo = TiffParser::decode(exifData(), iptcData(), xmpData(),
                                                                      rawData.pData_ + pos, rawData.size_ - pos);
                                    setByteOrder(bo);
                                }
                            } else {
#ifndef SUPPRESS_WARNINGS
                                EXV_WARNING << "Failed to decode Exif metadata." << std::endl;
#endif
                                exifData_.clear();
                            }
                        }

                        if (bIsIPTC) {
#ifdef EXIV2_DEBUG_MESSAGES
                            std::cout << "Exiv2::Jp2Image::readMetadata: Iptc data found" << std::endl;
#endif
                            enforce(box.length >= sizeof(box) + sizeof(uuid), kerCorruptedMetadata);
                            rawData.alloc(box.length - (sizeof(box) + sizeof(uuid)));
                            bufRead = io_->read(rawData.pData_, rawData.size_);
                            if (io_->error())
                                throw Error(kerFailedToReadImageData);
                            if (bufRead != rawData.size_)
                                throw Error(kerInputDataReadFailed);

                            if (IptcParser::decode(iptcData_, rawData.pData_, rawData.size_)) {
#ifndef SUPPRESS_WARNINGS
                                EXV_WARNING << "Failed to decode IPTC metadata." << std::endl;
#endif
                                iptcData_.clear();
                            }
                        }

                        if (bIsXMP) {
#ifdef EXIV2_DEBUG_MESSAGES
                            std::cout << "Exiv2::Jp2Image::readMetadata: Xmp data found" << std::endl;
#endif
                            enforce(box.length >= sizeof(box) + sizeof(uuid), kerCorruptedMetadata);
                            rawData.alloc(box.length - (uint32_t)(sizeof(box) + sizeof(uuid)));
                            bufRead = io_->read(rawData.pData_, rawData.size_);
                            if (io_->error())
                                throw Error(kerFailedToReadImageData);
                            if (bufRead != rawData.size_)
                                throw Error(kerInputDataReadFailed);
                            xmpPacket_.assign(reinterpret_cast<char*>(rawData.pData_), rawData.size_);

                            std::string::size_type idx = xmpPacket_.find_first_of('<');
                            if (idx != std::string::npos && idx > 0) {
#ifndef SUPPRESS_WARNINGS
                                EXV_WARNING << "Removing " << static_cast<uint32_t>(idx)
                                            << " characters from the beginning of the XMP packet" << std::endl;
#endif
                                xmpPacket_ = xmpPacket_.substr(idx);
                            }

                            if (xmpPacket_.size() > 0 && XmpParser::decode(xmpData_, xmpPacket_)) {
#ifndef SUPPRESS_WARNINGS
                                EXV_WARNING << "Failed to decode XMP metadata." << std::endl;
#endif
                            }
                        }
                    }
                    break;
                }

                default: {
                    break;
                }
            }
            lastBoxTypeRead = box.type;

            // Move to the next box.
            io_->seek(static_cast<long>(position - sizeof(box) + box.length), BasicIo::beg);
            if (io_->error())
                throw Error(kerFailedToReadImageData);
        }
    }

    void Jp2Image::printStructure(std::ostream& out, PrintStructureOption option, int depth)
    {
        if (io_->open() != 0)
            throw Error(kerDataSourceOpenFailed, io_->path(), strError());

        // Ensure that this is the correct image type
        if (!isJp2Type(*io_, false)) {
            if (io_->error() || io_->eof())
                throw Error(kerFailedToReadImageData);
            throw Error(kerNotAJpeg);
        }

        bool bPrint = option == kpsBasic || option == kpsRecursive;
        bool bRecursive = option == kpsRecursive;
        bool bICC = option == kpsIccProfile;
        bool bXMP = option == kpsXMP;
        bool bIPTCErase = option == kpsIptcErase;
        bool boxSignatureFound = false;

        if (bPrint) {
            out << "STRUCTURE OF JPEG2000 FILE: " << io_->path() << std::endl;
            out << " address |   length | box       | data" << std::endl;
        }

        if (bPrint || bXMP || bICC || bIPTCErase) {
            long position = 0;
            Internal::Jp2BoxHeader box = {1, 1};
            Internal::Jp2BoxHeader subBox = {1, 1};
            Internal::Jp2UuidBox uuid = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
            bool bLF = false;

            while (box.length && box.type != kJp2BoxTypeClose && io_->read((byte*)&box, sizeof(box)) == sizeof(box)) {
                position = io_->tell();
                box.length = getLong((byte*)&box.length, bigEndian);
                box.type = getLong((byte*)&box.type, bigEndian);
                enforce(box.length <= sizeof(box) + io_->size() - io_->tell(), Exiv2::kerCorruptedMetadata);

                if (bPrint) {
                    out << Internal::stringFormat("%8ld | %8ld | ", (size_t)(position - sizeof(box)),
                                                  (size_t)box.length)
                        << toAscii(box.type) << "      | ";
                    bLF = true;
                    if (box.type == kJp2BoxTypeClose)
                        lf(out, bLF);
                }
                if (box.type == kJp2BoxTypeClose)
                    break;

                switch (box.type) {
                    case kJp2BoxTypeSignature: {
                        if (boxSignatureFound)  // Only one is allowed
                            throw Error(kerCorruptedMetadata);
                        boxSignatureFound = true;
                        break;
                    }
                    case kJp2BoxTypeFileTypeBox: {
                        // This box shall immediately follow the JPEG 2000 Signature box
                        /// \todo  All files shall contain one and only one File Type box.
                        std::vector<byte> boxData(box.length - boxSize);
                        io_->read(boxData.data(), static_cast<long>(boxData.size()));
                        if (!Internal::isValidBoxFileType(boxData))
                            throw Error(kerCorruptedMetadata);
                        break;
                    }
                    case kJp2BoxTypeHeader: {
                        lf(out, bLF);

                        while (io_->read((byte*)&subBox, sizeof(subBox)) == sizeof(subBox) &&
                               io_->tell() < position + (long)box.length)  // don't read beyond the box!
                        {
                            const size_t address = io_->tell() - sizeof(subBox);
                            subBox.length = getLong((byte*)&subBox.length, bigEndian);
                            subBox.type = getLong((byte*)&subBox.type, bigEndian);

                            if (subBox.length < sizeof(box) || subBox.length > io_->size() - io_->tell()) {
                                throw Error(kerCorruptedMetadata);
                            }

                            DataBuf data(subBox.length - sizeof(box));
                            io_->read(data.pData_, data.size_);
                            if (bPrint) {
                                out << Internal::stringFormat("%8ld | %8ld |  sub:", (size_t)address,
                                                              (size_t)subBox.length)
                                    << toAscii(subBox.type) << " | "
                                    << Internal::binaryToString(makeSlice(data, 0, std::min(30l, data.size_)));
                                bLF = true;
                            }

                            if (subBox.type == kJp2BoxTypeImageHeader) {
                                enforce(subBox.length == 22, kerCorruptedMetadata);
                                // height (4), width (4), componentsCount (2), bpc (1)
                                uint8_t compressionType = data.pData_[11];
                                uint8_t unkC = data.pData_[12];
                                uint8_t ipr = data.pData_[13];
                                if (compressionType != 7 || unkC > 1 || ipr > 1) {
                                    throw Error(kerCorruptedMetadata);
                                }
                            }
                            if (subBox.type == kJp2BoxTypeColorSpec) {
                                long pad = 3;  // don't know why there are 3 padding bytes

                                // Bounds-check for the `getULong()` below, which reads 4 bytes, starting at `pad`.
                                enforce(data.size_ >= pad + 4, kerCorruptedMetadata);

                                /// \todo A conforming JP2 reader shall ignore all Colour Specification boxes after the
                                /// first.
                                uint8_t METH = data.pData_[0];
                                //                                auto PREC = data.read_uint8(1);
                                //                                auto APPROX = data.read_uint8(2);
                                if (METH == 1) {  // Enumerated Colourspace
                                    const uint32_t enumCS = getULong(data.pData_ + 3, bigEndian);
                                    if (enumCS != 16 && enumCS != 17) {
                                        throw Error(kerCorruptedMetadata);
                                    }
                                } else {  // Restricted ICC Profile
                                    // see the ICC Profile Format Specification, version ICC.1:1998-09
                                    const long iccLength = (long)getULong(data.pData_ + pad, bigEndian);
                                    if (bPrint) {
                                        out << " | iccLength:" << iccLength;
                                    }
                                    enforce(iccLength <= data.size_ - pad, kerCorruptedMetadata);
                                    if (bICC) {
                                        out.write((const char*)data.pData_ + pad, iccLength);
                                    }
                                }
                            }
                            lf(out, bLF);
                        }
                    } break;

                    case kJp2BoxTypeUuid: {
                        if (io_->read((byte*)&uuid, sizeof(uuid)) == sizeof(uuid)) {
                            bool bIsExif = memcmp(uuid.uuid, kJp2UuidExif, sizeof(uuid)) == 0;
                            bool bIsIPTC = memcmp(uuid.uuid, kJp2UuidIptc, sizeof(uuid)) == 0;
                            bool bIsXMP = memcmp(uuid.uuid, kJp2UuidXmp, sizeof(uuid)) == 0;

                            bool bUnknown = !(bIsExif || bIsIPTC || bIsXMP);

                            if (bPrint) {
                                if (bIsExif)
                                    out << "Exif: ";
                                if (bIsIPTC)
                                    out << "IPTC: ";
                                if (bIsXMP)
                                    out << "XMP : ";
                                if (bUnknown)
                                    out << "????: ";
                            }

                            DataBuf rawData;
                            enforce(box.length >= sizeof(uuid) + sizeof(box), kerCorruptedMetadata);
                            rawData.alloc(box.length - sizeof(uuid) - sizeof(box));
                            long bufRead = io_->read(rawData.pData_, rawData.size_);
                            if (io_->error())
                                throw Error(kerFailedToReadImageData);
                            if (bufRead != rawData.size_)
                                throw Error(kerInputDataReadFailed);

                            if (bPrint) {
                                out << Internal::binaryToString(
                                    makeSlice(rawData, 0, rawData.size_ > 40 ? 40 : rawData.size_));
                                out.flush();
                            }
                            lf(out, bLF);

                            if (bIsExif && bRecursive && rawData.size_ > 8) {  // "II*\0long"
                                if ((rawData.pData_[0] == rawData.pData_[1]) &&
                                    (rawData.pData_[0] == 'I' || rawData.pData_[0] == 'M')) {
                                    BasicIo::AutoPtr p = BasicIo::AutoPtr(new MemIo(rawData.pData_, rawData.size_));
                                    printTiffStructure(*p, out, option, depth);
                                }
                            }

                            if (bIsIPTC && bRecursive) {
                                IptcData::printStructure(out, makeSlice(rawData.pData_, 0, rawData.size_), depth);
                            }

                            if (bIsXMP && bXMP) {
                                out.write((const char*)rawData.pData_, rawData.size_);
                            }
                        }
                    } break;

                    default:
                        break;
                }

                // Move to the next box.
                io_->seek(static_cast<long>(position - sizeof(box) + box.length), BasicIo::beg);
                if (io_->error())
                    throw Error(kerFailedToReadImageData);
                if (bPrint)
                    lf(out, bLF);
            }
        }
    }  // JpegBase::printStructure

    void Jp2Image::writeMetadata()
    {
        if (io_->open() != 0) {
            throw Error(kerDataSourceOpenFailed, io_->path(), strError());
        }
        IoCloser closer(*io_);
        BasicIo::AutoPtr tempIo(new MemIo);
        assert(tempIo.get() != 0);

        doWriteMetadata(*tempIo);  // may throw
        io_->close();
        io_->transfer(*tempIo);  // may throw

    }  // Jp2Image::writeMetadata

#ifdef __clang__
// ignore cast align errors.  dataBuf.pData_ is allocated by malloc() and 4 (or 8 byte aligned).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
#endif

    void Jp2Image::encodeJp2Header(const DataBuf& boxBuf, DataBuf& outBuf)
    {
        DataBuf output(boxBuf.size_ + iccProfile_.size_ + 100);  // allocate sufficient space
        long outlen = sizeof(Internal::Jp2BoxHeader);            // now many bytes have we written to output?
        long inlen = sizeof(Internal::Jp2BoxHeader);             // how many bytes have we read from boxBuf?
        enforce(sizeof(Internal::Jp2BoxHeader) <= static_cast<size_t>(output.size_), Exiv2::kerCorruptedMetadata);
        Internal::Jp2BoxHeader* pBox = (Internal::Jp2BoxHeader*)boxBuf.pData_;
        uint32_t length = getLong((byte*)&pBox->length, bigEndian);

        enforce(length <= static_cast<size_t>(output.size_), Exiv2::kerCorruptedMetadata);
        uint32_t count = sizeof(Internal::Jp2BoxHeader);
        char* p = (char*)boxBuf.pData_;
        bool bWroteColor = false;

        while (count < length && !bWroteColor) {
            enforce(sizeof(Internal::Jp2BoxHeader) <= length - count, Exiv2::kerCorruptedMetadata);
            Internal::Jp2BoxHeader* pSubBox = (Internal::Jp2BoxHeader*)(p + count);

            // copy data.  pointer could be into a memory mapped file which we will decode!
            // pSubBox isn't always an aligned pointer, so use memcpy to do the copy.
            Internal::Jp2BoxHeader subBox;
            memcpy(&subBox, pSubBox, sizeof(Internal::Jp2BoxHeader));
            Internal::Jp2BoxHeader newBox = subBox;

            if (count < length) {
                subBox.length = getLong((byte*)&subBox.length, bigEndian);
                subBox.type = getLong((byte*)&subBox.type, bigEndian);
#ifdef EXIV2_DEBUG_MESSAGES
                std::cout << "Jp2Image::encodeJp2Header subbox: " << toAscii(subBox.type)
                          << " length = " << subBox.length << std::endl;
#endif
                enforce(subBox.length > 0, Exiv2::kerCorruptedMetadata);
                enforce(subBox.length <= length - count, Exiv2::kerCorruptedMetadata);
                count += subBox.length;
                newBox.type = subBox.type;
            } else {
                subBox.length = 0;
                newBox.type = kJp2BoxTypeColorSpec;
                count = length;
            }

            uint32_t newlen = subBox.length;
            if (newBox.type == kJp2BoxTypeColorSpec) {
                bWroteColor = true;
                if (!iccProfileDefined()) {
                    const char* pad = "\x01\x00\x00\x00\x00\x00\x10\x00\x00\x05\x1cuuid";
                    uint32_t psize = 15;
                    enforce(newlen <= static_cast<size_t>(output.size_ - outlen), Exiv2::kerCorruptedMetadata);
                    ul2Data((byte*)&newBox.length, psize, bigEndian);
                    ul2Data((byte*)&newBox.type, newBox.type, bigEndian);
                    ::memcpy(output.pData_ + outlen, &newBox, sizeof(newBox));
                    ::memcpy(output.pData_ + outlen + sizeof(newBox), pad, psize);
                } else {
                    const char* pad = "\x02\x00\x00";
                    uint32_t psize = 3;
                    newlen = sizeof(newBox) + psize + iccProfile_.size_;
                    enforce(newlen <= static_cast<size_t>(output.size_ - outlen), Exiv2::kerCorruptedMetadata);
                    ul2Data((byte*)&newBox.length, newlen, bigEndian);
                    ul2Data((byte*)&newBox.type, newBox.type, bigEndian);
                    ::memcpy(output.pData_ + outlen, &newBox, sizeof(newBox));
                    ::memcpy(output.pData_ + outlen + sizeof(newBox), pad, psize);
                    ::memcpy(output.pData_ + outlen + sizeof(newBox) + psize, iccProfile_.pData_, iccProfile_.size_);
                }
            } else {
                enforce(newlen <= static_cast<size_t>(output.size_ - outlen), Exiv2::kerCorruptedMetadata);
                ::memcpy(output.pData_ + outlen, boxBuf.pData_ + inlen, subBox.length);
            }

            outlen += newlen;
            inlen += subBox.length;
        }

        // allocate the correct number of bytes, copy the data and update the box header
        outBuf.alloc(outlen);
        ::memcpy(outBuf.pData_, output.pData_, outlen);
        pBox = (Internal::Jp2BoxHeader*)outBuf.pData_;
        ul2Data((byte*)&pBox->type, kJp2BoxTypeHeader, bigEndian);
        ul2Data((byte*)&pBox->length, outlen, bigEndian);
    }  // Jp2Image::encodeJp2Header

#ifdef __clang__
#pragma clang diagnostic pop
#endif

    void Jp2Image::doWriteMetadata(BasicIo& outIo)
    {
        if (!io_->isopen())
            throw Error(kerInputDataReadFailed);
        if (!outIo.isopen())
            throw Error(kerImageWriteFailed);

#ifdef EXIV2_DEBUG_MESSAGES
        std::cout << "Exiv2::Jp2Image::doWriteMetadata: Writing JPEG-2000 file " << io_->path() << std::endl;
        std::cout << "Exiv2::Jp2Image::doWriteMetadata: tmp file created " << outIo.path() << std::endl;
#endif

        // Ensure that this is the correct image type
        if (!isJp2Type(*io_, true)) {
            if (io_->error() || io_->eof())
                throw Error(kerInputDataReadFailed);
            throw Error(kerNoImageInInputData);
        }

        // Write JPEG2000 Signature.
        if (outIo.write(Jp2Signature, 12) != 12)
            throw Error(kerImageWriteFailed);

        Internal::Jp2BoxHeader box = {0, 0};

        byte boxDataSize[4];
        byte boxUUIDtype[4];
        DataBuf bheaderBuf(8);  // Box header : 4 bytes (data size) + 4 bytes (box type).

        // FIXME: Andreas, why the loop do not stop when EOF is taken from _io. The loop go out by an exception
        // generated by a zero size data read.

        while (io_->tell() < (long)io_->size()) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cout << "Exiv2::Jp2Image::doWriteMetadata: Position: " << io_->tell() << " / " << io_->size()
                      << std::endl;
#endif

            // Read chunk header.

            std::memset(bheaderBuf.pData_, 0x00, bheaderBuf.size_);
            long bufRead = io_->read(bheaderBuf.pData_, bheaderBuf.size_);
            if (io_->error())
                throw Error(kerFailedToReadImageData);
            if (bufRead != bheaderBuf.size_)
                throw Error(kerInputDataReadFailed);

            // Decode box header.

            box.length = getLong(bheaderBuf.pData_, bigEndian);
            box.type = getLong(bheaderBuf.pData_ + 4, bigEndian);

#ifdef EXIV2_DEBUG_MESSAGES
            std::cout << "Exiv2::Jp2Image::doWriteMetadata: box type: " << toAscii(box.type)
                      << " length: " << box.length << std::endl;
#endif

            if (box.length == 0) {
#ifdef EXIV2_DEBUG_MESSAGES
                std::cout << "Exiv2::Jp2Image::doWriteMetadata: Null Box size has been found. "
                             "This is the last box of file."
                          << std::endl;
#endif
                box.length = (uint32_t)(io_->size() - io_->tell() + 8);
            }
            if (box.length < 8) {
                // box is broken, so there is nothing we can do here
                throw Error(kerCorruptedMetadata);
            }

            // Prevent a malicious file from causing a large memory allocation.
            enforce(box.length - 8 <= static_cast<size_t>(io_->size() - io_->tell()), kerCorruptedMetadata);

            // Read whole box : Box header + Box data (not fixed size - can be null).
            DataBuf boxBuf(box.length);                              // Box header (8 bytes) + box data.
            memcpy(boxBuf.pData_, bheaderBuf.pData_, 8);             // Copy header.
            bufRead = io_->read(boxBuf.pData_ + 8, box.length - 8);  // Extract box data.
            if (io_->error()) {
#ifdef EXIV2_DEBUG_MESSAGES
                std::cout << "Exiv2::Jp2Image::doWriteMetadata: Error reading source file" << std::endl;
#endif

                throw Error(kerFailedToReadImageData);
            }

            if (bufRead != (long)(box.length - 8)) {
#ifdef EXIV2_DEBUG_MESSAGES
                std::cout << "Exiv2::Jp2Image::doWriteMetadata: Cannot read source file data" << std::endl;
#endif
                throw Error(kerInputDataReadFailed);
            }

            switch (box.type) {
                case kJp2BoxTypeHeader: {
                    DataBuf newBuf;
                    encodeJp2Header(boxBuf, newBuf);
#ifdef EXIV2_DEBUG_MESSAGES
                    std::cout << "Exiv2::Jp2Image::doWriteMetadata: Write JP2Header box (length: " << box.length << ")"
                              << std::endl;
#endif
                    if (outIo.write(newBuf.pData_, newBuf.size_) != newBuf.size_)
                        throw Error(kerImageWriteFailed);

                    // Write all updated metadata here, just after JP2Header.

                    if (exifData_.count() > 0) {
                        // Update Exif data to a new UUID box

                        Blob blob;
                        ExifParser::encode(blob, littleEndian, exifData_);
                        if (blob.size()) {
                            DataBuf rawExif(static_cast<long>(blob.size()));
                            memcpy(rawExif.pData_, &blob[0], blob.size());

                            DataBuf boxData(8 + 16 + rawExif.size_);
                            ul2Data(boxDataSize, boxData.size_, Exiv2::bigEndian);
                            ul2Data(boxUUIDtype, kJp2BoxTypeUuid, Exiv2::bigEndian);
                            memcpy(boxData.pData_, boxDataSize, 4);
                            memcpy(boxData.pData_ + 4, boxUUIDtype, 4);
                            memcpy(boxData.pData_ + 8, kJp2UuidExif, 16);
                            memcpy(boxData.pData_ + 8 + 16, rawExif.pData_, rawExif.size_);

#ifdef EXIV2_DEBUG_MESSAGES
                            std::cout << "Exiv2::Jp2Image::doWriteMetadata: Write box with Exif metadata (length: "
                                      << boxData.size_ << std::endl;
#endif
                            if (outIo.write(boxData.pData_, boxData.size_) != boxData.size_)
                                throw Error(kerImageWriteFailed);
                        }
                    }

                    if (iptcData_.count() > 0) {
                        // Update Iptc data to a new UUID box

                        DataBuf rawIptc = IptcParser::encode(iptcData_);
                        if (rawIptc.size_ > 0) {
                            DataBuf boxData(8 + 16 + rawIptc.size_);
                            ul2Data(boxDataSize, boxData.size_, Exiv2::bigEndian);
                            ul2Data(boxUUIDtype, kJp2BoxTypeUuid, Exiv2::bigEndian);
                            memcpy(boxData.pData_, boxDataSize, 4);
                            memcpy(boxData.pData_ + 4, boxUUIDtype, 4);
                            memcpy(boxData.pData_ + 8, kJp2UuidIptc, 16);
                            memcpy(boxData.pData_ + 8 + 16, rawIptc.pData_, rawIptc.size_);

#ifdef EXIV2_DEBUG_MESSAGES
                            std::cout << "Exiv2::Jp2Image::doWriteMetadata: Write box with Iptc metadata (length: "
                                      << boxData.size_ << std::endl;
#endif
                            if (outIo.write(boxData.pData_, boxData.size_) != boxData.size_)
                                throw Error(kerImageWriteFailed);
                        }
                    }

                    if (writeXmpFromPacket() == false) {
                        if (XmpParser::encode(xmpPacket_, xmpData_) > 1) {
#ifndef SUPPRESS_WARNINGS
                            EXV_ERROR << "Failed to encode XMP metadata." << std::endl;
#endif
                        }
                    }
                    if (xmpPacket_.size() > 0) {
                        // Update Xmp data to a new UUID box

                        DataBuf xmp(reinterpret_cast<const byte*>(xmpPacket_.data()),
                                    static_cast<long>(xmpPacket_.size()));
                        DataBuf boxData(8 + 16 + xmp.size_);
                        ul2Data(boxDataSize, boxData.size_, Exiv2::bigEndian);
                        ul2Data(boxUUIDtype, kJp2BoxTypeUuid, Exiv2::bigEndian);
                        memcpy(boxData.pData_, boxDataSize, 4);
                        memcpy(boxData.pData_ + 4, boxUUIDtype, 4);
                        memcpy(boxData.pData_ + 8, kJp2UuidXmp, 16);
                        memcpy(boxData.pData_ + 8 + 16, xmp.pData_, xmp.size_);

#ifdef EXIV2_DEBUG_MESSAGES
                        std::cout << "Exiv2::Jp2Image::doWriteMetadata: Write box with XMP metadata (length: "
                                  << boxData.size_ << ")" << std::endl;
#endif
                        if (outIo.write(boxData.pData_, boxData.size_) != boxData.size_)
                            throw Error(kerImageWriteFailed);
                    }

                    break;
                }

                case kJp2BoxTypeUuid: {
                    enforce(boxBuf.size_ >= 24, Exiv2::kerCorruptedMetadata);
                    if (memcmp(boxBuf.pData_ + 8, kJp2UuidExif, 16) == 0) {
#ifdef EXIV2_DEBUG_MESSAGES
                        std::cout << "Exiv2::Jp2Image::doWriteMetadata: strip Exif Uuid box" << std::endl;
#endif
                    } else if (memcmp(boxBuf.pData_ + 8, kJp2UuidIptc, 16) == 0) {
#ifdef EXIV2_DEBUG_MESSAGES
                        std::cout << "Exiv2::Jp2Image::doWriteMetadata: strip Iptc Uuid box" << std::endl;
#endif
                    } else if (memcmp(boxBuf.pData_ + 8, kJp2UuidXmp, 16) == 0) {
#ifdef EXIV2_DEBUG_MESSAGES
                        std::cout << "Exiv2::Jp2Image::doWriteMetadata: strip Xmp Uuid box" << std::endl;
#endif
                    } else {
#ifdef EXIV2_DEBUG_MESSAGES
                        std::cout << "Exiv2::Jp2Image::doWriteMetadata: write Uuid box (length: " << box.length << ")"
                                  << std::endl;
#endif
                        if (outIo.write(boxBuf.pData_, boxBuf.size_) != boxBuf.size_)
                            throw Error(kerImageWriteFailed);
                    }
                    break;
                }

                default: {
#ifdef EXIV2_DEBUG_MESSAGES
                    std::cout << "Exiv2::Jp2Image::doWriteMetadata: write box (length: " << box.length << ")"
                              << std::endl;
#endif
                    if (outIo.write(boxBuf.pData_, boxBuf.size_) != boxBuf.size_)
                        throw Error(kerImageWriteFailed);

                    break;
                }
            }
        }

#ifdef EXIV2_DEBUG_MESSAGES
        std::cout << "Exiv2::Jp2Image::doWriteMetadata: EOF" << std::endl;
#endif

    }  // Jp2Image::doWriteMetadata

    // *************************************************************************
    // free functions
    Image::AutoPtr newJp2Instance(BasicIo::AutoPtr io, bool create)
    {
        Image::AutoPtr image(new Jp2Image(io, create));
        if (!image->good()) {
            image.reset();
        }
        return image;
    }

    bool isJp2Type(BasicIo& iIo, bool advance)
    {
        const int32_t len = 12;
        byte buf[len];
        iIo.read(buf, len);
        if (iIo.error() || iIo.eof()) {
            return false;
        }
        bool matched = (memcmp(buf, Jp2Signature, len) == 0);
        if (!advance || !matched) {
            iIo.seek(-len, BasicIo::cur);
        }
        return matched;
    }
}  // namespace Exiv2
