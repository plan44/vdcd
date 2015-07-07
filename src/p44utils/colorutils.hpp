//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __p44utils__colorutils__
#define __p44utils__colorutils__

namespace p44 {

  typedef double Row3[3];
  typedef double Matrix3x3[3][3];

  extern void matrix3x3_copy(const Matrix3x3 &aFrom, Matrix3x3 &aTo);
  extern const Matrix3x3 sRGB_d65_calibration;

  bool matrix3x3_inverse(const Matrix3x3 &matrix, Matrix3x3 &em);

  bool XYZtoRGB(const Matrix3x3 &calib, const Row3 &XYZ, Row3 &RGB);
  bool RGBtoXYZ(const Matrix3x3 &calib, const Row3 &RGB, Row3 &XYZ);

  bool XYZtoxyV(const Row3 &XYZ, Row3 &xyV);
  bool xyVtoXYZ(const Row3 &xyV, Row3 &XYZ);

  bool RGBtoHSV(const Row3 &RGB, Row3 &HSV);
  bool HSVtoRGB(const Row3 &HSV, Row3 &RGB);

  bool HSVtoxyV(const Row3 &HSV, Row3 &xyV);
  bool xyVtoHSV(const Row3 &xyV, Row3 &HSV);

  bool CTtoxyV(double mired, Row3 &xyV);
  bool xyVtoCT(const Row3 &xyV, double &mired);

} // namespace p44

#endif /* defined(__p44utils__colorutils__) */