/****************************************************************************
 *
 * This file is part of the ustk software.
 * Copyright (C) 2016 - 2017 by Inria. All rights reserved.
 *
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * ("GPL") version 2 as published by the Free Software Foundation.
 * See the file LICENSE.txt at the root directory of this source
 * distribution for additional information about the GNU GPL.
 *
 * For using ustk with software that can not be combined with the GNU
 * GPL, please contact Inria about acquiring a ViSP Professional
 * Edition License.
 *
 * This software was developed at:
 * Inria Rennes - Bretagne Atlantique
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * France
 *
 * If you have questions regarding the use of this file, please contact
 * Inria at ustk@inria.fr
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors:
 * Marc Pouliquen
 *
 *****************************************************************************/

#include "usVirtualServer.h"

/**
* Constructor, sets the sequence path to open.
* @param sequencePath The path to the sequence to replay : xml file for pre-scan 2D or post-scan 2D (using
* usSequenceReader), or directory containing mhd/raw files (using usMHDSequenceReader).
* @param parent The optionnal QObject parent.
*/
usVirtualServer::usVirtualServer(std::string sequencePath, QObject *parent)
  : QObject(parent), m_tcpServer(), m_serverIsSendingImages(false), m_consoleListener()
{
  m_usePause = false;
  m_pauseOn = false;
  m_volumePauseTmp = false;
  m_pauseDurationOffset = 0;
  m_pauseIndexOffset = 0;

  m_useRewind = false;

  if (qApp->arguments().contains(QString("--pause"))) {
    m_usePause = true;
    m_pauseImageNumber = qApp->arguments().at(qApp->arguments().indexOf(QString("--pause")) + 1).toInt();
    std::cout << "Pause activated on image N. : " << m_pauseImageNumber << "\n";
  }

  if (qApp->arguments().contains(QString("--rewind"))) {
    m_useRewind = true;
    std::cout << "Rewind option activated\n";
  }

  imageHeader.frameCount = 0;
  // read sequence parameters
  setSequencePath(sequencePath); // opens first image of the sequence

  // init TCP server
  // set : acceptTheConnection() will be called whenever there is a new connection
  connect(&m_tcpServer, SIGNAL(newConnection()), this, SLOT(acceptTheConnection()));

  // loop sending image activation / desactivation
  connect(this, SIGNAL(runAcquisitionSignal(bool)), this, SLOT(runAcquisition(bool)));
  connect(this, SIGNAL(startSendingLoopSignal()), this, SLOT(startSendingLoop()));

  // console input (user)
  connect(&m_consoleListener, SIGNAL(quitPause()), this, SLOT(quitPause()));

  // Start listening on port 8080
  QString portNum = QString::number(8080);
  bool status = m_tcpServer.listen(QHostAddress::Any, portNum.toUShort());

  // Check, if the server did start correctly or not
  if (status == true) {
    std::cout << "TCP server Started\nServer now listening on port# " << portNum.toStdString() << std::endl;
  } else {
    std::cout << "TCP server start failure" << m_tcpServer.errorString().toStdString() << std::endl;
  }
}

/**
* Destructor.
*/
usVirtualServer::~usVirtualServer() {}

/**
* Slot called when there is an incomming connection on the server.
*/
void usVirtualServer::acceptTheConnection()
{

  connectionSoc = m_tcpServer.nextPendingConnection();

  connect(connectionSoc, SIGNAL(readyRead()), this, SLOT(readIncomingData()));

  connect(connectionSoc, SIGNAL(disconnected()), this, SLOT(connectionAboutToClose()));
}

/**
* Slot called whenever the data (coming from client) is available.
*/
void usVirtualServer::readIncomingData()
{
  // headers possible to be received
  usVirtualServer::usInitHeaderIncomming headerInit;

  // reading part

  // prepare reading in QDataStream
  QDataStream in;
  in.setDevice(connectionSoc);
  //in.setVersion(QDataStream::Qt_5_0);
  in.setVersion(QDataStream::Qt_4_8);

  // read header id
  int id;
  in >> id;

  if (id == 1) { // init header
    in >> headerInit.probeId;
    in >> headerInit.slotId;
    in >> headerInit.imagingMode;

    // values by default for virtual server
    confirmHeader.initOk = 1;
    confirmHeader.probeId = 0;

    // send back default params
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    //out.setVersion(QDataStream::Qt_5_0);
    out.setVersion(QDataStream::Qt_4_8);

    out << confirmHeader.headerId;
    out << confirmHeader.initOk;
    out << confirmHeader.probeId;

    writeInitAcquisitionParameters(out, headerInit.imagingMode);

    connectionSoc->write(block);
  } else if (id == 2) { // update header
    throw(vpException(vpException::fatalError, "no update available for virtual server !"));
  } else if (id == 3) { // run - stop command
    bool run;
    in >> run;

    emit(runAcquisitionSignal(run));
  } else {
    std::cout << "ERROR : unknown data received !" << std::endl;
  }
}

/**
* Slot called whenever whenever the connection is closed by the client.
*/
void usVirtualServer::connectionAboutToClose()
{
  // Set this text into the label
  std::cout << "Connection to client closed" << std::endl;

  // Close the connection (Say bye)
  connectionSoc->close();

// delete sequcence reader and reset frame count (to prepare them for a potential new connection)
#ifdef VISP_HAVE_XML2
  m_sequenceReaderPostScan = usSequenceReader<usImagePostScan2D<unsigned char> >();
  m_sequenceReaderPreScan = usSequenceReader<usImagePreScan2D<unsigned char> >();
#endif
  imageHeader.frameCount = 0;

  // Re-open the sequence to prepare next connection incomming (the server is still running)
  setSequencePath(m_sequencePath);
}

/**
* Slot called whenever whenever the connection is closed by the client.
*/
void usVirtualServer::writeInitAcquisitionParameters(QDataStream &stream, int imagingMode)
{

  int transmitFrequency = 0;
  int samplingFrequency = 0;
  bool postScanMode = false;
  int postScanHeight = 0;
  int postScanWidth = 0;
  int imageDepth = 0;
  int sector = 100;
  bool activateMotor = false;
  int motorPosition = 0;
  int framesPerVolume = 1;
  int stepsPerFrame = 0;
  int transmitFrequencyMin = 0;
  int samplingFrequencyMin = 0;
  int imagingModeMin = 0;
  int imageDepthMin = 0;
  int sectorMin = 100;
  int motorPositionMin = 0;
  int framesPerVolumeMin = 1;
  int stepsPerFrameMin = 0;
  int transmitFrequencyMax = 0;
  int samplingFrequencyMax = 0;
  int imagingModeMax = 27;
  int imageDepthMax = 0;
  int sectorMax = 100;
  int motorPositionMax = 0;
  int framesPerVolumeMax = 0;
  int stepsPerFrameMax = 0;

  if (m_imageType == us::PRESCAN_2D) {
    transmitFrequency = m_preScanImage2d.getTransmitFrequency();
    transmitFrequencyMin = m_preScanImage2d.getTransmitFrequency();
    transmitFrequencyMax = m_preScanImage2d.getTransmitFrequency();

    samplingFrequency = m_preScanImage2d.getSamplingFrequency();
    samplingFrequencyMin = m_preScanImage2d.getSamplingFrequency();
    samplingFrequencyMax = m_preScanImage2d.getSamplingFrequency();

    imageDepth = m_preScanImage2d.getDepth();
    imageDepthMin = m_preScanImage2d.getDepth();
    imageDepthMax = m_preScanImage2d.getDepth();
  } else if (m_imageType == us::POSTSCAN_2D) {
    transmitFrequency = m_postScanImage2d.getTransmitFrequency();
    transmitFrequencyMin = m_postScanImage2d.getTransmitFrequency();
    transmitFrequencyMax = m_postScanImage2d.getTransmitFrequency();

    samplingFrequency = m_postScanImage2d.getSamplingFrequency();
    samplingFrequencyMin = m_postScanImage2d.getSamplingFrequency();
    samplingFrequencyMax = m_postScanImage2d.getSamplingFrequency();

    imageDepth = m_postScanImage2d.getDepth();
    imageDepthMin = m_postScanImage2d.getDepth();
    imageDepthMax = m_postScanImage2d.getDepth();

    postScanMode = true;
  }

  stream << transmitFrequency;
  stream << samplingFrequency;
  stream << imagingMode;
  stream << postScanMode;
  stream << postScanHeight;
  stream << postScanWidth;
  stream << imageDepth;
  stream << sector;
  stream << activateMotor;
  stream << motorPosition;
  stream << framesPerVolume;
  stream << stepsPerFrame;
  stream << transmitFrequencyMin;
  stream << samplingFrequencyMin;
  stream << imagingModeMin;
  stream << imageDepthMin;
  stream << sectorMin;
  stream << motorPositionMin;
  stream << framesPerVolumeMin;
  stream << stepsPerFrameMin;
  stream << transmitFrequencyMax;
  stream << samplingFrequencyMax;
  stream << imagingModeMax;
  stream << imageDepthMax;
  stream << sectorMax;
  stream << motorPositionMax;
  stream << framesPerVolumeMax;
  stream << stepsPerFrameMax;
}

/**
* Setter for sequence path : pointing on the sequence to replay.
* @param sequencePath The path to the sequence to replay : xml file for pre-scan 2D or post-scan 2D (@see
* usSequenceReader), or directory containing mhd/raw files (@see usMHDSequenceReader).
*/
void usVirtualServer::setSequencePath(const std::string sequencePath)
{

  if (vpIoTools::checkFilename(sequencePath)) { // xml file pointing on a sequence of 2d images (pre-scan or post-scan)

    m_sequencePath = sequencePath;
#ifdef VISP_HAVE_XML2
    m_sequenceReaderPostScan.setSequenceFileName(sequencePath);
    m_sequenceReaderPreScan.setSequenceFileName(sequencePath);
    // try to open post-scan sequence
    try {
      uint64_t timestampTmp;
      m_sequenceReaderPostScan.open(m_postScanImage2d, timestampTmp);
      imageHeader.timeStamp = timestampTmp;
      m_nextImageTimestamp = m_sequenceReaderPostScan.getSequenceTimestamps().at(imageHeader.frameCount + 1);
      if (imageHeader.timeStamp ==
          m_nextImageTimestamp) { // timestamps are supposed to be different for different frames
        throw(vpException(vpException::fatalError), "usVirtualServer error : successives timestamps are equal !");
      }
      m_imageType = us::POSTSCAN_2D;
      m_isMHDSequence = false;
    } catch (...) {
      // if we have an exception, it's not a post-scan image. So we try a pre-scan
      try {
        uint64_t timestampTmp;
        m_sequenceReaderPreScan.open(m_preScanImage2d, timestampTmp);
        imageHeader.timeStamp = timestampTmp;
        m_nextImageTimestamp = m_sequenceReaderPreScan.getSequenceTimestamps().at(imageHeader.frameCount + 1);
        if (imageHeader.timeStamp ==
            m_nextImageTimestamp) { // timestamps are supposed to be different for different frames
          throw(vpException(vpException::fatalError), "usVirtualServer error : successives timestamps are equal !");
        }
        invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
        m_imageType = us::PRESCAN_2D;
        m_isMHDSequence = false;
      } catch (...) {
        throw(vpException(vpException::badValue), "usVirtualServer error : trying to open a xml image sequence not "
                                                  "managed (neither pre-scan 2D nor post-scan 2D)or with no timestamp "
                                                  "associated");
      }
    }
#endif
  }

  // case of a directory containing a sequence of mhd/raw images
  else if (vpIoTools::checkDirectory(sequencePath) &&
           usImageIo::getHeaderFormat(vpIoTools::getDirFiles(sequencePath).front()) == usImageIo::FORMAT_MHD) {
    m_MHDSequenceReader.setSequenceDirectory(sequencePath);

    // at this point, we don't know the type of image contained in the sequence, we have to try them all
    try {
      uint64_t timestampTmp;
      m_MHDSequenceReader.acquire(m_rfImage2d, timestampTmp);
      imageHeader.timeStamp = timestampTmp;
      m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
      if (imageHeader.timeStamp == m_nextImageTimestamp) // timestamps are supposed to be different for different frames
        throw(vpException(vpException::fatalError), "usVirtualServer error : successives timestamps are equal !");
    } catch (...) { // if we have an exception, it's not a RF 2D image. So we try a pre-scan 2D
      try {
        uint64_t timestampTmp;
        m_MHDSequenceReader.acquire(m_preScanImage2d, timestampTmp);
        imageHeader.timeStamp = timestampTmp;
        m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
        if (imageHeader.timeStamp ==
            m_nextImageTimestamp) // timestamps are supposed to be different for different frames
          throw(vpException(vpException::fatalError), "usVirtualServer error : successives timestamps are equal !");

        invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
      } catch (...) {              // it's not a pre-scan 2D image...
        try {
          uint64_t timestampTmp;
          m_MHDSequenceReader.acquire(m_postScanImage2d, timestampTmp);
          imageHeader.timeStamp = timestampTmp;
          m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
          if (imageHeader.timeStamp ==
              m_nextImageTimestamp) // timestamps are supposed to be different for different frames
            throw(vpException(vpException::fatalError), "usVirtualServer error : successives timestamps are equal !");
        } catch (...) { // it's not a post-scan 2D image...
          try {
            m_timestamps.clear();
            m_MHDSequenceReader.acquire(m_rfImage3d, m_timestamps);
            imageHeader.timeStamp = m_timestamps.at(0);
            m_nextImageTimestamp = m_timestamps.at(1);
            if (imageHeader.timeStamp == m_nextImageTimestamp)
              throw(vpException(vpException::fatalError), "usVirtualServer error : successives timestamps are equal !");
          } catch (...) { // it's not a rf 3D image...
            try {
              m_timestamps.clear();
              m_MHDSequenceReader.acquire(m_preScanImage3d, m_timestamps);
              imageHeader.timeStamp = m_timestamps.at(0);
              m_nextImageTimestamp = m_timestamps.at(1);
              if (imageHeader.timeStamp == m_nextImageTimestamp)
                throw(vpException(vpException::fatalError),
                      "usVirtualServer error : successives timestamps are equal !");

              m_preScanImage3d.getFrame(m_preScanImage2d, 0);
              m_preScanImage2d.setImagePreScanSettings(m_preScanImage3d);
              invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
            } catch (...) {              // it's not a valid type
              throw(vpException(vpException::badValue), "usVirtualServer error : trying to open a mhd image sequence "
                                                        "not managed, or with no timestamps associated to sequence "
                                                        "frames !");
            }
          }
        }
      }
    }
    m_imageType = m_MHDSequenceReader.getImageType();
    m_isMHDSequence = true;
  } else {
    throw(vpException(vpException::badValue, "usVirtualServer error : sequence path incorrect !"));
  }
}

/**
* Slot called when the clients asks to start the acquisition.
*/
void usVirtualServer::startSendingLoop()
{
  if (m_isMHDSequence)
    sendingLoopSequenceMHD();
  else
    sendingLoopSequenceXml();
}

/**
* Method to send the image sequence through the network in case of xml sequence as input.
*/
void usVirtualServer::sendingLoopSequenceXml()
{
#ifdef VISP_HAVE_XML2
  bool endOfSequence = false;
  while (m_serverIsSendingImages && !endOfSequence) {
    // manage first frame sent (already aquired with open() method)
    if (imageHeader.frameCount != 0) {
      if (m_imageType == us::PRESCAN_2D) {
        if (m_usePause) {
          if (!m_pauseOn) {
            uint64_t localTimestamp;
            m_sequenceReaderPreScan.acquire(m_preScanImage2d, localTimestamp);
            invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
            imageHeader.timeStamp = localTimestamp + m_pauseDurationOffset;
            if (m_sequenceReaderPreScan.getSequenceTimestamps().size() >
                imageHeader.frameCount + 1 - m_pauseIndexOffset)
              m_nextImageTimestamp =
                  m_sequenceReaderPreScan.getSequenceTimestamps().at(imageHeader.frameCount + 1 - m_pauseIndexOffset) +
                  m_pauseDurationOffset;
            imageHeader.imageType = 0;
          } else { // pause activated, we continue sending the same image, and increasing timestamps
            uint64_t deltaT = m_nextImageTimestamp - imageHeader.timeStamp;
            m_nextImageTimestamp += deltaT;
            imageHeader.timeStamp += deltaT;
            m_pauseDurationOffset += deltaT;
            m_pauseIndexOffset++;
          }

          if (imageHeader.frameCount == m_pauseImageNumber)
            m_pauseOn = true;
        } else {
          uint64_t localTimestamp;
          m_sequenceReaderPreScan.acquire(m_preScanImage2d, localTimestamp);
          invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
          imageHeader.timeStamp = localTimestamp;
          if (m_sequenceReaderPreScan.getSequenceTimestamps().size() > imageHeader.frameCount + 1)
            m_nextImageTimestamp = m_sequenceReaderPreScan.getSequenceTimestamps().at(imageHeader.frameCount + 1);
          imageHeader.imageType = 0;
        }
      } else if (m_imageType == us::POSTSCAN_2D) {
        if (m_usePause) {
          if (!m_pauseOn) {
            uint64_t localTimestamp;
            m_sequenceReaderPreScan.acquire(m_preScanImage2d, localTimestamp);
            invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
            imageHeader.timeStamp = localTimestamp;
            if (m_sequenceReaderPreScan.getSequenceTimestamps().size() >
                imageHeader.frameCount + 1 - m_pauseIndexOffset)
              m_nextImageTimestamp =
                  m_sequenceReaderPreScan.getSequenceTimestamps().at(imageHeader.frameCount + 1 - m_pauseIndexOffset);
            imageHeader.imageType = 1;
          } else { // pause activated, we continue sending the same image, and increasing timestamps
            uint64_t deltaT = m_nextImageTimestamp - imageHeader.timeStamp;
            m_nextImageTimestamp += deltaT;
            imageHeader.timeStamp += deltaT;
            m_pauseDurationOffset += deltaT;
            m_pauseIndexOffset++;
          }

          if (imageHeader.frameCount == m_pauseImageNumber)
            m_pauseOn = true;
        } else {
          uint64_t localTimestamp;
          m_sequenceReaderPostScan.acquire(m_postScanImage2d, localTimestamp);
          imageHeader.timeStamp = localTimestamp;
          if (m_sequenceReaderPostScan.getSequenceTimestamps().size() > imageHeader.frameCount + 1)
            m_nextImageTimestamp = m_sequenceReaderPostScan.getSequenceTimestamps().at(imageHeader.frameCount + 1);
          imageHeader.imageType = 1;
        }
      }
    } else { // first image
      if (m_usePause && imageHeader.frameCount == m_pauseImageNumber)
        m_pauseOn = true;

      if (m_usePause && m_pauseOn) {
        uint64_t deltaT = 40; // if we pause on first image, we set the pause time to 40ms
        m_nextImageTimestamp += deltaT;
        imageHeader.timeStamp += deltaT;
        m_pauseDurationOffset += deltaT;
        if (imageHeader.frameCount != 0) // manage first case
          m_pauseIndexOffset++;
      }
    }

    imageHeader.dataRate = 1000.0 / (m_nextImageTimestamp - imageHeader.timeStamp);

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    //out.setVersion(QDataStream::Qt_5_0);
    out.setVersion(QDataStream::Qt_4_8);
    out << imageHeader.headerId;
    out << imageHeader.frameCount;
    out << imageHeader.timeStamp;
    out << imageHeader.dataRate;

    if (m_imageType == us::PRESCAN_2D) {                                      // send pre-scan image
      out << (int)m_preScanImage2d.getHeight() * m_preScanImage2d.getWidth(); // datalength in bytes
      out << (int)8;                                                          // sample size in bits
      out << (int)0;
      out << m_preScanImage2d.getHeight();
      out << m_preScanImage2d.getWidth();
      out << (double).0; // pixelWidth
      out << (double).0; // pixelHeight
      out << m_preScanImage2d.getTransmitFrequency();
      out << m_preScanImage2d.getSamplingFrequency();
      out << m_preScanImage2d.getTransducerRadius();
      out << m_preScanImage2d.getScanLinePitch();
      out << (int)m_preScanImage2d.getScanLineNumber();
      out << (int)(m_postScanImage2d.getDepth() / 1000.0); // int in mm
      out << (double).0;                                   // degPerFrame
      out << (int)0;                                       // framesPerVolume
      out << (double).0;                                   // motorRadius
      out << (int)0;                                       // motorType
      out.writeRawData((char *)m_preScanImage2d.bitmap,
                       (int)m_preScanImage2d.getHeight() * m_preScanImage2d.getWidth());

      endOfSequence = (m_sequenceReaderPreScan.getFrameCount() == imageHeader.frameCount + 1 - m_pauseIndexOffset);
    } else if (m_imageType == us::POSTSCAN_2D) {                                // send post-scan image
      out << (int)m_postScanImage2d.getHeight() * m_postScanImage2d.getWidth(); // datalength in bytes
      out << (int)8;                                                            // sample size in bits
      out << (int)1;
      out << m_postScanImage2d.getWidth();
      out << m_postScanImage2d.getHeight();
      out << m_postScanImage2d.getWidthResolution();  // pixelWidth
      out << m_postScanImage2d.getHeightResolution(); // pixelHeight
      out << m_postScanImage2d.getTransmitFrequency();
      out << m_postScanImage2d.getSamplingFrequency();
      out << m_postScanImage2d.getTransducerRadius();
      out << m_postScanImage2d.getScanLinePitch();
      out << (int)m_postScanImage2d.getScanLineNumber();
      out << (int)(m_postScanImage2d.getDepth() / 1000.0); // int in mm
      out << (double).0;                                   // degPerFrame
      out << (int)0;                                       // framesPerVolume
      out << (double).0;                                   // motorRadius
      out << (int)0;                                       // motorType
      out.writeRawData((char *)m_postScanImage2d.bitmap,
                       (int)m_postScanImage2d.getHeight() * m_postScanImage2d.getWidth());

      endOfSequence = (m_sequenceReaderPostScan.getFrameCount() == imageHeader.frameCount + 1 - m_pauseIndexOffset);
    }

    connectionSoc->write(block);
    qApp->processEvents();

    std::cout << "new frame sent, No " << imageHeader.frameCount << std::endl;

    imageHeader.frameCount++;
    // WAITING PROCESS (to respect sequence timestamps)
    vpTime::wait((double)(m_nextImageTimestamp - imageHeader.timeStamp));
  }
#else
  throw(vpException(vpException::badValue),
        "usVirtualServer error : cannot use xml sequence, xml2 dependency is missing !");
#endif
}

/**
* Method to send the image sequence through the network in case of mhd/raw sequence as input.
*/
void usVirtualServer::sendingLoopSequenceMHD()
{
  bool endOfSequence = false;
  while (m_serverIsSendingImages && !endOfSequence) {
    // manage first frame sent (already aquired with open() method)
    if (imageHeader.frameCount != 0) {
      if (m_imageType == us::RF_2D) {
        if (m_usePause) {
          if (!m_pauseOn) {
            if (m_useRewind) {
              // counting the number of total sequence sent
              unsigned int currentNumberOfWholeSequenceSent = 0;
              if (imageHeader.frameCount - m_pauseIndexOffset >=
                  (unsigned int)m_MHDSequenceReader.getTotalImageNumber()) {
                currentNumberOfWholeSequenceSent =
                    ((imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) /
                     (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                    1;
              }
              if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence
                int imageIndex =
                    (imageHeader.frameCount - m_pauseIndexOffset) % (m_MHDSequenceReader.getTotalImageNumber());

                uint64_t localTimestamp;
                m_MHDSequenceReader.acquire(m_rfImage2d, localTimestamp);
                imageHeader.timeStamp = localTimestamp;
                if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
                  m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
                } else { // next timestamp is the previous image, so we compute the abs diff between 2 timpestamps
                  // first we get the timestamp of the previous image
                  usImageRF2D<short int> tmpImg;
                  uint64_t tmpTimpstamp;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, tmpTimpstamp);
                  // then we compute the delta
                  m_nextImageTimestamp = (imageHeader.timeStamp - tmpTimpstamp) + imageHeader.timeStamp;
                }
                imageHeader.imageType = 2;

              } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
                int imageIndex =
                    (imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) %
                        (m_MHDSequenceReader.getTotalImageNumber() - 1) +
                    1;

                // current real timestamp
                uint64_t localTimestamp;
                m_MHDSequenceReader.getImage(imageIndex, m_rfImage2d, localTimestamp);
                // offset (increment because of rewind)
                uint64_t previousTimestamp;
                usImageRF2D<short int> tmpImg;
                m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, previousTimestamp);
                uint64_t deltaTimestamp = localTimestamp - previousTimestamp;
                imageHeader.timeStamp += deltaTimestamp;

                // next timestamp (for waiting process)
                uint64_t nextTimestamp;
                uint64_t deltaTimestampNext;
                if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
                  usImageRF2D<short int> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = nextTimestamp - localTimestamp;
                } else {
                  usImageRF2D<short int> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = localTimestamp - nextTimestamp;
                }
                m_nextImageTimestamp += deltaTimestampNext;

                imageHeader.imageType = 2;
              } else {
                int imageIndex =
                    m_MHDSequenceReader.getTotalImageNumber() - 2 -
                    ((imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) %
                     (m_MHDSequenceReader.getTotalImageNumber() - 1));

                // current real timestamp
                uint64_t localTimestamp;
                m_MHDSequenceReader.getImage(imageIndex, m_rfImage2d, localTimestamp);
                // offset (increment because of rewind)
                uint64_t previousTimestamp;
                usImageRF2D<short int> tmpImg;
                m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, previousTimestamp);
                uint64_t deltaTimestamp = previousTimestamp - localTimestamp;
                imageHeader.timeStamp += deltaTimestamp;

                // next timestamp (for waiting process)
                uint64_t nextTimestamp;
                uint64_t deltaTimestampNext;
                if (imageIndex != 0) {
                  usImageRF2D<short int> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = localTimestamp - nextTimestamp;
                } else {
                  usImageRF2D<short int> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = nextTimestamp - localTimestamp;
                }
                m_nextImageTimestamp += deltaTimestampNext;
                imageHeader.imageType = 2;
              }
            } else {
              uint64_t localTimestamp;
              m_MHDSequenceReader.acquire(m_rfImage2d, localTimestamp);
              imageHeader.timeStamp = localTimestamp + m_pauseDurationOffset;
              if (!m_MHDSequenceReader.end())
                m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp() + m_pauseDurationOffset;
              imageHeader.imageType = 2;
            }
          } else { // pause activated, we continue sending the same image, and increasing timestamps
            uint64_t deltaT = m_nextImageTimestamp - imageHeader.timeStamp;
            m_nextImageTimestamp += deltaT;
            imageHeader.timeStamp += deltaT;
            m_pauseDurationOffset += deltaT;
            m_pauseIndexOffset++;
          }

          if (imageHeader.frameCount == m_pauseImageNumber)
            m_pauseOn = true;
        } else if (m_useRewind) {
          // counting the number of total sequence sent
          unsigned int currentNumberOfWholeSequenceSent = 0;
          if (imageHeader.frameCount >= (unsigned int)m_MHDSequenceReader.getTotalImageNumber()) {
            currentNumberOfWholeSequenceSent = ((imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) /
                                                (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                                               1;
          }
          if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence
            int imageIndex = (imageHeader.frameCount % (m_MHDSequenceReader.getTotalImageNumber()));

            uint64_t localTimestamp;
            m_MHDSequenceReader.acquire(m_rfImage2d, localTimestamp);
            imageHeader.timeStamp = localTimestamp;
            if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
              m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
            } else { // next timestamp is the previous image, so we compute the abs diff between 2 timpestamps
              // first we get the timestamp of the previous image
              usImageRF2D<short int> tmpImg;
              uint64_t tmpTimpstamp;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, tmpTimpstamp);
              // then we compute the delta
              m_nextImageTimestamp = (imageHeader.timeStamp - tmpTimpstamp) + imageHeader.timeStamp;
            }
            imageHeader.imageType = 2;

          } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
            int imageIndex = (imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) %
                                 (m_MHDSequenceReader.getTotalImageNumber() - 1) +
                             1;

            // current real timestamp
            uint64_t localTimestamp;
            m_MHDSequenceReader.getImage(imageIndex, m_rfImage2d, localTimestamp);
            // offset (increment because of rewind)
            uint64_t previousTimestamp;
            usImageRF2D<short int> tmpImg;
            m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, previousTimestamp);
            uint64_t deltaTimestamp = localTimestamp - previousTimestamp;
            imageHeader.timeStamp += deltaTimestamp;

            // next timestamp (for waiting process)
            uint64_t nextTimestamp;
            uint64_t deltaTimestampNext;
            if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
              usImageRF2D<short int> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
              deltaTimestampNext = nextTimestamp - localTimestamp;
            } else {
              usImageRF2D<short int> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
              deltaTimestampNext = localTimestamp - nextTimestamp;
            }
            m_nextImageTimestamp += deltaTimestampNext;

            imageHeader.imageType = 2;
          } else {
            int imageIndex = m_MHDSequenceReader.getTotalImageNumber() - 2 -
                             ((imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) %
                              (m_MHDSequenceReader.getTotalImageNumber() - 1));

            // current real timestamp
            uint64_t localTimestamp;
            m_MHDSequenceReader.getImage(imageIndex, m_rfImage2d, localTimestamp);
            // offset (increment because of rewind)
            uint64_t previousTimestamp;
            usImageRF2D<short int> tmpImg;
            m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, previousTimestamp);
            uint64_t deltaTimestamp = previousTimestamp - localTimestamp;
            imageHeader.timeStamp += deltaTimestamp;

            // next timestamp (for waiting process)
            uint64_t nextTimestamp;
            uint64_t deltaTimestampNext;
            if (imageIndex != 0) {
              usImageRF2D<short int> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
              deltaTimestampNext = localTimestamp - nextTimestamp;
            } else {
              usImageRF2D<short int> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
              deltaTimestampNext = nextTimestamp - localTimestamp;
            }
            m_nextImageTimestamp += deltaTimestampNext;
            imageHeader.imageType = 2;
          }
        } else {
          uint64_t localTimestamp;
          m_MHDSequenceReader.acquire(m_rfImage2d, localTimestamp);
          imageHeader.timeStamp = localTimestamp;
          if (!m_MHDSequenceReader.end())
            m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
          imageHeader.imageType = 2;
        }
      } else if (m_imageType == us::PRESCAN_2D) {
        if (m_usePause) {
          if (!m_pauseOn) {
            if (m_useRewind) {
              // counting the number of total sequence sent
              unsigned int currentNumberOfWholeSequenceSent = 0;
              if (imageHeader.frameCount - m_pauseIndexOffset >=
                  (unsigned int)m_MHDSequenceReader.getTotalImageNumber()) {
                currentNumberOfWholeSequenceSent =
                    ((imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) /
                     (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                    1;
              }
              if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence
                int imageIndex =
                    (imageHeader.frameCount - m_pauseIndexOffset) % (m_MHDSequenceReader.getTotalImageNumber());

                uint64_t localTimestamp;
                m_MHDSequenceReader.acquire(m_preScanImage2d, localTimestamp);
                invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
                imageHeader.timeStamp = localTimestamp;
                if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
                  m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
                } else { // next timestamp is the previous image, so we compute the abs diff between 2 timpestamps
                  // first we get the timestamp of the previous image
                  usImagePreScan2D<unsigned char> tmpImg;
                  uint64_t tmpTimpstamp;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, tmpTimpstamp);
                  // then we compute the delta
                  m_nextImageTimestamp = (imageHeader.timeStamp - tmpTimpstamp) + imageHeader.timeStamp;
                }
                imageHeader.imageType = 0;

              } else if (currentNumberOfWholeSequenceSent % 2 == 0) { // sequence currently sent : from image 1 to N-1
                int imageIndex =
                    (imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) %
                        (m_MHDSequenceReader.getTotalImageNumber() - 1) +
                    1;

                // current real timestamp
                uint64_t localTimestamp;
                m_MHDSequenceReader.getImage(imageIndex, m_preScanImage2d, localTimestamp);
                invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
                // offset (increment because of rewind)
                uint64_t previousTimestamp;
                usImagePreScan2D<unsigned char> tmpImg;
                m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, previousTimestamp);
                uint64_t deltaTimestamp = localTimestamp - previousTimestamp;
                imageHeader.timeStamp += deltaTimestamp;

                // next timestamp (for waiting process)
                uint64_t nextTimestamp;
                uint64_t deltaTimestampNext;
                if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
                  usImagePreScan2D<unsigned char> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = nextTimestamp - localTimestamp;
                } else {
                  usImagePreScan2D<unsigned char> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = localTimestamp - nextTimestamp;
                }
                m_nextImageTimestamp += deltaTimestampNext;

                imageHeader.imageType = 0;
              } else { // sequence currently sent : from image N-2 to 0
                int imageIndex =
                    m_MHDSequenceReader.getTotalImageNumber() - 2 -
                    ((imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) %
                     (m_MHDSequenceReader.getTotalImageNumber() - 1));

                // current real timestamp
                uint64_t localTimestamp;
                m_MHDSequenceReader.getImage(imageIndex, m_preScanImage2d, localTimestamp);
                invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
                // offset (increment because of rewind)
                uint64_t previousTimestamp;
                usImagePreScan2D<unsigned char> tmpImg;
                m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, previousTimestamp);
                uint64_t deltaTimestamp = previousTimestamp - localTimestamp;
                imageHeader.timeStamp += deltaTimestamp;

                // next timestamp (for waiting process)
                uint64_t nextTimestamp;
                uint64_t deltaTimestampNext;
                if (imageIndex != 0) {
                  usImagePreScan2D<unsigned char> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = localTimestamp - nextTimestamp;
                } else {
                  usImagePreScan2D<unsigned char> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = nextTimestamp - localTimestamp;
                }
                m_nextImageTimestamp += deltaTimestampNext;
                imageHeader.imageType = 0;
              }
            } else {
              uint64_t localTimestamp;
              m_MHDSequenceReader.acquire(m_preScanImage2d, localTimestamp);
              invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
              imageHeader.timeStamp = localTimestamp + m_pauseDurationOffset;
              if (!m_MHDSequenceReader.end())
                m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp() + m_pauseDurationOffset;
              imageHeader.imageType = 0;
            }
          } else { // pause activated, we continue sending the same image, and increasing timestamps
            uint64_t deltaT = m_nextImageTimestamp - imageHeader.timeStamp;
            m_nextImageTimestamp += deltaT;
            imageHeader.timeStamp += deltaT;
            m_pauseDurationOffset += deltaT;
            m_pauseIndexOffset++;
          }

          if (imageHeader.frameCount == m_pauseImageNumber)
            m_pauseOn = true;
        } else if (m_useRewind) {
          // counting the number of total sequence sent
          unsigned int currentNumberOfWholeSequenceSent = 0;
          if (imageHeader.frameCount >= (unsigned int)m_MHDSequenceReader.getTotalImageNumber()) {
            currentNumberOfWholeSequenceSent = ((imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) /
                                                (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                                               1;
          }
          if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence
            int imageIndex = (imageHeader.frameCount % (m_MHDSequenceReader.getTotalImageNumber()));

            uint64_t localTimestamp;
            m_MHDSequenceReader.acquire(m_preScanImage2d, localTimestamp);
            invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
            imageHeader.timeStamp = localTimestamp;
            if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
              m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
            } else { // next timestamp is the previous image, so we compute the abs diff between 2 timpestamps
              // first we get the timestamp of the previous image
              usImagePreScan2D<unsigned char> tmpImg;
              uint64_t tmpTimpstamp;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, tmpTimpstamp);
              // then we compute the delta
              m_nextImageTimestamp = (imageHeader.timeStamp - tmpTimpstamp) + imageHeader.timeStamp;
            }
            imageHeader.imageType = 0;

          } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
            int imageIndex = (imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) %
                                 (m_MHDSequenceReader.getTotalImageNumber() - 1) +
                             1;

            // current real timestamp
            uint64_t localTimestamp;
            m_MHDSequenceReader.getImage(imageIndex, m_preScanImage2d, localTimestamp);
            invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
            // offset (increment because of rewind)
            uint64_t previousTimestamp;
            usImagePreScan2D<unsigned char> tmpImg;
            m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, previousTimestamp);
            uint64_t deltaTimestamp = localTimestamp - previousTimestamp;
            imageHeader.timeStamp += deltaTimestamp;

            // next timestamp (for waiting process)
            uint64_t nextTimestamp;
            uint64_t deltaTimestampNext;
            if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
              usImagePreScan2D<unsigned char> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
              deltaTimestampNext = nextTimestamp - localTimestamp;
            } else {
              usImagePreScan2D<unsigned char> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
              deltaTimestampNext = localTimestamp - nextTimestamp;
            }
            m_nextImageTimestamp += deltaTimestampNext;

            imageHeader.imageType = 0;
          } else {
            int imageIndex = m_MHDSequenceReader.getTotalImageNumber() - 2 -
                             ((imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) %
                              (m_MHDSequenceReader.getTotalImageNumber() - 1));

            // current real timestamp
            uint64_t localTimestamp;
            m_MHDSequenceReader.getImage(imageIndex, m_preScanImage2d, localTimestamp);
            invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
            // offset (increment because of rewind)
            uint64_t previousTimestamp;
            usImagePreScan2D<unsigned char> tmpImg;
            m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, previousTimestamp);
            uint64_t deltaTimestamp = previousTimestamp - localTimestamp;
            imageHeader.timeStamp += deltaTimestamp;

            // next timestamp (for waiting process)
            uint64_t nextTimestamp;
            uint64_t deltaTimestampNext;
            if (imageIndex != 0) {
              usImagePreScan2D<unsigned char> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
              deltaTimestampNext = localTimestamp - nextTimestamp;
            } else {
              usImagePreScan2D<unsigned char> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
              deltaTimestampNext = nextTimestamp - localTimestamp;
            }
            m_nextImageTimestamp += deltaTimestampNext;
            imageHeader.imageType = 0;
          }
        } else {
          uint64_t localTimestamp;
          m_MHDSequenceReader.acquire(m_preScanImage2d, localTimestamp);
          invertRowsColsOnPreScan(); // to fit with ultrasonix grabbers (pre-scan image is inverted in porta SDK)
          imageHeader.timeStamp = localTimestamp;
          if (!m_MHDSequenceReader.end())
            m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
          imageHeader.imageType = 0;
        }
      } else if (m_imageType == us::POSTSCAN_2D) {
        if (m_usePause) {
          if (!m_pauseOn) {
            if (m_useRewind) {
              // counting the number of total sequence sent
              unsigned int currentNumberOfWholeSequenceSent = 0;
              if (imageHeader.frameCount - m_pauseIndexOffset >=
                  (unsigned int)m_MHDSequenceReader.getTotalImageNumber()) {
                currentNumberOfWholeSequenceSent =
                    ((imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) /
                     (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                    1;
              }
              if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence
                int imageIndex =
                    (imageHeader.frameCount - m_pauseIndexOffset) % (m_MHDSequenceReader.getTotalImageNumber());

                uint64_t localTimestamp;
                m_MHDSequenceReader.acquire(m_postScanImage2d, localTimestamp);
                imageHeader.timeStamp = localTimestamp;
                if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
                  m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
                } else { // next timestamp is the previous image, so we compute the abs diff between 2 timpestamps
                  // first we get the timestamp of the previous image
                  usImagePostScan2D<unsigned char> tmpImg;
                  uint64_t tmpTimpstamp;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, tmpTimpstamp);
                  // then we compute the delta
                  m_nextImageTimestamp = (imageHeader.timeStamp - tmpTimpstamp) + imageHeader.timeStamp;
                }
                imageHeader.imageType = 1;

              } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
                int imageIndex =
                    (imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) %
                        (m_MHDSequenceReader.getTotalImageNumber() - 1) +
                    1;

                // current real timestamp
                uint64_t localTimestamp;
                m_MHDSequenceReader.getImage(imageIndex, m_postScanImage2d, localTimestamp);
                // offset (increment because of rewind)
                uint64_t previousTimestamp;
                usImagePostScan2D<unsigned char> tmpImg;
                m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, previousTimestamp);
                uint64_t deltaTimestamp = localTimestamp - previousTimestamp;
                imageHeader.timeStamp += deltaTimestamp;

                // next timestamp (for waiting process)
                uint64_t nextTimestamp;
                uint64_t deltaTimestampNext;
                if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
                  usImagePostScan2D<unsigned char> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = nextTimestamp - localTimestamp;
                } else {
                  usImagePostScan2D<unsigned char> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = localTimestamp - nextTimestamp;
                }
                m_nextImageTimestamp += deltaTimestampNext;

                imageHeader.imageType = 1;
              } else {
                int imageIndex =
                    m_MHDSequenceReader.getTotalImageNumber() - 2 -
                    ((imageHeader.frameCount - m_pauseIndexOffset - m_MHDSequenceReader.getTotalImageNumber()) %
                     (m_MHDSequenceReader.getTotalImageNumber() - 1));

                // current real timestamp
                uint64_t localTimestamp;
                m_MHDSequenceReader.getImage(imageIndex, m_postScanImage2d, localTimestamp);
                // offset (increment because of rewind)
                uint64_t previousTimestamp;
                usImagePostScan2D<unsigned char> tmpImg;
                m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, previousTimestamp);
                uint64_t deltaTimestamp = previousTimestamp - localTimestamp;
                imageHeader.timeStamp += deltaTimestamp;

                // next timestamp (for waiting process)
                uint64_t nextTimestamp;
                uint64_t deltaTimestampNext;
                if (imageIndex != 0) {
                  usImagePostScan2D<unsigned char> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = localTimestamp - nextTimestamp;
                } else {
                  usImagePostScan2D<unsigned char> tmpImg;
                  m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
                  deltaTimestampNext = nextTimestamp - localTimestamp;
                }
                m_nextImageTimestamp += deltaTimestampNext;
                imageHeader.imageType = 1;
              }
            } else {
              uint64_t localTimestamp;
              m_MHDSequenceReader.acquire(m_postScanImage2d, localTimestamp);
              imageHeader.timeStamp = localTimestamp + m_pauseDurationOffset;
              if (!m_MHDSequenceReader.end())
                m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp() + m_pauseDurationOffset;
              imageHeader.imageType = 1;
            }
          } else { // pause activated, we continue sending the same image, and increasing timestamps
            uint64_t deltaT = m_nextImageTimestamp - imageHeader.timeStamp;
            m_nextImageTimestamp += deltaT;
            imageHeader.timeStamp += deltaT;
            m_pauseDurationOffset += deltaT;
            m_pauseIndexOffset++;
          }

          if (imageHeader.frameCount == m_pauseImageNumber)
            m_pauseOn = true;
        } else if (m_useRewind) {
          // counting the number of total sequence sent
          unsigned int currentNumberOfWholeSequenceSent = 0;
          if (imageHeader.frameCount >= (unsigned int)m_MHDSequenceReader.getTotalImageNumber()) {
            currentNumberOfWholeSequenceSent = ((imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) /
                                                (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                                               1;
          }
          if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence
            int imageIndex = (imageHeader.frameCount % (m_MHDSequenceReader.getTotalImageNumber()));

            uint64_t localTimestamp;
            m_MHDSequenceReader.acquire(m_postScanImage2d, localTimestamp);
            imageHeader.timeStamp = localTimestamp;
            if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
              m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
            } else { // next timestamp is the previous image, so we compute the abs diff between 2 timpestamps
              // first we get the timestamp of the previous image
              usImagePostScan2D<unsigned char> tmpImg;
              uint64_t tmpTimpstamp;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, tmpTimpstamp);
              // then we compute the delta
              m_nextImageTimestamp = (imageHeader.timeStamp - tmpTimpstamp) + imageHeader.timeStamp;
            }
            imageHeader.imageType = 1;

          } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
            int imageIndex = (imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) %
                                 (m_MHDSequenceReader.getTotalImageNumber() - 1) +
                             1;

            // current real timestamp
            uint64_t localTimestamp;
            m_MHDSequenceReader.getImage(imageIndex, m_postScanImage2d, localTimestamp);
            // offset (increment because of rewind)
            uint64_t previousTimestamp;
            usImagePostScan2D<unsigned char> tmpImg;
            m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, previousTimestamp);
            uint64_t deltaTimestamp = localTimestamp - previousTimestamp;
            imageHeader.timeStamp += deltaTimestamp;

            // next timestamp (for waiting process)
            uint64_t nextTimestamp;
            uint64_t deltaTimestampNext;
            if (imageIndex != m_MHDSequenceReader.getTotalImageNumber() - 1) {
              usImagePostScan2D<unsigned char> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
              deltaTimestampNext = nextTimestamp - localTimestamp;
            } else {
              usImagePostScan2D<unsigned char> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
              deltaTimestampNext = localTimestamp - nextTimestamp;
            }
            m_nextImageTimestamp += deltaTimestampNext;

            imageHeader.imageType = 1;
          } else {
            int imageIndex = m_MHDSequenceReader.getTotalImageNumber() - 2 -
                             ((imageHeader.frameCount - m_MHDSequenceReader.getTotalImageNumber()) %
                              (m_MHDSequenceReader.getTotalImageNumber() - 1));

            // current real timestamp
            uint64_t localTimestamp;
            m_MHDSequenceReader.getImage(imageIndex, m_postScanImage2d, localTimestamp);
            // offset (increment because of rewind)
            uint64_t previousTimestamp;
            usImagePostScan2D<unsigned char> tmpImg;
            m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, previousTimestamp);
            uint64_t deltaTimestamp = previousTimestamp - localTimestamp;
            imageHeader.timeStamp += deltaTimestamp;

            // next timestamp (for waiting process)
            uint64_t nextTimestamp;
            uint64_t deltaTimestampNext;
            if (imageIndex != 0) {
              usImagePostScan2D<unsigned char> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex - 1, tmpImg, nextTimestamp);
              deltaTimestampNext = localTimestamp - nextTimestamp;
            } else {
              usImagePostScan2D<unsigned char> tmpImg;
              m_MHDSequenceReader.getImage(imageIndex + 1, tmpImg, nextTimestamp);
              deltaTimestampNext = nextTimestamp - localTimestamp;
            }
            m_nextImageTimestamp += deltaTimestampNext;
            imageHeader.imageType = 1;
          }
        } else {
          uint64_t localTimestamp;
          m_MHDSequenceReader.acquire(m_postScanImage2d, localTimestamp);
          imageHeader.timeStamp = localTimestamp;
          if (!m_MHDSequenceReader.end())
            m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamp();
          imageHeader.imageType = 1;
        }
      } else if (m_imageType == us::RF_3D) {
        if (m_usePause) {
          if (!m_pauseOn) {
            if (m_useRewind) {
              // counting the number of total sequence sent
              unsigned int currentNumberOfWholeSequenceSent = 0;

              if (imageHeader.frameCount - m_pauseIndexOffset >=
                  (unsigned int)m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber()) {
                currentNumberOfWholeSequenceSent =
                    ((imageHeader.frameCount - m_pauseIndexOffset -
                      (m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber())) /
                     ((m_MHDSequenceReader.getTotalImageNumber() - 1) * m_rfImage3d.getFrameNumber())) +
                    1;
              }

              if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence
                int volumeIndex = ((imageHeader.frameCount - m_pauseIndexOffset) / m_rfImage3d.getFrameNumber());

                m_MHDSequenceReader.getImage(volumeIndex, m_rfImage3d, m_timestamps);
                imageHeader.timeStamp = m_timestamps.at(0);
                m_nextImageTimestamp = m_timestamps.at(1);
                imageHeader.imageType = 2;

              } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
                int volumeIndex = (((imageHeader.frameCount - m_pauseIndexOffset -
                                     (m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber())) /
                                    m_rfImage3d.getFrameNumber()) %
                                   (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                                  1;

                m_MHDSequenceReader.getImage(volumeIndex, m_rfImage3d, m_timestamps);
                imageHeader.imageType = 2;
              } else {
                int volumeIndex = m_MHDSequenceReader.getTotalImageNumber() - 2 -
                                  ((imageHeader.frameCount - m_pauseIndexOffset -
                                    (m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber())) /
                                   m_rfImage3d.getFrameNumber()) %
                                      (m_MHDSequenceReader.getTotalImageNumber() - 1);

                m_MHDSequenceReader.getImage(volumeIndex, m_rfImage3d, m_timestamps);
                imageHeader.imageType = 2;
              }
            } else {
              m_MHDSequenceReader.acquire(m_rfImage3d, m_timestamps);
              imageHeader.timeStamp = m_timestamps.at(0) + m_pauseDurationOffset;
              m_nextImageTimestamp = m_timestamps.at(1) + m_pauseDurationOffset;
              imageHeader.imageType = 2;
            }
          }

          if (imageHeader.frameCount / m_rfImage3d.getFrameNumber() == m_pauseImageNumber &&
              !m_pauseOn) { // we arrive on the volume to pause on
            m_MHDSequenceReader.acquire(m_rfImage3dTemp,
                                        m_timestampsTemp); // temp attributes contains next volume and timestamps
            m_pauseOn = true;
          }
        } else if (m_useRewind) {
          // counting the number of total sequence sent
          unsigned int currentNumberOfWholeSequenceSent = 0;

          if (imageHeader.frameCount >=
              (unsigned int)m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber()) {
            currentNumberOfWholeSequenceSent =
                ((imageHeader.frameCount - (m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber())) /
                 ((m_MHDSequenceReader.getTotalImageNumber() - 1) * m_rfImage3d.getFrameNumber())) +
                1;
          }

          if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence

            m_MHDSequenceReader.acquire(m_rfImage3d, m_timestamps);
            imageHeader.timeStamp = m_timestamps.at(0);
            m_nextImageTimestamp = m_timestamps.at(1);
            imageHeader.imageType = 2;

          } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
            int volumeIndex = (((imageHeader.frameCount -
                                 (m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber())) /
                                m_rfImage3d.getFrameNumber()) %
                               (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                              1;

            m_MHDSequenceReader.getImage(volumeIndex, m_rfImage3d, m_timestamps);
            imageHeader.imageType = 2;
          } else {
            int volumeIndex =
                m_MHDSequenceReader.getTotalImageNumber() - 2 -
                ((imageHeader.frameCount - (m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber())) /
                 m_rfImage3d.getFrameNumber()) %
                    (m_MHDSequenceReader.getTotalImageNumber() - 1);

            m_MHDSequenceReader.getImage(volumeIndex, m_rfImage3d, m_timestamps);
            imageHeader.imageType = 2;
          }
        } else {
          m_MHDSequenceReader.acquire(m_rfImage3d, m_timestamps);
          imageHeader.timeStamp = m_timestamps.at(0);
          m_nextImageTimestamp = m_timestamps.at(1);
          imageHeader.imageType = 2;
        }
      } else if (m_imageType == us::PRESCAN_3D) {
        if (m_usePause) {
          if (!m_pauseOn) {
            if (m_useRewind) {
              // counting the number of total sequence sent
              unsigned int currentNumberOfWholeSequenceSent = 0;

              if (imageHeader.frameCount - m_pauseIndexOffset >=
                  (unsigned int)m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber()) {
                currentNumberOfWholeSequenceSent =
                    ((imageHeader.frameCount - m_pauseIndexOffset -
                      (m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber())) /
                     ((m_MHDSequenceReader.getTotalImageNumber() - 1) * m_preScanImage3d.getFrameNumber())) +
                    1;
              }

              if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence
                int volumeIndex = ((imageHeader.frameCount - m_pauseIndexOffset) / m_preScanImage3d.getFrameNumber());

                m_MHDSequenceReader.getImage(volumeIndex, m_preScanImage3d, m_timestamps);
                imageHeader.timeStamp = m_timestamps.at(0);
                m_nextImageTimestamp = m_timestamps.at(1);
                imageHeader.imageType = 0;

              } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
                int volumeIndex = (((imageHeader.frameCount - m_pauseIndexOffset -
                                     (m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber())) /
                                    m_preScanImage3d.getFrameNumber()) %
                                   (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                                  1;

                m_MHDSequenceReader.getImage(volumeIndex, m_preScanImage3d, m_timestamps);
                imageHeader.imageType = 0;
              } else {
                int volumeIndex = m_MHDSequenceReader.getTotalImageNumber() - 2 -
                                  ((imageHeader.frameCount - m_pauseIndexOffset -
                                    (m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber())) /
                                   m_preScanImage3d.getFrameNumber()) %
                                      (m_MHDSequenceReader.getTotalImageNumber() - 1);

                m_MHDSequenceReader.getImage(volumeIndex, m_preScanImage3d, m_timestamps);
                imageHeader.imageType = 0;
              }
            } else {
              m_MHDSequenceReader.acquire(m_preScanImage3d, m_timestamps);
              imageHeader.timeStamp = m_timestamps.at(0) + m_pauseDurationOffset;
              m_nextImageTimestamp = m_timestamps.at(1) + m_pauseDurationOffset;
              imageHeader.imageType = 0;
            }
          }

          if (imageHeader.frameCount / m_preScanImage3d.getFrameNumber() == m_pauseImageNumber &&
              !m_pauseOn) { // we arrive on the volume to pause on
            m_MHDSequenceReader.acquire(m_preScanImage3dTemp,
                                        m_timestampsTemp); // temp attributes contains next volume and timestamps
            m_pauseOn = true;
          }
        } else if (m_useRewind) {
          // counting the number of total sequence sent
          unsigned int currentNumberOfWholeSequenceSent = 0;

          if (imageHeader.frameCount >=
              (unsigned int)m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber()) {
            currentNumberOfWholeSequenceSent =
                ((imageHeader.frameCount -
                  (m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber())) /
                 ((m_MHDSequenceReader.getTotalImageNumber() - 1) * m_preScanImage3d.getFrameNumber())) +
                1;
          }

          if (currentNumberOfWholeSequenceSent == 0) { // sending first sequence

            m_MHDSequenceReader.acquire(m_preScanImage3d, m_timestamps);
            imageHeader.timeStamp = m_timestamps.at(0);
            m_nextImageTimestamp = m_timestamps.at(1);
            imageHeader.imageType = 0;

          } else if (currentNumberOfWholeSequenceSent % 2 == 0) {
            int volumeIndex = (((imageHeader.frameCount -
                                 (m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber())) /
                                m_preScanImage3d.getFrameNumber()) %
                               (m_MHDSequenceReader.getTotalImageNumber() - 1)) +
                              1;

            m_MHDSequenceReader.getImage(volumeIndex, m_preScanImage3d, m_timestamps);
            imageHeader.imageType = 0;
          } else {
            int volumeIndex = m_MHDSequenceReader.getTotalImageNumber() - 2 -
                              ((imageHeader.frameCount -
                                (m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber())) /
                               m_preScanImage3d.getFrameNumber()) %
                                  (m_MHDSequenceReader.getTotalImageNumber() - 1);

            m_MHDSequenceReader.getImage(volumeIndex, m_preScanImage3d, m_timestamps);
            imageHeader.imageType = 0;
          }
        } else {
          m_MHDSequenceReader.acquire(m_preScanImage3d, m_timestamps);
          imageHeader.timeStamp = m_timestamps.at(0);
          m_nextImageTimestamp = m_timestamps.at(1);
          imageHeader.imageType = 0;
        }
      }
    } else if (m_imageType == us::RF_2D || m_imageType == us::PRESCAN_2D ||
               m_imageType == us::POSTSCAN_2D) { // pause on first image (2D case)
      if (m_usePause && imageHeader.frameCount == m_pauseImageNumber)
        m_pauseOn = true;

      if (m_usePause && m_pauseOn) {
        uint64_t deltaT = 40; // if we pause on first image, we set the time delta between 2 same frames to 40ms
        m_nextImageTimestamp += deltaT;
        imageHeader.timeStamp += deltaT;
        m_pauseDurationOffset += deltaT;
        if (imageHeader.frameCount != 0) // manage first image case
          m_pauseIndexOffset++;
      }
    }

    if (m_imageType == us::RF_2D) { // send RF image
      imageHeader.dataRate = 1000.0 / (m_nextImageTimestamp - imageHeader.timeStamp);

      QByteArray block;
      QDataStream out(&block, QIODevice::WriteOnly);
      //out.setVersion(QDataStream::Qt_5_0);
      out.setVersion(QDataStream::Qt_4_8);

      out << imageHeader.headerId;
      out << imageHeader.frameCount;
      out << imageHeader.timeStamp;
      out << imageHeader.dataRate;
      out << (int)m_rfImage2d.getHeight() * m_rfImage2d.getWidth() * 2; // datalength in bytes
      out << (int)16;                                                   // sample size in bits
      out << (int)2;                                                    // image type
      out << m_rfImage2d.getWidth();
      out << m_rfImage2d.getHeight();
      out << (double).0; // pixelWidth
      out << (double).0; // pixelHeight
      out << m_rfImage2d.getTransmitFrequency();
      out << m_rfImage2d.getSamplingFrequency();
      out << m_rfImage2d.getTransducerRadius();
      out << m_rfImage2d.getScanLinePitch();
      out << (int)m_rfImage2d.getScanLineNumber();
      out << (int)(m_rfImage2d.getDepth() * 1000.0); // int in mm
      out << (double).0;                             // degPerFrame
      out << (int)0;                                 // framesPerVolume
      out << (double).0;                             // motorRadius
      out << (int)0;                                 // motorType
      out.writeRawData((char *)m_rfImage2d.bitmap, (int)m_rfImage2d.getHeight() * m_rfImage2d.getWidth() * 2);

      endOfSequence = m_MHDSequenceReader.end() && !m_useRewind;

      connectionSoc->write(block);
      qApp->processEvents();

      std::cout << "new frame sent, No " << imageHeader.frameCount << std::endl;

      imageHeader.frameCount++;

      // WAITING PROCESS (to respect sequence timestamps)
      vpTime::wait((double)(m_nextImageTimestamp - imageHeader.timeStamp));
    } else if (m_imageType == us::PRESCAN_2D) { // send pre-scan image
      imageHeader.dataRate = 1000.0 / (m_nextImageTimestamp - imageHeader.timeStamp);

      QByteArray block;
      QDataStream out(&block, QIODevice::WriteOnly);
      //out.setVersion(QDataStream::Qt_5_0);
      out.setVersion(QDataStream::Qt_4_8);

      out << imageHeader.headerId;
      out << imageHeader.frameCount;
      out << imageHeader.timeStamp;
      out << imageHeader.dataRate;
      out << (int)m_preScanImage2d.getHeight() * m_preScanImage2d.getWidth(); // datalength in bytes
      out << (int)8;                                                          // sample size in bits
      out << (int)0;
      out << m_preScanImage2d.getHeight();
      out << m_preScanImage2d.getWidth();
      out << (double).0; // pixelWidth
      out << (double).0; // pixelHeight
      out << m_preScanImage2d.getTransmitFrequency();
      out << m_preScanImage2d.getSamplingFrequency();
      out << m_preScanImage2d.getTransducerRadius();
      out << m_preScanImage2d.getScanLinePitch();
      out << (int)m_preScanImage2d.getScanLineNumber();
      out << (int)(m_preScanImage2d.getDepth() * 1000.0); // int in mm
      out << (double).0;                                  // degPerFrame
      out << (int)0;                                      // framesPerVolume
      out << (double).0;                                  // motorRadius
      out << (int)0;                                      // motorType
      out.writeRawData((char *)m_preScanImage2d.bitmap,
                       (int)m_preScanImage2d.getHeight() * m_preScanImage2d.getWidth());

      endOfSequence = m_MHDSequenceReader.end() && !m_useRewind;

      connectionSoc->write(block);
      qApp->processEvents();

      std::cout << "new frame sent, No " << imageHeader.frameCount << std::endl;

      imageHeader.frameCount++;

      // WAITING PROCESS (to respect sequence timestamps)
      vpTime::wait((double)(m_nextImageTimestamp - imageHeader.timeStamp));
    } else if (m_imageType == us::POSTSCAN_2D) { // send post-scan image
      imageHeader.dataRate = 1000.0 / (m_nextImageTimestamp - imageHeader.timeStamp);

      QByteArray block;
      QDataStream out(&block, QIODevice::WriteOnly);
      //out.setVersion(QDataStream::Qt_5_0);
      out.setVersion(QDataStream::Qt_4_8);

      out << imageHeader.headerId;
      out << imageHeader.frameCount;
      out << imageHeader.timeStamp;
      out << imageHeader.dataRate;
      out << (int)m_postScanImage2d.getHeight() * m_postScanImage2d.getWidth(); // datalength in bytes
      out << (int)8;                                                            // sample size in bits
      out << (int)1;
      out << m_postScanImage2d.getWidth();
      out << m_postScanImage2d.getHeight();
      out << m_postScanImage2d.getWidthResolution();  // pixelWidth
      out << m_postScanImage2d.getHeightResolution(); // pixelHeight
      out << m_postScanImage2d.getTransmitFrequency();
      out << m_postScanImage2d.getSamplingFrequency();
      out << m_postScanImage2d.getTransducerRadius();
      out << m_postScanImage2d.getScanLinePitch();
      out << (int)m_postScanImage2d.getScanLineNumber();
      out << (int)(m_postScanImage2d.getDepth() * 1000.0); // int in mm
      out << (double).0;                                   // degPerFrame
      out << (int)0;                                       // framesPerVolume
      out << (double).0;                                   // motorRadius
      out << (int)0;                                       // motorType
      out.writeRawData((char *)m_postScanImage2d.bitmap,
                       (int)m_postScanImage2d.getHeight() * m_postScanImage2d.getWidth());

      endOfSequence = m_MHDSequenceReader.end() && !m_useRewind;

      connectionSoc->write(block);
      qApp->processEvents();

      std::cout << "new frame sent, No " << imageHeader.frameCount << std::endl;

      imageHeader.frameCount++;

      // WAITING PROCESS (to respect sequence timestamps)
      vpTime::wait((double)(m_nextImageTimestamp - imageHeader.timeStamp));
    }
    // 3D case
    else if (m_imageType == us::RF_3D) { // send RF volume frame by frame
      bool endOfVolume = false;
      unsigned int currentFrameInVolume = 0;
      while (!endOfVolume) {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
      //out.setVersion(QDataStream::Qt_5_0);
      out.setVersion(QDataStream::Qt_4_8);
        // new frame to send
        if (m_pauseOn) { // pause case (we send volumes V, V+1, V, V+1, ...)
          // check if current volume is odd or even
          unsigned int framePositionInVolume;
          if ((imageHeader.frameCount / m_rfImage3d.getFrameNumber()) % 2 == 1)
            framePositionInVolume = m_rfImage3d.getFrameNumber() - currentFrameInVolume - 1;
          else
            framePositionInVolume = currentFrameInVolume;

          // we choose between V and V+1
          if (imageHeader.frameCount % (2 * m_rfImage3d.getFrameNumber()) >= m_rfImage3d.getFrameNumber() - 1) { // V+1
            m_rfImage3dTemp.getFrame(m_rfImage2d, framePositionInVolume);
            imageHeader.timeStamp = m_timestampsTemp.at(currentFrameInVolume) + m_pauseDurationOffset;
            if (m_rfImage3dTemp.getFrameNumber() ==
                currentFrameInVolume + 1) { // we're sending last frame of the volume
              m_nextImageTimestamp =
                  imageHeader.timeStamp + 40; // next delta is 40ms (we don't know timestamp of first image of V+2
              if (m_volumePauseTmp)
                m_pauseOn = false;
            } else
              m_nextImageTimestamp = m_timestampsTemp.at(currentFrameInVolume + 1) + m_pauseDurationOffset;
          } else {
            m_rfImage3d.getFrame(m_rfImage2d, framePositionInVolume);
            imageHeader.timeStamp = m_timestamps.at(currentFrameInVolume) + m_pauseDurationOffset;
            if (m_rfImage3dTemp.getFrameNumber() == currentFrameInVolume + 1) // we're sending last frame of the volume
              m_nextImageTimestamp = m_timestampsTemp.at(currentFrameInVolume + 1) +
                                     m_pauseDurationOffset; // next timestamp is first of V+1
            else
              m_nextImageTimestamp = m_timestamps.at(currentFrameInVolume + 1) + m_pauseDurationOffset;
          }
          // time offset due to pause
          uint64_t deltaT = m_nextImageTimestamp - imageHeader.timeStamp;
          m_pauseDurationOffset += deltaT;
          m_pauseIndexOffset++;
        } else if (m_useRewind) {
          // counting the number of total sequence sent
          unsigned int currentNumberOfWholeSequenceSent = 0;
          if (imageHeader.frameCount >=
              (unsigned int)m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber()) {
            currentNumberOfWholeSequenceSent =
                ((imageHeader.frameCount - (m_MHDSequenceReader.getTotalImageNumber() * m_rfImage3d.getFrameNumber())) /
                 ((m_MHDSequenceReader.getTotalImageNumber() - 1) * m_rfImage3d.getFrameNumber())) +
                1;
          }

          if (currentNumberOfWholeSequenceSent == 0) {
            // get frame
            if ((imageHeader.frameCount / m_rfImage3d.getFrameNumber()) % 2 == 1) {
              m_rfImage3d.getFrame(m_rfImage2d, m_rfImage3d.getFrameNumber() - currentFrameInVolume - 1);
            } else {
              m_rfImage3d.getFrame(m_rfImage2d, currentFrameInVolume);
            }

            // timestamp
            imageHeader.timeStamp = m_timestamps.at(currentFrameInVolume) + m_pauseDurationOffset;
            if (m_rfImage3d.getFrameNumber() == currentFrameInVolume + 1) { // we're sending last frame of the volume
              if (m_MHDSequenceReader.end()) {                              // last frame of last volume
                m_nextImageTimestamp = imageHeader.timeStamp + (m_timestamps.at(currentFrameInVolume) -
                                                                m_timestamps.at(currentFrameInVolume - 1));
              } else if ((imageHeader.frameCount / m_rfImage3d.getFrameNumber()) % 2 == 0) { // nex volume is odd
                m_nextImageTimestamp =
                    m_MHDSequenceReader.getNextTimeStamps().back(); // next timestamp is last of next volume
              } else {                                              // next volume is even
                m_nextImageTimestamp =
                    m_MHDSequenceReader.getNextTimeStamps().back(); // next timestamp is first of next volume
              }
            } else if (m_timestamps.size() != currentFrameInVolume + 1)
              m_nextImageTimestamp = m_timestamps.at(currentFrameInVolume + 1);

          } else if (currentNumberOfWholeSequenceSent % 2 == 0) { // we go from volume 1 to N-1
            // get frame
            if ((imageHeader.frameCount / m_rfImage3d.getFrameNumber()) % 2 == 1) {
              m_rfImage3d.getFrame(m_rfImage2d, m_rfImage3d.getFrameNumber() - currentFrameInVolume - 1);
            } else {
              m_rfImage3d.getFrame(m_rfImage2d, currentFrameInVolume);
            }

            // timestamp
            uint64_t delta;
            uint64_t deltaNext;
            if (m_rfImage3d.getFrameNumber() == currentFrameInVolume + 1) { // we're sending last frame of the volume
              delta = m_timestamps.at(currentFrameInVolume) - m_timestamps.at(currentFrameInVolume - 1);
              deltaNext = delta;                    // we consider same delta for convinience
            } else if (currentFrameInVolume == 0) { // first frame of the volume
              deltaNext = m_timestamps.at(currentFrameInVolume + 1) - m_timestamps.at(currentFrameInVolume);
              delta = deltaNext; // we consider same delta for convinience
            } else {
              delta = m_timestamps.at(currentFrameInVolume) - m_timestamps.at(currentFrameInVolume - 1);
              deltaNext = m_timestamps.at(currentFrameInVolume + 1) - m_timestamps.at(currentFrameInVolume);
            }
            imageHeader.timeStamp += delta;
            m_nextImageTimestamp = imageHeader.timeStamp + deltaNext;
          } else { // we go from volume N-2 to 0
            // get frame
            if ((imageHeader.frameCount / m_rfImage3d.getFrameNumber()) % 2 == 1) {
              m_rfImage3d.getFrame(m_rfImage2d, m_rfImage3d.getFrameNumber() - currentFrameInVolume - 1);
            } else {
              m_rfImage3d.getFrame(m_rfImage2d, currentFrameInVolume);
            }

            // timestamp
            uint64_t delta;
            uint64_t deltaNext;
            if (m_rfImage3d.getFrameNumber() == currentFrameInVolume + 1) { // we're sending last frame of the volume
              delta = m_timestamps.at(currentFrameInVolume) - m_timestamps.at(currentFrameInVolume - 1);
              deltaNext = delta;                    // we consider same delta for convinience
            } else if (currentFrameInVolume == 0) { // first frame of the volume
              deltaNext = m_timestamps.at(currentFrameInVolume + 1) - m_timestamps.at(currentFrameInVolume);
              delta = deltaNext; // we consider same delta for convinience
            } else {
              delta = m_timestamps.at(currentFrameInVolume) - m_timestamps.at(currentFrameInVolume - 1);
              deltaNext = m_timestamps.at(currentFrameInVolume + 1) - m_timestamps.at(currentFrameInVolume);
            }
            imageHeader.timeStamp += delta;
            m_nextImageTimestamp = imageHeader.timeStamp + deltaNext;
          }
        } else {
          if ((imageHeader.frameCount / m_rfImage3d.getFrameNumber()) % 2 == 1)
            m_rfImage3d.getFrame(m_rfImage2d, m_rfImage3d.getFrameNumber() - currentFrameInVolume - 1);
          else
            m_rfImage3d.getFrame(m_rfImage2d, currentFrameInVolume);

          // timestamps
          imageHeader.timeStamp = m_timestamps.at(currentFrameInVolume) + m_pauseDurationOffset;
          if (m_rfImage3d.getFrameNumber() == currentFrameInVolume + 1) { // we're sending last frame of the volume
            if (m_MHDSequenceReader.end())                                // last frame of last volume
              m_nextImageTimestamp = imageHeader.timeStamp;
            else if ((imageHeader.frameCount / m_rfImage3d.getFrameNumber()) % 2 == 0) { // nex volume is odd
              m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamps().back() +
                                     m_pauseDurationOffset; // next timestamp is last of next volume
            } else {                                        // next volume is even
              m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamps().back() +
                                     m_pauseDurationOffset; // next timestamp is first of next volume
            }
          } else if (m_timestamps.size() != currentFrameInVolume + 1)
            m_nextImageTimestamp = m_timestamps.at(currentFrameInVolume + 1) + m_pauseDurationOffset;
        }
        imageHeader.dataRate = 1000.0 / (m_nextImageTimestamp - imageHeader.timeStamp);

        out << imageHeader.headerId;
        out << imageHeader.frameCount;
        out << imageHeader.timeStamp;
        out << imageHeader.dataRate;
        out << (int)m_rfImage3d.getDimX() * m_rfImage3d.getDimY() * 2; // datalength in bytes
        out << (int)16;                                                // sample size in bits
        out << (int)2;                                                 // image type
        out << m_rfImage3d.getDimX();
        out << m_rfImage3d.getDimY();
        out << (double).0; // pixelWidth
        out << (double).0; // pixelHeight
        out << m_rfImage3d.getTransmitFrequency();
        out << m_rfImage3d.getSamplingFrequency();
        out << m_rfImage3d.getTransducerRadius();
        out << m_rfImage3d.getScanLinePitch();
        out << (int)m_rfImage3d.getScanLineNumber();
        out << (int)(m_rfImage3d.getDepth() * 1000.0);           // int in mm
        out << (double)vpMath::deg(m_rfImage3d.getFramePitch()); // degPerFrame
        out << (int)m_rfImage3d.getFrameNumber();                // framesPerVolume
        out << (double)m_rfImage3d.getMotorRadius();             // motorRadius
        out << (int)m_rfImage3d.getMotorType();                  // motorType
        out.writeRawData((char *)m_rfImage2d.bitmap, (int)m_rfImage2d.getHeight() * m_rfImage2d.getWidth() * 2);

        connectionSoc->write(block);
        qApp->processEvents();

        std::cout << "new frame sent, No " << imageHeader.frameCount << std::endl;

        imageHeader.frameCount++;
        currentFrameInVolume++;
        endOfVolume = m_rfImage3d.getFrameNumber() == currentFrameInVolume;
        endOfSequence = (m_MHDSequenceReader.end() && endOfVolume && !m_useRewind);

        // WAITING PROCESS (to respect sequence timestamps)
        vpTime::wait((double)(m_nextImageTimestamp - imageHeader.timeStamp));
      }
    } else if (m_imageType == us::PRESCAN_3D) { // send pre-scan volume frame by frame
      bool endOfVolume = false;
      unsigned int currentFrameInVolume = 0;
      while (!endOfVolume) {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
      //out.setVersion(QDataStream::Qt_5_0);
      out.setVersion(QDataStream::Qt_4_8);
        // new frame to send
        if (m_pauseOn) { // pause case (we send volumes V, V+1, V, V+1, ...)
          // check if current volume is odd or even
          unsigned int framePositionInVolume;
          if ((imageHeader.frameCount / m_preScanImage3d.getFrameNumber()) % 2 == 1)
            framePositionInVolume = m_preScanImage3d.getFrameNumber() - currentFrameInVolume - 1;
          else
            framePositionInVolume = currentFrameInVolume;

          // we choose between V and V+1
          if (imageHeader.frameCount % (2 * m_preScanImage3d.getFrameNumber()) >=
              m_preScanImage3d.getFrameNumber() - 1) { // V+1
            m_preScanImage3dTemp.getFrame(m_preScanImage2d, framePositionInVolume);
            imageHeader.timeStamp = m_timestampsTemp.at(currentFrameInVolume) + m_pauseDurationOffset;
            if (m_preScanImage3dTemp.getFrameNumber() ==
                currentFrameInVolume + 1) { // we're sending last frame of the volume
              m_nextImageTimestamp =
                  imageHeader.timeStamp + 40; // next delta is 40ms (we don't know timestamp of first image of V+2
              if (m_volumePauseTmp)
                m_pauseOn = false;
            } else
              m_nextImageTimestamp = m_timestampsTemp.at(currentFrameInVolume + 1) + m_pauseDurationOffset;
          } else {
            m_preScanImage3d.getFrame(m_preScanImage2d, framePositionInVolume);
            imageHeader.timeStamp = m_timestamps.at(currentFrameInVolume) + m_pauseDurationOffset;
            if (m_preScanImage3dTemp.getFrameNumber() ==
                currentFrameInVolume + 1) // we're sending last frame of the volume
              m_nextImageTimestamp = m_timestampsTemp.at(currentFrameInVolume + 1) +
                                     m_pauseDurationOffset; // next timestamp is first of V+1
            else
              m_nextImageTimestamp = m_timestamps.at(currentFrameInVolume + 1) + m_pauseDurationOffset;
          }
          invertRowsColsOnPreScan();
          // time offset due to pause
          uint64_t deltaT = m_nextImageTimestamp - imageHeader.timeStamp;
          m_pauseDurationOffset += deltaT;
          m_pauseIndexOffset++;
        } else if (m_useRewind) {
          // counting the number of total sequence sent
          unsigned int currentNumberOfWholeSequenceSent = 0;
          if (imageHeader.frameCount >=
              (unsigned int)m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber()) {
            currentNumberOfWholeSequenceSent =
                ((imageHeader.frameCount -
                  (m_MHDSequenceReader.getTotalImageNumber() * m_preScanImage3d.getFrameNumber())) /
                 ((m_MHDSequenceReader.getTotalImageNumber() - 1) * m_preScanImage3d.getFrameNumber())) +
                1;
          }

          if (currentNumberOfWholeSequenceSent == 0) {
            // get frame
            if ((imageHeader.frameCount / m_preScanImage3d.getFrameNumber()) % 2 == 1) {
              m_preScanImage3d.getFrame(m_preScanImage2d, m_preScanImage3d.getFrameNumber() - currentFrameInVolume - 1);
            } else {
              m_preScanImage3d.getFrame(m_preScanImage2d, currentFrameInVolume);
            }

            invertRowsColsOnPreScan();

            // timestamp
            imageHeader.timeStamp = m_timestamps.at(currentFrameInVolume) + m_pauseDurationOffset;
            if (m_preScanImage3d.getFrameNumber() ==
                currentFrameInVolume + 1) {    // we're sending last frame of the volume
              if (m_MHDSequenceReader.end()) { // last frame of last volume
                m_nextImageTimestamp = imageHeader.timeStamp + (m_timestamps.at(currentFrameInVolume) -
                                                                m_timestamps.at(currentFrameInVolume - 1));
              } else if ((imageHeader.frameCount / m_preScanImage3d.getFrameNumber()) % 2 == 0) { // nex volume is odd
                m_nextImageTimestamp =
                    m_MHDSequenceReader.getNextTimeStamps().back(); // next timestamp is last of next volume
              } else {                                              // next volume is even
                m_nextImageTimestamp =
                    m_MHDSequenceReader.getNextTimeStamps().back(); // next timestamp is first of next volume
              }
            } else if (m_timestamps.size() != currentFrameInVolume + 1)
              m_nextImageTimestamp = m_timestamps.at(currentFrameInVolume + 1);

          } else if (currentNumberOfWholeSequenceSent % 2 == 0) { // we go from volume 1 to N-1
            // get frame
            if ((imageHeader.frameCount / m_preScanImage3d.getFrameNumber()) % 2 == 1) {
              m_preScanImage3d.getFrame(m_preScanImage2d, m_preScanImage3d.getFrameNumber() - currentFrameInVolume - 1);
            } else {
              m_preScanImage3d.getFrame(m_preScanImage2d, currentFrameInVolume);
            }

            invertRowsColsOnPreScan();

            // timestamp
            uint64_t delta;
            uint64_t deltaNext;
            if (m_preScanImage3d.getFrameNumber() ==
                currentFrameInVolume + 1) { // we're sending last frame of the volume
              delta = m_timestamps.at(currentFrameInVolume) - m_timestamps.at(currentFrameInVolume - 1);
              deltaNext = delta;                    // we consider same delta for convinience
            } else if (currentFrameInVolume == 0) { // first frame of the volume
              deltaNext = m_timestamps.at(currentFrameInVolume + 1) - m_timestamps.at(currentFrameInVolume);
              delta = deltaNext; // we consider same delta for convinience
            } else {
              delta = m_timestamps.at(currentFrameInVolume) - m_timestamps.at(currentFrameInVolume - 1);
              deltaNext = m_timestamps.at(currentFrameInVolume + 1) - m_timestamps.at(currentFrameInVolume);
            }
            imageHeader.timeStamp += delta;
            m_nextImageTimestamp = imageHeader.timeStamp + deltaNext;
          } else { // we go from volume N-2 to 0
            // get frame
            if ((imageHeader.frameCount / m_preScanImage3d.getFrameNumber()) % 2 == 1) {
              m_preScanImage3d.getFrame(m_preScanImage2d, m_preScanImage3d.getFrameNumber() - currentFrameInVolume - 1);
            } else {
              m_preScanImage3d.getFrame(m_preScanImage2d, currentFrameInVolume);
            }

            invertRowsColsOnPreScan();

            // timestamp
            uint64_t delta;
            uint64_t deltaNext;
            if (m_preScanImage3d.getFrameNumber() ==
                currentFrameInVolume + 1) { // we're sending last frame of the volume
              delta = m_timestamps.at(currentFrameInVolume) - m_timestamps.at(currentFrameInVolume - 1);
              deltaNext = delta;                    // we consider same delta for convinience
            } else if (currentFrameInVolume == 0) { // first frame of the volume
              deltaNext = m_timestamps.at(currentFrameInVolume + 1) - m_timestamps.at(currentFrameInVolume);
              delta = deltaNext; // we consider same delta for convinience
            } else {
              delta = m_timestamps.at(currentFrameInVolume) - m_timestamps.at(currentFrameInVolume - 1);
              deltaNext = m_timestamps.at(currentFrameInVolume + 1) - m_timestamps.at(currentFrameInVolume);
            }
            imageHeader.timeStamp += delta;
            m_nextImageTimestamp = imageHeader.timeStamp + deltaNext;
          }
        } else {
          if ((imageHeader.frameCount / m_preScanImage3d.getFrameNumber()) % 2 == 1)
            m_preScanImage3d.getFrame(m_preScanImage2d, m_preScanImage3d.getFrameNumber() - currentFrameInVolume - 1);
          else
            m_preScanImage3d.getFrame(m_preScanImage2d, currentFrameInVolume);
          invertRowsColsOnPreScan();
          // timestamps
          imageHeader.timeStamp = m_timestamps.at(currentFrameInVolume) + m_pauseDurationOffset;
          if (m_preScanImage3d.getFrameNumber() == currentFrameInVolume + 1) { // we're sending last frame of the volume
            if (m_MHDSequenceReader.end())                                     // last frame of last volume
              m_nextImageTimestamp = imageHeader.timeStamp;
            else if ((imageHeader.frameCount / m_preScanImage3d.getFrameNumber()) % 2 == 0) { // nex volume is odd
              m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamps().back() +
                                     m_pauseDurationOffset; // next timestamp is last of next volume
            } else {                                        // next volume is even
              m_nextImageTimestamp = m_MHDSequenceReader.getNextTimeStamps().back() +
                                     m_pauseDurationOffset; // next timestamp is first of next volume
            }
          } else if (m_timestamps.size() != currentFrameInVolume + 1)
            m_nextImageTimestamp = m_timestamps.at(currentFrameInVolume + 1) + m_pauseDurationOffset;
        }

        imageHeader.dataRate = 1000.0 / (m_nextImageTimestamp - imageHeader.timeStamp);

        out << imageHeader.headerId;
        out << imageHeader.frameCount;
        out << imageHeader.timeStamp;
        out << imageHeader.dataRate;
        out << (int)m_preScanImage3d.getDimX() * m_preScanImage3d.getDimY(); // datalength in bytes
        out << (int)8;                                                       // sample size in bits
        out << (int)0;
        out << m_preScanImage3d.getDimX();
        out << m_preScanImage3d.getDimY();
        out << (double).0; // pixelWidth
        out << (double).0; // pixelHeight
        out << m_preScanImage3d.getTransmitFrequency();
        out << m_preScanImage3d.getSamplingFrequency();
        out << m_preScanImage3d.getTransducerRadius();
        out << m_preScanImage3d.getScanLinePitch();
        out << (int)m_preScanImage3d.getScanLineNumber();
        out << (int)(m_preScanImage3d.getDepth() * 1000.0);           // int in mm
        out << (double)vpMath::deg(m_preScanImage3d.getFramePitch()); // degPerFrame
        out << (int)m_preScanImage3d.getFrameNumber();                // framesPerVolume
        out << (double)m_preScanImage3d.getMotorRadius();             // motorRadius
        out << (int)m_preScanImage3d.getMotorType();                  // motorType
        // invert row / cols to fit porta SDK (using member image pre-scan 2D
        out.writeRawData((char *)m_preScanImage2d.bitmap,
                         (int)m_preScanImage2d.getHeight() * m_preScanImage2d.getWidth());

        connectionSoc->write(block);
        qApp->processEvents();

        std::cout << "new frame sent, No " << imageHeader.frameCount << std::endl;

        imageHeader.frameCount++;
        currentFrameInVolume++;
        endOfVolume = m_preScanImage3d.getFrameNumber() == currentFrameInVolume;
        endOfSequence = (m_MHDSequenceReader.end() && endOfVolume && !m_useRewind);

        // WAITING PROCESS (to respect sequence timestamps)
        if (!endOfSequence)
          vpTime::wait((double)(m_nextImageTimestamp - imageHeader.timeStamp));
      }
    }
  }
}

/**
* Method to invert rows and columns in the image (case of pre-scan frames) : to fit real server behaviour.
*/
void usVirtualServer::invertRowsColsOnPreScan()
{
  usImagePreScan2D<unsigned char> temp = m_preScanImage2d;
  m_preScanImage2d.resize(temp.getWidth(), temp.getHeight());

  for (unsigned int i = 0; i < m_preScanImage2d.getHeight(); i++)
    for (unsigned int j = 0; j < m_preScanImage2d.getWidth(); j++)
      m_preScanImage2d(i, j, temp(j, i));
}

/**
* Slot called when the server receives a run/stop order.
*/
void usVirtualServer::runAcquisition(bool run)
{
  m_serverIsSendingImages = run;
  if (run) {
    emit(startSendingLoopSignal());
  }
}

/**
* Slot called when the user un-pauses the server (stop sending the same image).
*/
void usVirtualServer::quitPause()
{
  if (m_imageType == us::RF_2D || m_imageType == us::PRESCAN_2D || m_imageType == us::POSTSCAN_2D) { //(2D case)
    m_pauseOn = false;
  } else { // we wait the volume (V & V+1) is totally sent
    m_volumePauseTmp = true;
  }
}
