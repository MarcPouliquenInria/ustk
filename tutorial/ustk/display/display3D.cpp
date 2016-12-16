/****************************************************************************
 *
 * This file is part of the UsNeedleDetection software.
 * Copyright (C) 2013 - 2016 by Inria. All rights reserved.
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
 * Authors:
 * Pierre Chatelain
 * Alexandre Krupa
 *
 *****************************************************************************/

//ustk
#include <visp3/ustk_io/usImageIo.h>
#include <visp3/ustk_gui/usViewer3D.h>
#include <visp3/ustk_gui/usViewer2D.h>
#include <vtkMatrix4x4.h>
#include <vtkAbstractTransform.h>


int main()
{
  usViewer3D viewer3D = usViewer3D();

  usViewer2D viewer2DX = usViewer2D(us::Xorientation,(int)*viewer3D.getXSliceOrigin());
  usViewer2D viewer2DY = usViewer2D(us::Yorientation,(int)viewer3D.getYSliceOrigin()[1]);
  usViewer2D viewer2DZ = usViewer2D(us::Zorientation,(int)viewer3D.getZSliceOrigin()[2]);

  viewer3D.start();
  viewer2DX.start();
  viewer2DY.start();
  viewer2DZ.start();

  return 0;
}
