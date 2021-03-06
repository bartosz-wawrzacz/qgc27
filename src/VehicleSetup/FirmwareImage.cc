/*=====================================================================
 
 QGroundControl Open Source Ground Control Station
 
 (c) 2009, 2015 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 
 This file is part of the QGROUNDCONTROL project
 
 QGROUNDCONTROL is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 QGROUNDCONTROL is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with QGROUNDCONTROL. If not, see <http://www.gnu.org/licenses/>.
 
 ======================================================================*/

/// @file
///     @brief Support for Intel Hex firmware file
///     @author Don Gagne <don@thegagnes.com>

#include "FirmwareImage.h"
#include "QGCLoggingCategory.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QFileInfo>
#include <QDir>

FirmwareImage::FirmwareImage(QObject* parent) :
    QObject(parent),
    _imageSize(0)
{
    
}

bool FirmwareImage::load(const QString& imageFilename, uint32_t boardId)
{
    _imageSize = 0;
    _boardId = boardId;
    
    if (imageFilename.endsWith(".bin")) {
        return _binLoad(imageFilename);
        _binFormat = true;
        return true;
    } else if (imageFilename.endsWith(".px4")) {
        _binFormat = true;
        return _px4Load(imageFilename);
    } else if (imageFilename.endsWith(".ihx")) {
        _binFormat = false;
        return _ihxLoad(imageFilename);
    } else {
        emit errorMessage("Unsupported file format");
        return false;
    }
}

bool FirmwareImage::_readByteFromStream(QTextStream& stream, uint8_t& byte)
{
    QString hex = stream.read(2);
    
    if (hex.count() != 2) {
        return false;
    }
    
    bool success;
    byte = (uint8_t)hex.toInt(&success, 16);
    
    return success;
}

bool FirmwareImage::_readWordFromStream(QTextStream& stream, uint16_t& word)
{
    QString hex = stream.read(4);
    
    if (hex.count() != 4) {
        return false;
    }
    
    bool success;
    word = (uint16_t)hex.toInt(&success, 16);
    
    return success;
}

bool FirmwareImage::_readBytesFromStream(QTextStream& stream, uint8_t byteCount, QByteArray& bytes)
{
    bytes.clear();
    
    while (byteCount) {
        uint8_t byte;
        
        if (!_readByteFromStream(stream, byte)) {
            return false;
        }
        bytes += byte;
        
        byteCount--;
    }
    
    return true;
}

bool FirmwareImage::_ihxLoad(const QString& ihxFilename)
{
    _imageSize = 0;
    _ihxBlocks.clear();
    
    QFile ihxFile(ihxFilename);
    if (!ihxFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorMessage(QString("Unable to open firmware file %1, error: %2").arg(ihxFilename).arg(ihxFile.errorString()));
        return false;
    }
    
    QTextStream stream(&ihxFile);
    
    while (true) {
        if (stream.read(1) != ":") {
            emit errorMessage("Incorrectly formatted .ihx file, line does not begin with :");
            return false;
        }
        
        uint8_t     blockByteCount;
        uint16_t    address;
        uint8_t     recordType;
        QByteArray  bytes;
        uint8_t     crc;
        
        if (!_readByteFromStream(stream, blockByteCount) ||
            !_readWordFromStream(stream, address) ||
            !_readByteFromStream(stream, recordType) ||
            !_readBytesFromStream(stream, blockByteCount, bytes) ||
            !_readByteFromStream(stream, crc)) {
            emit errorMessage("Incorrectly formatted line in .ihx file, line too short");
            return false;
        }
        
        if (!(recordType == 0 || recordType == 1)) {
            emit errorMessage(QString("Unsupported record type in file: %1").arg(recordType));
            return false;
        }
        
        if (recordType == 0) {
            bool appendToLastBlock = false;
            
            // Can we append this block to the last one?
            
            if (_ihxBlocks.count()) {
                int lastBlockIndex = _ihxBlocks.count() - 1;
                
                if (_ihxBlocks[lastBlockIndex].address + _ihxBlocks[lastBlockIndex].bytes.count() == address) {
                    appendToLastBlock = true;
                }
            }
            
            if (appendToLastBlock) {
                _ihxBlocks[_ihxBlocks.count() - 1].bytes += bytes;
                // Too noisy even for verbose
                //qCDebug(FirmwareUpgradeVerboseLog) << QString("_ihxLoad - append - address:%1 size:%2 block:%3").arg(address).arg(blockByteCount).arg(ihxBlockCount());
            } else {
                IntelHexBlock_t block;
                
                block.address = address;
                block.bytes = bytes;
                
                _ihxBlocks += block;
                qCDebug(FirmwareUpgradeVerboseLog) << QString("_ihxLoad - new block - address:%1 size:%2 block:%3").arg(address).arg(blockByteCount).arg(ihxBlockCount());
            }
            
            _imageSize += blockByteCount;
        } else if (recordType == 1) {
            // EOF
            qCDebug(FirmwareUpgradeLog) << QString("_ihxLoad - EOF");
            break;
        }
        
        // Move to next line
        stream.readLine();
    }
    
    ihxFile.close();
    
    return true;
}

bool FirmwareImage::_px4Load(const QString& imageFilename)
{
    _imageSize = 0;
    
    // We need to collect information from the .px4 file as well as pull the binary image out to a seperate file.
    
    QFile px4File(imageFilename);
    if (!px4File.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorMessage(QString("Unable to open firmware file %1, error: %2").arg(imageFilename).arg(px4File.errorString()));
        return false;
    }
    
    QByteArray bytes = px4File.readAll();
    px4File.close();
    QJsonDocument doc = QJsonDocument::fromJson(bytes);
    
    if (doc.isNull()) {
        emit errorMessage("Supplied file is not a valid JSON document");
        return false;
    }
    
    QJsonObject px4Json = doc.object();
    
    // Make sure the keys we need are available
    static const char* rgJsonKeys[] = { "board_id", "image_size", "description", "git_identity" };
    for (size_t i=0; i<sizeof(rgJsonKeys)/sizeof(rgJsonKeys[0]); i++) {
        if (!px4Json.contains(rgJsonKeys[i])) {
            emit errorMessage(QString("Incorrectly formatted firmware file. No %1 key.").arg(rgJsonKeys[i]));
            return false;
        }
    }
    
    uint32_t firmwareBoardId = (uint32_t)px4Json.value(QString("board_id")).toInt();
    if (firmwareBoardId != _boardId) {
        emit errorMessage(QString("Downloaded firmware board id does not match hardware board id: %1 != %2").arg(firmwareBoardId).arg(_boardId));
        return false;
    }
    
    // Decompress the parameter xml and save to file
    QByteArray decompressedBytes;
    bool success = _decompressJsonValue(px4Json,               // JSON object
                                        bytes,                 // Raw bytes of JSON document
                                        "parameter_xml_size",  // key which holds byte size
                                        "parameter_xml",       // key which holds compress bytes
                                        decompressedBytes);    // Returned decompressed bytes
    if (success) {
        // We cache the parameter xml in the same location as settings
        QSettings settings;
        QDir parameterDir = QFileInfo(settings.fileName()).dir();
        QString parameterFilename = parameterDir.filePath("PX4ParameterFactMetaData.xml");
        QFile parameterFile(parameterFilename);

        if (parameterFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qint64 bytesWritten = parameterFile.write(decompressedBytes);
            if (bytesWritten != decompressedBytes.count()) {
                // FIXME: What about these warnings?
                emit statusMessage(QString("Write failed for parameter meta data file, error: %1").arg(parameterFile.errorString()));
                parameterFile.close();
                QFile::remove(parameterFilename);
            } else {
                parameterFile.close();
            }
        } else {
            emit statusMessage(QString("Unable to open parameter meta data file %1 for writing, error: %2").arg(parameterFilename).arg(parameterFile.errorString()));
        }
    }

    // Decompress the airframe xml and save to file
    success = _decompressJsonValue(px4Json,               // JSON object
                                        bytes,                 // Raw bytes of JSON document
                                        "airframe_xml_size",  // key which holds byte size
                                        "airframe_xml",       // key which holds compress bytes
                                        decompressedBytes);    // Returned decompressed bytes
    if (success) {
        // We cache the airframe xml in the same location as settings and parameters
        QSettings settings;
        QDir airframeDir = QFileInfo(settings.fileName()).dir();
        QString airframeFilename = airframeDir.filePath("PX4AirframeFactMetaData.xml");
        //qDebug() << airframeFilename;
        QFile airframeFile(airframeFilename);

        if (airframeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qint64 bytesWritten = airframeFile.write(decompressedBytes);
            if (bytesWritten != decompressedBytes.count()) {
                // FIXME: What about these warnings?
                emit statusMessage(QString("Write failed for airframe meta data file, error: %1").arg(airframeFile.errorString()));
                airframeFile.close();
                QFile::remove(airframeFilename);
            } else {
                airframeFile.close();
            }
        } else {
            emit statusMessage(QString("Unable to open airframe meta data file %1 for writing, error: %2").arg(airframeFilename).arg(airframeFile.errorString()));
        }
    }
    
    // Decompress the image and save to file
    _imageSize = px4Json.value(QString("image_size")).toInt();
    success = _decompressJsonValue(px4Json,               // JSON object
                                   bytes,                 // Raw bytes of JSON document
                                   "image_size",          // key which holds byte size
                                   "image",               // key which holds compress bytes
                                   decompressedBytes);    // Returned decompressed bytes
    if (!success) {
        return false;
    }
    
    // Pad image to 4-byte boundary
    while ((decompressedBytes.count() % 4) != 0) {
        decompressedBytes.append(static_cast<char>(static_cast<unsigned char>(0xFF)));
    }
    
    // Store decompressed image file in same location as original download file
    QDir imageDir = QFileInfo(imageFilename).dir();
    QString decompressFilename = imageDir.filePath("PX4FlashUpgrade.bin");
    
    QFile decompressFile(decompressFilename);
    if (!decompressFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorMessage(QString("Unable to open decompressed file %1 for writing, error: %2").arg(decompressFilename).arg(decompressFile.errorString()));
        return false;
    }
    
    qint64 bytesWritten = decompressFile.write(decompressedBytes);
    if (bytesWritten != decompressedBytes.count()) {
        emit errorMessage(QString("Write failed for decompressed image file, error: %1").arg(decompressFile.errorString()));
        return false;
    }
    decompressFile.close();
    
    _binFilename = decompressFilename;
    
    return true;
}

/// Decompress a set of bytes stored in a Json document.
bool FirmwareImage::_decompressJsonValue(const QJsonObject&	jsonObject,			///< JSON object
                                         const QByteArray&	jsonDocBytes,		///< Raw bytes of JSON document
                                         const QString&		sizeKey,			///< key which holds byte size
                                         const QString&		bytesKey,			///< key which holds compress bytes
                                         QByteArray&		decompressedBytes)	///< Returned decompressed bytes
{
    // Validate decompressed size key
    if (!jsonObject.contains(sizeKey)) {
        emit statusMessage(QString("Firmware file missing %1 key").arg(sizeKey));
        return false;
    }
    int decompressedSize = jsonObject.value(QString(sizeKey)).toInt();
    if (decompressedSize == 0) {
        emit errorMessage(QString("Firmware file has invalid decompressed size for %1").arg(sizeKey));
        return false;
    }
    
    // XXX Qt's JSON string handling is terribly broken, strings
    // with some length (18K / 25K) are just weirdly cut.
    // The code below works around this by manually 'parsing'
    // for the image string. Since its compressed / checksummed
    // this should be fine.
    
    QStringList parts = QString(jsonDocBytes).split(QString("\"%1\": \"").arg(bytesKey));
    if (parts.count() == 1) {
        emit errorMessage(QString("Could not find compressed bytes for %1 in Firmware file").arg(bytesKey));
        return false;
    }
    parts = parts.last().split("\"");
    if (parts.count() == 1) {
        emit errorMessage(QString("Incorrectly formed compressed bytes section for %1 in Firmware file").arg(bytesKey));
        return false;
    }
    
    // Store decompressed size as first four bytes. This is required by qUncompress routine.
    QByteArray raw;
    raw.append((unsigned char)((decompressedSize >> 24) & 0xFF));
    raw.append((unsigned char)((decompressedSize >> 16) & 0xFF));
    raw.append((unsigned char)((decompressedSize >> 8) & 0xFF));
    raw.append((unsigned char)((decompressedSize >> 0) & 0xFF));
    
    QByteArray raw64 = parts.first().toUtf8();
    raw.append(QByteArray::fromBase64(raw64));
    decompressedBytes = qUncompress(raw);
    
    if (decompressedBytes.count() == 0) {
        emit errorMessage(QString("Firmware file has 0 length %1").arg(bytesKey));
        return false;
    }
    if (decompressedBytes.count() != decompressedSize) {
        emit errorMessage(QString("Size for decompressed %1 does not match stored size: Expected(%1) Actual(%2)").arg(decompressedSize).arg(decompressedBytes.count()));
        return false;
    }
    
    emit statusMessage(QString("Succesfully decompressed %1").arg(bytesKey));
    
    return true;
}

uint16_t FirmwareImage::ihxBlockCount(void) const
{
    return _ihxBlocks.count();
}

bool FirmwareImage::ihxGetBlock(uint16_t index, uint16_t& address, QByteArray& bytes) const
{
    address = 0;
    bytes.clear();
    
    if (index < ihxBlockCount()) {
        address = _ihxBlocks[index].address;
        bytes = _ihxBlocks[index].bytes;
        return true;
    } else {
        return false;
    }
}

bool FirmwareImage::_binLoad(const QString& imageFilename)
{
    QFile binFile(imageFilename);
    if (!binFile.open(QIODevice::ReadOnly)) {
        emit errorMessage(QString("Unabled to open firmware file %1, %2").arg(imageFilename).arg(binFile.errorString()));
        return false;
    }
    
    _imageSize = (uint32_t)binFile.size();
    
    binFile.close();
    
    return true;
}
