/****************************************************************************
 *
 * This file is part of the UsTk software.
 * Copyright (C) 2014 by Inria. All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License ("GPL") as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * See the file COPYING at the root directory of this source
 * distribution for additional information about the GNU GPL.
 * 
 * This software was developed at:
 * INRIA Rennes - Bretagne Atlantique
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * France
 * http://www.irisa.fr/lagadic
 *
 * If you have questions regarding the use of this file, please contact the
 * authors at Alexandre.Krupa@inria.fr
 * 
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 * Authors:
 * Pierre Chatelain
 *
 *****************************************************************************/

/**
 * @file usBackScanConverter2D.h
 * @brief 2D scan-converter
 * @author Pierre Chatelain
 */

#ifndef US_BACK_SCAN_CONVERTER_2D_H
#define US_BACK_SCAN_CONVERTER_2D_H

#include <visp3/ustk_core/usImagePreScan2D.h>
#include <visp3/ustk_core/usImagePostScan2D.h>


/**
 * @class usBackScanConverter2D
 * @brief 2D back-scan converter
 * @author Pierre Chatelain
 *
 * This class allows to convert 2D postscan ultrasound images to prescan.
 * The converter should be initialized through init() and then applied through run().
 */
class VISP_EXPORT usBackScanConverter2D
{
 public:

  usBackScanConverter2D();

  ~usBackScanConverter2D();

  void init(const usImagePostScan2D<unsigned char> &inputSettings, const int BModeSampleNumber, const int scanLineNumber);
  void init(const usTransducerSettings &inputSettings, const double BModeSampleNumber,
            const double scanLineNumber,const double xResolution, const double yResolution);

  void run(const usImagePostScan2D<unsigned char> &imageToConvert, usImagePreScan2D<unsigned char> &imageConverted);

 private:
  vpMatrix m_iMap;
  vpMatrix m_jMap;
  double m_xResolution;
  double m_yResolution;
  int m_scanLineNumber;
  int m_BModeSampleNumber;
  usTransducerSettings m_initSettings;

  double interpolateLinear(const vpImage<unsigned char>& I, double x, double y);
};

#endif // US_BACK_SCAN_CONVERTER_2D_H