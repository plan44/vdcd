//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include <math.h>

#include "colorutils.hpp"

using namespace p44;

// sRGB with D65 reference white calibration matrix
// [[Xr,Xg,Xb],[Yr,Yg,Yb],[Zr,Zg,Zb]]
const Matrix3x3 p44::sRGB_d65_calibration = {
  { 0.4123955889674142161, 0.3575834307637148171, 0.1804926473817015735 },
  { 0.2125862307855955516, 0.7151703037034108499, 0.0722004986433362269 },
  { 0.0192972154917469448, 0.1191838645808485318, 0.9504971251315797660 }
};


static void swapRows(int r1, int r2, Matrix3x3 matrix)
{
  double temp;
  for (int c=0; c<3; c++) {
    temp = matrix[r1][0];
    matrix[r1][0] = matrix[r2][0];
    matrix[r2][0] = temp;
  }
}


// Matrix Inverse
// Guass-Jordan Elimination Method
// Reduced Row Eshelon Form (RREF)
bool p44::matrix3x3_inverse(const Matrix3x3 &inmatrix, Matrix3x3 &em)
{
  Matrix3x3 matrix;
  for (int r=0; r<3; r++) { for (int c=0; c<3; c++) { matrix[r][c] = inmatrix[r][c]; }};
  // init result with unity matrix
  em[0][0] = 1; em[0][1] = 0; em[0][2] = 0;
  em[1][0] = 0; em[1][1] = 1; em[1][2] = 0;
  em[2][0] = 0; em[2][1] = 0; em[2][2] = 1;
  // calc
  int lead = 0;
  int rowCount = 3;
  int columnCount = 3;
  for (int r = 0; r < rowCount; r++) {
    if (lead >= columnCount)
      break;
    int i = r;
    while (matrix[i][lead] == 0) {
      i++;
      if (i==rowCount) {
        i = r;
        lead++;
        if (lead==columnCount) {
          return false; // error
        }
      }
    }
    // swap rows i and r in input matrix
    swapRows(i,r,matrix);
    // swap rows i and r in unity matrix
    swapRows(i,r,em);

    double lv = matrix[r][lead];
    for (int j = 0; j < columnCount; j++) {
      matrix[r][j] = matrix[r][j] / lv;
      em[r][j] = em[r][j] / lv;
    }
    for (i = 0; i<rowCount; i++) {
      if (i!=r) {
        lv = matrix[i][lead];
        for (int j = 0; j<columnCount; j++) {
          matrix[i][j] -= lv * matrix[r][j];
          em[i][j] -= lv * em[r][j];
        }
      }
    }
    lead++;
  }
  // success, em contains result
  return true;
}


bool p44::XYZtoRGB(const Matrix3x3 &calib, const Row3 &XYZ, Row3 &RGB)
{
  Matrix3x3 m_inv;
  if (!matrix3x3_inverse(calib, m_inv)) return false;
  RGB[0] = m_inv[0][0]*XYZ[0] + m_inv[0][1]*XYZ[1] + m_inv[0][2]*XYZ[2];
  RGB[1] = m_inv[1][0]*XYZ[0] + m_inv[1][1]*XYZ[1] + m_inv[1][2]*XYZ[2];
  RGB[2] = m_inv[2][0]*XYZ[0] + m_inv[2][1]*XYZ[1] + m_inv[2][2]*XYZ[2];
  return true;
}


bool p44::RGBtoXYZ(const Matrix3x3 &calib, const Row3 &RGB, Row3 &XYZ)
{
  XYZ[0] = calib[0][0]*RGB[0] + calib[0][1]*RGB[1] + calib[0][2]*RGB[2];
  XYZ[1] = calib[1][0]*RGB[0] + calib[1][1]*RGB[1] + calib[1][2]*RGB[2];
  XYZ[2] = calib[2][0]*RGB[0] + calib[2][1]*RGB[1] + calib[2][2]*RGB[2];
  return true;
}

bool p44::XYZtoxyV(const Row3 &XYZ, Row3 &xyV)
{
  if ((XYZ[0]+XYZ[1]+XYZ[2]) == 0) {
    xyV[0] = 0;
    xyV[1] = 0;
    xyV[2] = 0;
  } else {
    xyV[0] = XYZ[0]/(XYZ[0]+XYZ[1]+XYZ[2]);
    xyV[1] = XYZ[1]/(XYZ[0]+XYZ[1]+XYZ[2]);
    xyV[2] = XYZ[1];
  }
  return true;
}


bool p44::xyVtoXYZ(const Row3 &xyV, Row3 &XYZ)
{
  double divisor = xyV[1];
  if (divisor < 0.01)
    divisor = 0.01; // do not divide by 0
  XYZ[0] = xyV[0]*(xyV[2]/divisor);
  XYZ[1] = xyV[2];
  XYZ[2] = (1-xyV[0]-divisor)*(xyV[2]/divisor);
  return true;
}


bool p44::RGBtoHSV(const Row3 &RGB, Row3 &HSV)
{
  // calc min/max
  int maxt = 0;
  double max = RGB[0];
  int mint = 0;
  double min = RGB[0];
  for (int i=1; i<3; i++) {
    if (RGB[i] > max) {
      maxt = i;
      max = RGB[i];
    }
    if (RGB[i] < min) {
      mint = i;
      min = RGB[i];
    }
  }
  // Hue
  if (max==min) {
    HSV[0] = 0;
  } else {
    switch (maxt) {
      case 0: // max = R?
        HSV[0] = 60*(0+(RGB[1]-RGB[2])/(max-min)); break;
      case 1: // max = G?
        HSV[0] = 60*(2+(RGB[2]-RGB[0])/(max-min)); break;
      case 2: // max = B?
        HSV[0] = 60*(4+(RGB[0]-RGB[1])/(max-min)); break;
    }
  }
  if (HSV[0] < 0)
    HSV[0] += 360;
  // Saturation
  if (max == 0) {
    HSV[1] = 0;
  } else {
    HSV[1] = (max-min) / max;
  }
  // Value (brightness)
  HSV[2] = max;
  return true;
}


bool p44::HSVtoRGB(const Row3 &HSV, Row3 &RGB)
{
  int hi = floor(HSV[0] / 60);
  double f = (HSV[0] / 60 - hi);
  double p = HSV[2] * (1 - HSV[1]);
  double q = HSV[2] * (1 - (HSV[1]*f));
  double t = HSV[2] * (1 - (HSV[1]*(1-f)));
  switch (hi) {
    case 0:
    case 6:
      RGB[0] = HSV[2];
      RGB[1] = t;
      RGB[2] = p;
      break;
    case 1:
      RGB[0] = q;
      RGB[1] = HSV[2];
      RGB[2] = p;
      break;
    case 2:
      RGB[0] = p;
      RGB[1] = HSV[2];
      RGB[2] = t;
      break;
    case 3:
      RGB[0] = p;
      RGB[1] = q;
      RGB[2] = HSV[2];
      break;
    case 4:
      RGB[0] = t;
      RGB[1] = p;
      RGB[2] = HSV[2];
      break;
    case 5:
      RGB[0] = HSV[2];
      RGB[1] = p;
      RGB[2] = q;
      break;
  }
  return true;
}


bool p44::HSVtoxyV(const Row3 &HSV, Row3 &xyV)
{
  Row3 RGB;
  HSVtoRGB(HSV, RGB);
  Row3 XYZ;
  RGBtoXYZ(sRGB_d65_calibration, RGB, XYZ);
  XYZtoxyV(XYZ, xyV);
  return true;
}


bool p44::xyVtoHSV(const Row3 &xyV, Row3 &HSV)
{
  Row3 XYZ;
  xyVtoXYZ(xyV, XYZ);
  Row3 RGB;
  XYZtoRGB(sRGB_d65_calibration, XYZ, RGB);
  RGBtoHSV(RGB, HSV);
  return true;
}


const int countCts = 37;
const double cts[countCts][2] = {
  { 948,0.33782873820708 },
  { 1019,0.34682388376817 },
  { 1091,0.35545575770743 },
  { 1163,0.36353287224500 },
  { 1237,0.37121206756052 },
  { 1312,0.37832319611070 },
  { 1388,0.38482574553216 },
  { 1466,0.39076326126528 },
  { 1545,0.39602948797950 },
  { 1626,0.40067257983490 },
  { 1708,0.40462758231674 },
  { 1793,0.40798078933257 },
  { 1880,0.41068017199236 },
  { 1969,0.41273637414613 },
  { 2061,0.41418105044123 },
  { 2157,0.41502718841801 },
  { 2256,0.41527448264726 },
  { 2359,0.41494487494675 },
  { 2466,0.41405903487263 },
  { 2579,0.41261744057645 },
  { 2698,0.41063633036979 },
  { 2823,0.40814486823430 },
  { 2957,0.40511150919122 },
  { 3099,0.40159310586449 },
  { 3252,0.39755898609813 },
  { 3417,0.39303263395499 },
  { 3597,0.38799332181520 },
  { 3793,0.38248898245784 },
  { 4010,0.37647311389569 },
  { 4251,0.36997922346483 },
  { 4522,0.36299131572450 },
  { 4831,0.35549007551420 },
  { 5189,0.34745303570846 },
  { 5609,0.33890583227018 },
  { 6113,0.32982098812739 },
  { 6735,0.32016657303155 },
  { 7530,0.30991572591376 }
};


bool p44::CTtoxyV(double mired, Row3 &xyV)
{
  double CT = 1000000/mired;
  if ((CT<cts[0][0]) || (CT>=cts[countCts-1][0])) {
    xyV[0] = 0.33;
    xyV[1] = 0.33; // CT < 948 || CT > 10115
  }
  for (int i=0; i<countCts; i++) {
    if (CT<cts[i][0]) {
      double fac = (CT-cts[i-1][0])/(cts[i][0]-cts[i-1][0]);
      xyV[1] = fac*(cts[i][1]-cts[i-1][1])+cts[i-1][1];
      xyV[0] = 0.68-((i-1)/100)-(fac/100);
      break;
    }
  }
  xyV[2] = 1.0; // mired has no brightness information, assume 100% = 1.0
  return true;
}


bool p44::xyVtoCT(const Row3 &xyV, double &mired)
{
  // very rough approximation:
  // - CIE x 0.28 -> 10000K = 100mired
  // - CIE x 0.65 -> 1000K = 1000mired
  double x = xyV[0] - 0.28;
  if (x<0) x=0;
  mired = (xyV[0]-0.28)/(0.65-0.28)*900 + 100;
  return true;
}

