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

/**
 * @file usDataGrabbed.h
 * @brief Class to store additionnal informations arriving on the network with ultrasound images grabbed (frame count, timestamp ...).
 */

#ifndef __usDataGrabbed_h_
#define __usDataGrabbed_h_


#include <visp3/ustk_grabber/usGrabberConfig.h>

#if defined(USTK_GRABBER_HAVE_QT5)

#include <visp3/ustk_grabber/usNetworkGrabber.h>

#include <visp3/core/vpMutex.h>

/**
 * @class usDataGrabbed
 * @brief Class to store additionnal informations arriving on the network with ultrasound images grabbed, such as frame count, timestamp.
 * Usefull to do real-time process.
 * @ingroup module_ustk_grabber
 */
template<class Type>
class VISP_EXPORT usDataGrabbed : public Type
{
public:

  explicit usDataGrabbed();
  ~usDataGrabbed();

  double getDataRate() const;
  quint32 getFrameCount() const;
  int getFramesPerVolume() const;
  quint64 getTimeStamp() const;

  void setDataRate(double dataRate);
  void setFrameCount(quint32 frameCount);
  void setFramesPerVolume(int framesPerVolume);
  void setTimeStamp(quint64 timeStamp);

  vpMutex mutex; // security to access the usImage grabbed from different threads

private:
  quint32 m_frameCount; //from the beginning of acquisition
  quint64 m_timeStamp; //msecs since epoch (on ultrasond machine)
  int m_framesPerVolume; //number of frames in a volume (for 3D case)
  double m_dataRate;
};

/**
* Constructor.
*/
template<class Type>
usDataGrabbed<Type>::usDataGrabbed() :
  Type(), mutex(), m_frameCount(0), m_timeStamp(0), m_framesPerVolume(0)
{

}

/**
* Destructor.
*/
template<class Type>
usDataGrabbed<Type>::~usDataGrabbed()
{

}

/**
* Frame count getter.
* @return The frame number since beginning of acquisition.
*/
template<class Type>
quint32 usDataGrabbed<Type>::getFrameCount() const {
  return m_frameCount;
}

/**
* Frame per volume getter.
* @return The number of frames per volume acquired. Used to reconstruct 3D volumes.
*/
template<class Type>
int usDataGrabbed<Type>::getFramesPerVolume() const{
  return m_framesPerVolume;
}

/**
* Time stamp getter.
* @return The time stamp of ultrasound station when the frame was acquired.
*/
template<class Type>
quint64 usDataGrabbed<Type>::getTimeStamp() const{
  return m_timeStamp;
}

/**
* Frame count setter.
* @param frameCount The frame number since beginning of acquisition.
*/
template<class Type>
void usDataGrabbed<Type>::setFrameCount(quint32 frameCount) {
  m_frameCount = frameCount;
}

/**
* Frame per volume setter.
* @param framesPerVolume The number of frames per volume acquired. Used to reconstruct 3D volumes.
*/
template<class Type>
void usDataGrabbed<Type>::setFramesPerVolume(int framesPerVolume){
  m_framesPerVolume = framesPerVolume;
}

/**
* Time stamp setter.
* @param timeStamp The time stamp of ultrasound station when the frame was acquired.
*/
template<class Type>
void usDataGrabbed<Type>::setTimeStamp(quint64 timeStamp){
  m_timeStamp = timeStamp;
}

/**
* Data rate getter.
* @return dataRate The data rate of ultrasound station between the 2 last frames.
*/
template<class Type>
double usDataGrabbed<Type>::getDataRate() const {
  return m_dataRate;
}

/**
* Data rate setter.
* @param dataRate The data rate of ultrasound station between the 2 last frames.
*/
template<class Type>
void usDataGrabbed<Type>::setDataRate(double dataRate) {
  m_dataRate = dataRate;
}

/*!
  Print data grabbed information in a ostream.
  Usage example:
  \code
  usDataGrabbed<Type> myDataGrabbed;
  std::cout << myDataGrabbed << std::endl;
  \endcode
*/
template<class Type>
VISP_EXPORT std::ostream& operator<<(std::ostream& out, const usDataGrabbed<Type> &other)
{
  out << "timestamp : " << other.getTimeStamp() << std::endl
      << "frameCount : " << other.getFrameCount() << std::endl
      << "frames per volume : " << other.getFramesPerVolume() << std::endl
      << (Type)other << std::endl;

  return out;
}


#endif // QT4 || QT5
#endif // __usDataGrabbed_h_
