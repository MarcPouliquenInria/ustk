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

#include <visp3/ustk_needle_detection/us3DNeedleModel.h>

#if defined USNEEDLEDETECTION_HAVE_VTK
#include <vtkPolyLine.h>
#include <vtkCellArray.h>
#endif

us3DNeedleModel::us3DNeedleModel() {
  m_order = 0;
  m_nRenderingLines = 0;
  m_controlPoints = vpMatrix();
  m_model = vpMatrix();
  m_renderingPoints = vpMatrix();
}

us3DNeedleModel::us3DNeedleModel(const us3DNeedleModel &needle) {
  m_order = needle.getOrder();
  m_nRenderingLines = needle.getNumberOfRenderingLines();
  m_controlPoints = needle.getControlPoints();
  m_model = needle.m_model;
  m_renderingPoints = needle.getRenderingPoints();
}

us3DNeedleModel::us3DNeedleModel(unsigned int order) {
  m_order = order;
  if (m_order < 2) {
    std::cerr << "Error: the order of the polynomial curve should be at least 2." << std::endl;
    m_order = 2;
    m_nRenderingLines = 1;
  }
  else if (m_order == 2)
    m_nRenderingLines = 1;
  else
    m_nRenderingLines = 10;
  m_controlPoints = vpMatrix(3, m_order);
  m_model = vpMatrix(3, order);
  m_renderingPoints = vpMatrix(3, m_nRenderingLines+1);
}

void us3DNeedleModel::setOrder(unsigned int order) {
  m_order = order;
  if (m_order < 2)
    std::cerr << "Error: the order of the polynomial curve should be at least 2." << std::endl;
  else if (m_order == 2)
    m_nRenderingLines = 1;
  else
    m_nRenderingLines = 10;
  m_controlPoints = vpMatrix(3, m_order);
  m_renderingPoints = vpMatrix(3, m_nRenderingLines+1);
  m_model = vpMatrix(3, order);
}

unsigned int us3DNeedleModel::getOrder() const { return m_order; }

void us3DNeedleModel::setNumberOfRenderingLines(unsigned int nRenderingLines) {
  m_nRenderingLines = nRenderingLines;
}

unsigned int us3DNeedleModel::getNumberOfRenderingLines() const { return m_nRenderingLines; }

void us3DNeedleModel::setControlPoints(const vpMatrix &controlPoints) {
  if (controlPoints.getCols() != 3) {
    std::cerr << "Error: in us3DNeedleModel::setControlPoints(): "
	      << "Input matrix should of size nx3." << std::endl;
    exit(EXIT_FAILURE);
  }
  if (controlPoints.getRows() != m_order) {
    //    std::cerr << "Error: in us3DNeedleModel::setControlPoints(): "
    //	      << "The size of the input matrix does not match the order of the model."
    //	      << std::endl;
    //exit(EXIT_FAILURE);
    setOrder(controlPoints.getRows());
  }

  m_controlPoints = controlPoints.t();
  
  if ((m_controlPoints.getCol(m_order-1) - m_controlPoints.getCol(0)).euclideanNorm() == 0.0) {
    std::cerr << "Warning: in us3DNeedleModel::setControlPoints(): needle model is degenerate."
	      << std::endl;
    for (unsigned int i=0; i<3; ++i) {
      m_model[i][0] = m_controlPoints[i][0];
      m_model[i][1] = 0.0;
      m_model[i][2] = 0.0;
    }
  } else {
    if (m_order!=6) {
      vpMatrix tapprox = usNeedleDetectionTools::approximateCoordinates(m_controlPoints.t(),
									m_controlPoints.t(),
									m_order);
      m_model = m_controlPoints * tapprox.inverseByLU();
    }
    else {
      double norm = (m_controlPoints.getCol(2) - m_controlPoints.getCol(0)).euclideanNorm();
      if (norm == 0.0) {
	std::cerr << "Warning: in us3DNeedleModel::setControlPoints(): needle model is degenerate."
		  << std::endl;
	norm = 1;
	for (unsigned int i=0; i<3; ++i) {
	  m_model[i][0] = m_controlPoints[i][0];
	  m_model[i][1] = 0.0;
	  m_model[i][2] = 0.0;
	}
      } else {
	double t = (m_controlPoints.getCol(1) - m_controlPoints.getCol(0)).euclideanNorm() / norm;
	if (t == 0.0) {
	  std::cerr << "Warning: in us3DNeedleModel::setControlPoints(): needle model is degenerate."
		    << std::endl;
	  for (unsigned int i=0; i<3; ++i) {
	    m_model[i][0] = m_controlPoints[i][0];
	    m_model[i][1] = 0.0;
	    m_model[i][2] = 0.0;
	  }
	} else {
	  for (unsigned int i=0; i<3; ++i) {
	    m_model[i][0] = m_controlPoints[i][0];
	    m_model[i][2] = (t * (m_controlPoints[i][2]-m_model[i][0]) - m_controlPoints[i][1])
	      / (t * (1.0 - t) + 1e-10);
	    m_model[i][1] = m_controlPoints[i][2] - m_model[i][0] - m_model[i][2];
	  }
	}
      }
    }
  }
  vpMatrix coords = vpMatrix(m_order, m_nRenderingLines+1);
  for (unsigned int j=0; j<m_nRenderingLines+1; ++j) {
    coords[0][j] = 1.0;
    double t = static_cast<double>(j) / static_cast<double>(m_nRenderingLines);
    for (unsigned int i=1; i<m_order; ++i) 
      coords[i][j]  = coords[i-1][j] * t;
  }
  m_renderingPoints = m_model * coords;
}

void us3DNeedleModel::setControlPoints(double **controlPoints) {
  for (unsigned int i=0; i<3; ++i)
    for (unsigned int j=0; j<m_order; ++j)
      m_controlPoints[i][j] = controlPoints[j][i];
  vpMatrix tapprox = usNeedleDetectionTools::approximateCoordinates(m_controlPoints.t(), m_controlPoints.t(), m_order);
  m_model = m_controlPoints * tapprox.inverseByLU();
  vpMatrix coords = vpMatrix(m_order, m_nRenderingLines+1);
  
  for (unsigned int j=0; j<m_nRenderingLines+1; ++j) {
    coords[0][j] = 1.0;
    double t = static_cast<double>(j) / static_cast<double>(m_nRenderingLines);
    for (unsigned int i=1; i<m_order; ++i) 
      coords[i][j]  = coords[i-1][j] * t;
  }
  m_renderingPoints = m_model * coords;
}

vpMatrix us3DNeedleModel::getControlPoints() const { return m_controlPoints; }
vpMatrix us3DNeedleModel::getRenderingPoints() const
{
  vpMatrix points = m_renderingPoints;
  return points; }

void us3DNeedleModel::setModel(const vpMatrix &model) {
  m_model = model;
  vpMatrix coords(m_order,m_order);
  for (unsigned int j=0; j<m_order; ++j) {
    coords[0][j] = 1.0;
    double t = static_cast<double>(j) / static_cast<double>(m_order-1);
    for (unsigned int i=1; i<m_order; ++i) 
      coords[i][j]  = coords[i-1][j] * t;
  }
  m_controlPoints = m_model * coords;
  coords = vpMatrix(m_order, m_nRenderingLines+1);
  for (unsigned int j=0; j<m_nRenderingLines+1; ++j) {
    coords[0][j] = 1.0;
    double t = static_cast<double>(j) / static_cast<double>(m_nRenderingLines);
    for (unsigned int i=1; i<m_order; ++i) 
      coords[i][j]  = coords[i-1][j] * t;
  }
  m_renderingPoints = m_model * coords;
}

vpMatrix *us3DNeedleModel::getModel() { return &m_model; }

#if defined USNEEDLEDETECTION_HAVE_VTK
vtkSmartPointer<vtkPolyData> us3DNeedleModel::getPolyData() const {
  vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
  points->SetNumberOfPoints(m_nRenderingLines+1);
  vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
  polyLine->GetPointIds()->SetNumberOfIds(m_nRenderingLines+1);
  vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
  vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
  cells->Initialize();
  for (unsigned int i=0; i<m_nRenderingLines+1; ++i) {
    points->SetPoint(i, m_renderingPoints[0][i], m_renderingPoints[1][i], m_renderingPoints[2][i]);
    polyLine->GetPointIds()->SetId(i,i);
  }
  cells->InsertNextCell(polyLine);
  polyData->SetPoints(points);
  polyData->SetLines(cells);
  return polyData;
}
#endif

vpColVector us3DNeedleModel::getPoint(double t) const {
  vpColVector T(m_order);
  T[0] = 1.0;
  for (unsigned int i=1; i<m_order; ++i)
    T[i] = T[i-1] * t;
  return m_model * T;
}

vpColVector us3DNeedleModel::getTangent(double t) const {
  vpColVector T(m_order);
  double tt = 1.0;
  T[0] = 0.0;
  for (unsigned int i=1; i<m_order; ++i) {
    T[i] = i * tt;
    tt *= t;
  }
  return m_model * T;
}

double us3DNeedleModel::getLength() const {
  vpMatrix coords(m_order, 50);
  for (unsigned int j=0; j<50; ++j) {
    coords[0][j] = 1.0;
    double t = static_cast<double>(j) / 49.0;
    for (unsigned int i=1; i<m_order; ++i) 
      coords[i][j]  = coords[i-1][j] * t;
  }
  vpMatrix points = m_model * coords;
  double length = 0.0;
  for (unsigned int i=0; i<49; ++i)
    length += (points.getCol(i) - points.getCol(i+1)).euclideanNorm();
  return length;
};

double us3DNeedleModel::curveDistance(us3DNeedleModel &n1, us3DNeedleModel &n2) {
  unsigned int order1 = n1.getOrder();
  unsigned int order2 = n2.getOrder();
  vpMatrix coords1(order1, 50);
  vpMatrix coords2(order2, 50);
  for (unsigned int j=0; j<50; ++j) {
    coords1[0][j] = 1.0;
    coords2[0][j] = 1.0;
    double t = static_cast<double>(j) / 49.0;
    for (unsigned int i=1; i<order1; ++i) 
      coords1[i][j]  = coords1[i-1][j] * t;
    for (unsigned int i=1; i<order2; ++i) 
      coords2[i][j]  = coords2[i-1][j] * t;
  }
  vpMatrix p1 = (*n1.getModel()) * coords1;
  vpMatrix p2 = (*n2.getModel()) * coords2;
  double distance = 0.0;
  for (unsigned int i=0; i<50; ++i)
    distance += (p1.getCol(i) - p2.getCol(i)).euclideanNorm();
  distance /= 50;
  return distance;
}

double us3DNeedleModel::getCurvature() {
  vpMatrix coords(m_order, 50);
  for (unsigned int j=0; j<50; ++j) {
    coords[0][j] = 1.0;
    double t = static_cast<double>(j) / 49.0;
    for (unsigned int i=1; i<m_order; ++i) 
      coords[i][j]  = coords[i-1][j] * t;
  }
  vpMatrix p1 = m_model * coords;
  vpColVector p1p2 = m_controlPoints.getCol(m_order-1) - m_controlPoints.getCol(0);
  vpColVector p2;
  double curvature = 0.0;
  for (unsigned int i=0; i<50; ++i) {
    p2 = m_controlPoints.getCol(0) + static_cast<double>(i) / 49.0 * p1p2;
    curvature += (p1.getCol(i) - p2).euclideanNorm();
  }
  curvature /= 50;
  return curvature;
}

us3DNeedleModel us3DNeedleModel::changePolynomialOrder(unsigned int newOrder) {
  us3DNeedleModel newNeedleModel(newOrder);
  vpMatrix newControlPoints;
  vpMatrix coords(m_order,newOrder);
  for (unsigned int j=0; j<newOrder; ++j) {
    coords[0][j] = 1.0;
    double t = static_cast<double>(j) / static_cast<double>(newOrder-1);
    for (unsigned int i=1; i<m_order; ++i) 
      coords[i][j]  = coords[i-1][j] * t;
  }
  //m_model.print(std::cerr, 5, "MODEL");
  //coords.print(std::cerr, 5, "T");
  newControlPoints = m_model * coords;
  //newControlPoints.print(std::cerr, 5, "CP");
  newNeedleModel.setControlPoints(newControlPoints.t());
  return newNeedleModel;
}
