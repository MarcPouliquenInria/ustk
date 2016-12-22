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
 * @file us.h
 * @brief us namespace.
 */

#ifndef US_H
#define US_H

#include <visp3/core/vpConfig.h>
#include <visp3/ustk_core/usImagePreScanSettings.h>
#include <visp3/ustk_core/usMotorSettings.h>
/**
 * @namespace us
 * @brief General tools
 * @ingroup module_ustk_core
 */
namespace us {

/*! Enum to know the ultrasound image type
  Used in ultrasonix grabber to adapt the grabber when we receive the header, and in the mhd parser.*/
typedef enum {
  UNKNOWN = -1, /*!< Unkownn format. */
  NOT_SET,      /*!< Not set (usefull for mhd parser). */
  RF_2D,        /*!< Case of 2D RF image. */
  RF_3D,        /*!< Case of 3D RF image. */
  PRESCAN_2D,   /*!< Case of 2D pre-scan image. */
  PRESCAN_3D,   /*!< Case of 3D pre-scan image. */
  POSTSCAN_2D,  /*!< Case of 2D post-scan image. */
  POSTSCAN_3D,  /*!< Case of 3D post-scan image. */
}ImageType;

/*! Enum to describe the orientation of a 2D plane in a 3D image.*/
typedef enum {
  Xorientation, /*!< Plane normal to X axis. */
  Yorientation, /*!< Plane normal to Y axis. */
  Zorientation, /*!< Plane normal to Z axis. */
}Orientation;


  VISP_EXPORT void getUltrasonix4DC7MotorSettings(usMotorSettings &motorSettings);
  VISP_EXPORT void getUltrasonix4DC7PreScanSettings(usImagePreScanSettings &preScanSettings);
};


#endif // US_H

