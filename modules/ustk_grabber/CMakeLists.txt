#############################################################################
#
# This file is part of the ustk software.
# Copyright (C) 2016 - 2017 by Inria. All rights reserved.
#
# This software is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# ("GPL") version 2 as published by the Free Software Foundation.
# See the file LICENSE.txt at the root directory of this source
# distribution for additional information about the GNU GPL.
#
# For using ustk with software that can not be combined with the GNU
# GPL, please contact Inria about acquiring a ViSP Professional
# Edition License.
#
# This software was developed at:
# Inria Rennes - Bretagne Atlantique
# Campus Universitaire de Beaulieu
# 35042 Rennes Cedex
# France
#
# If you have questions regarding the use of this file, please contact
# Inria at ustk@inria.fr
#
# This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
# WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# Authors:
# Fabien Spindler
#
#############################################################################


    find_package(Qt4 REQUIRED QtGui QtCore)
    # Use what VTK built with
    set(QT_QMAKE_EXECUTABLE ${VTK_QT_QMAKE_EXECUTABLE} CACHE FILEPATH "")
    set(QT_MOC_EXECUTABLE ${VTK_QT_MOC_EXECUTABLE} CACHE FILEPATH "")
    set(QT_UIC_EXECUTABLE ${VTK_QT_UIC_EXECUTABLE} CACHE FILEPATH "")

    list(APPEND opt_incs "${QT_QTGUI_INCLUDE_DIR}")
    #list(APPEND opt_incs "${QT_INCLUDE_DIR}/QtCore") # seems not useful
    list(APPEND opt_libs ${QT_QTGUI_LIBRARIES})
    list(APPEND opt_libs ${QT_QTCORE_LIBRARIES})
    list(APPEND opt_libs ${QT_QTNETWORK_LIBRARIES})
#if(USE_QT5 OR USE_VTK)
  vp_add_module(ustk_grabber visp_gui visp_ustk_io )
  vp_glob_module_sources()
  vp_module_include_directories(${opt_incs})
  vp_create_module(${opt_libs})

  vp_add_tests()
#endif()
