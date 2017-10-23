#ifndef US_VIRTUAL_SERVER_H
#define US_VIRTUAL_SERVER_H

#include <QApplication>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtCore/QDataStream>
#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QFile>

#include <vector>
#include <cmath>
#include <ctime>
#include <iostream>
//#include <unistd.h>

//USTK inclues
#include <visp3/ustk_io/usSequenceReader.h>
#include <visp3/ustk_io/usMHDSequenceReader.h>

class usVirtualServer : public QObject
{
  Q_OBJECT

public:

  // Following headers must be the same in the client grabber !

  //header sent by the client to init porta
  struct usInitHeaderIncomming{
    usInitHeaderIncomming() : headerId(1) {} // set header Id to 1 by default
    int headerId; //to differenciate usInitHeaderIncomming (=1) / usAcquisitionParameters (=2) / usRunControlHeaderSent(=3)

    //probe / slot selection
    int probeId; // available probes : 4DC7 (id = 15) , C5-2 (id = 10)
    int slotId; // 3 slots on ultrasonix station (0 : top, 2 : middle, 1 : botom)

    int imagingMode; // see ImagingModes.h
  };

  //header sent by the client to update porta config
  struct usUpdateHeaderIncomming{
    usUpdateHeaderIncomming() : headerId(2) {} // set header Id to 1 by default
    int headerId; //to differenciate usInitHeaderIncomming (=1) / usUpdateHeaderIncomming (=2)

    int transmitFrequency;
    int samplingFrequency;
    int imagingMode;
    bool postScanMode;
    int postScanHeight;
    int postScanWidth;
    int imageDepth;
    int sector;
    bool activateMotor;
    int motorPosition;
    int framesPerVolume;
    int stepsPerFrame;
  };

  //header sent to the client to confirm porta is initialized
  struct usInitHeaderConfirmation{
    usInitHeaderConfirmation() : headerId(1) {} // set header Id to 1 by default
    int headerId; //to differenciate usInitHeaderConfirmation (=1) / usImageHeader (=2)

    int initOk; // 1 if init ok, 0 otherwise
    int probeId; // unique code for each probe (4DC7 = 15, C 50-62 = 10)
  };

  //
  struct usImageHeader{
    usImageHeader() : headerId(2) {} //set header Id to 2 by default
    int headerId; //to differenciate usInitHeaderConfirmation (=1) / usImageHeader (=2)

    quint32 frameCount;
    quint64 timeStamp;

    double dataRate; // fps

    int dataLength; //image size in bytes
    int ss;

    int imageType;

    int frameWidth;
    int frameHeight;

    double pixelWidth; // width of a pixel of a post-scan frame (meters)
    double pixelHeight; // height of a pixel of a post-scan frame (meters)

    int transmitFrequency;
    int samplingFrequency;

    double transducerRadius;
    double scanLinePitch;
    unsigned int scanLineNumber;
    int imageDepth;

    double degPerFrame;
    int framesPerVolume;
    double motorRadius;
    int motorType;
  };

  explicit usVirtualServer(std::string sequencePath, QObject *parent = 0);
  ~usVirtualServer();

  QTcpSocket* getSocket();

signals:
  void runAcquisitionSignal(bool run);

  void startSendingLoopSignal();

private slots:
  // Called automatically when a client attempts to connect
  void acceptTheConnection();

  // Called automatically when client has closed the connection
  void connectionAboutToClose();

  // Called automatically when data sent by a client is fully available to the server
  void readIncomingData();

  void runAcquisition(bool run);

  void startSendingLoop();

private:

  void initServer(usInitHeaderIncomming header);

  void invertRowsColsOnPreScan();

  void setSequencePath(const std::string sequencePath);

  void sendingLoopSequenceXml();
  void sendingLoopSequenceMHD();

  bool updateServer(usUpdateHeaderIncomming header);

  void writeInitAcquisitionParameters(QDataStream & out, int imagingMode);

  // Variable(socket) to store listening tcpserver
  QTcpServer m_tcpServer;

  // Variable(socket) to store newly established connection with the client
  QTcpSocket * connectionSoc;
  usInitHeaderConfirmation confirmHeader;

  bool initWithoutUpdate;

  usSequenceReader<usImagePostScan2D <unsigned char> > m_sequenceReaderPostScan;
  usSequenceReader<usImagePreScan2D <unsigned char> > m_sequenceReaderPreScan;
  usMHDSequenceReader m_MHDSequenceReader;

  usImageHeader imageHeader;

  usImagePostScan2D <unsigned char> m_postScanImage2d;
  usImagePreScan2D <unsigned char> m_preScanImage2d;
  usImageRF2D <short int> m_rfImage2d;
  usImagePostScan3D <unsigned char> m_postScanImage3d;
  usImagePreScan3D <unsigned char> m_preScanImage3d;
  usImageRF3D <short int> m_rfImage3d;

  us::ImageType m_imageType;

  bool m_isMHDSequence; // mhd sequence if true, xml otherwise

  int64_t m_previousImageTimestamp;

  bool m_serverIsSendingImages;

  std::string m_sequencePath;
};

#endif // US_VIRTUAL_SERVER_H