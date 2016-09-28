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
 * Marc Pouliquen
 *
 *****************************************************************************/

/**
 * @file usImageIo.h
 * @brief Input/output operations between ultrasound data and image files and their associated header (containing usImageSettings info).
 */

#ifndef US_IMAGE_IO_H
#define US_IMAGE_IO_H

#include <string>

#include <visp3/ustk_core/usImageRF2D.h>
#include <visp3/ustk_core/usImageRF3D.h>
#include <visp3/ustk_core/usImagePreScan2D.h>
#include <visp3/ustk_core/usImagePreScan3D.h>
#include <visp3/ustk_core/usImagePostScan2D.h>
#include <visp3/ustk_core/usImagePostScan3D.h>
#include <visp3/ustk_io/usMetaHeaderParser.h>


/**
 * @class usImageIo
 * @brief Input/output operations between ultrasound data and files (header + image file).
 */
class VISP_EXPORT usImageIo
{
public:

  //RF
  static void write(const usImageRF2D &rfImage, const std::string filename);
  static void read(usImageRF2D &rfImage,const std::string filename);
  static void write(const usImageRF3D &rfImage3D, const std::string filename);
  static void read(usImageRF3D &rfImage3D, const std::string filename);

  //PreScan-unsigned char
#ifdef VISP_HAVE_XML2
  static void writeXml(const usImagePreScan2D<unsigned char> &preScanImage, const std::string xmlFilename);
  static void readXml(usImagePreScan2D<unsigned char> &preScanImage,const std::string xmlFilename);
#endif //VISP_HAVE_XML2
  static void write(const usImagePreScan3D<unsigned char> &preScanImage3D, const std::string filename);
  static void read(usImagePreScan3D<unsigned char> &preScanImage3D, const std::string filename);

  //PreScan-double
  static void write(const usImagePreScan2D<double> &preScanImage, const std::string filename);
  static void read(usImagePreScan2D<double> &preScanImage,const std::string filename);
  static void write(const usImagePreScan3D<double> &preScanImage3D, const std::string filename);
  static void read(usImagePreScan3D<double> &preScanImage3D,const std::string filename);

  //postScan
#ifdef VISP_HAVE_XML2
  static void writeXml(const usImagePostScan2D &postScanImage, const std::string filename);
  static void readPostScan2DFromXml(usImagePostScan2D &postScanImage, const std::string filename);
#endif //VISP_HAVE_XML2
  static void write(const usImagePostScan3D &postScanImage3D, const std::string filename);
  static void read(const usImagePostScan3D &postScanImage3D,std::string mhdFileName);
};

#endif //US_IMAGE_IO_H
