/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2008-2011, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// @Authors
//      Nghia Ho, nghiaho12@yahoo.com
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of OpenCV Foundation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the OpenCV Foundation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/
#include "precomp.hpp"

namespace cv
{

int rotatedRectangleIntersection( const RotatedRect& rect1, const RotatedRect& rect2, OutputArray intersectingRegion )
{
    CV_INSTRUMENT_REGION();

    // Shift rectangles closer to origin (0, 0) to improve the calculation of the intesection region
    // To do that, the average center of the rectangles is moved to the origin
    const auto averageCenter = (rect1.center + rect2.center) / 2.0f;
    RotatedRect shiftedRect1(rect1);
    RotatedRect shiftedRect2(rect2);

    // Move rectangles closer to origin
    shiftedRect1.center -= averageCenter;
    shiftedRect2.center -= averageCenter;

    // L2 metric
    const float samePointEps = std::max(1e-16f, 1e-6f * (float)std::max(shiftedRect1.size.area(), shiftedRect2.size.area()));

    if (shiftedRect1.size.empty() || shiftedRect2.size.empty())
    {
        intersectingRegion.release();
        return INTERSECT_NONE;
    }

    Point2f vec1[4], vec2[4];
    Point2f pts1[4], pts2[4];

    std::vector <Point2f> intersection; intersection.reserve(24);

    shiftedRect1.points(pts1);
    shiftedRect2.points(pts2);

    int ret = INTERSECT_FULL;

    // Specical case of shiftedRect1 == shiftedRect2
    {
        bool same = true;

        for( int i = 0; i < 4; i++ )
        {
            if( fabs(pts1[i].x - pts2[i].x) > samePointEps || (fabs(pts1[i].y - pts2[i].y) > samePointEps) )
            {
                same = false;
                break;
            }
        }

        if(same)
        {
            intersection.resize(4);

            for( int i = 0; i < 4; i++ )
            {
                intersection[i] = pts1[i];
            }

            Mat(intersection).copyTo(intersectingRegion);

            return INTERSECT_FULL;
        }
    }

    // Line vector
    // A line from p1 to p2 is: p1 + (p2-p1)*t, t=[0,1]
    for( int i = 0; i < 4; i++ )
    {
        vec1[i].x = pts1[(i+1)%4].x - pts1[i].x;
        vec1[i].y = pts1[(i+1)%4].y - pts1[i].y;

        vec2[i].x = pts2[(i+1)%4].x - pts2[i].x;
        vec2[i].y = pts2[(i+1)%4].y - pts2[i].y;
    }

    // Line test - test all line combos for intersection
    for( int i = 0; i < 4; i++ )
    {
        for( int j = 0; j < 4; j++ )
        {
            // Solve for 2x2 Ax=b
            float x21 = pts2[j].x - pts1[i].x;
            float y21 = pts2[j].y - pts1[i].y;

            float vx1 = vec1[i].x;
            float vy1 = vec1[i].y;

            float vx2 = vec2[j].x;
            float vy2 = vec2[j].y;

            float det = vx2*vy1 - vx1*vy2;

            float t1 = (vx2*y21 - vy2*x21) / det;
            float t2 = (vx1*y21 - vy1*x21) / det;

            // This takes care of parallel lines
            if( cvIsInf(t1) || cvIsInf(t2) || cvIsNaN(t1) || cvIsNaN(t2) )
            {
                continue;
            }

            if( t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f )
            {
                float xi = pts1[i].x + vec1[i].x*t1;
                float yi = pts1[i].y + vec1[i].y*t1;

                intersection.push_back(Point2f(xi,yi));
            }
        }
    }

    if( !intersection.empty() )
    {
        ret = INTERSECT_PARTIAL;
    }

    // Check for vertices from shiftedRect1 inside shiftedRect2
    for( int i = 0; i < 4; i++ )
    {
        // We do a sign test to see which side the point lies.
        // If the point all lie on the same sign for all 4 sides of the rect,
        // then there's an intersection
        int posSign = 0;
        int negSign = 0;

        float x = pts1[i].x;
        float y = pts1[i].y;

        for( int j = 0; j < 4; j++ )
        {
            // line equation: Ax + By + C = 0
            // see which side of the line this point is at
            float A = -vec2[j].y;
            float B = vec2[j].x;
            float C = -(A*pts2[j].x + B*pts2[j].y);

            float s = A*x+ B*y+ C;

            if( s >= 0 )
            {
                posSign++;
            }
            else
            {
                negSign++;
            }
        }

        if( posSign == 4 || negSign == 4 )
        {
            intersection.push_back(pts1[i]);
        }
    }

    // Reverse the check - check for vertices from shiftedRect2 inside shiftedRect1
    for( int i = 0; i < 4; i++ )
    {
        // We do a sign test to see which side the point lies.
        // If the point all lie on the same sign for all 4 sides of the rect,
        // then there's an intersection
        int posSign = 0;
        int negSign = 0;

        float x = pts2[i].x;
        float y = pts2[i].y;

        for( int j = 0; j < 4; j++ )
        {
            // line equation: Ax + By + C = 0
            // see which side of the line this point is at
            float A = -vec1[j].y;
            float B = vec1[j].x;
            float C = -(A*pts1[j].x + B*pts1[j].y);

            float s = A*x + B*y + C;

            if( s >= 0 )
            {
                posSign++;
            }
            else
            {
                negSign++;
            }
        }

        if( posSign == 4 || negSign == 4 )
        {
            intersection.push_back(pts2[i]);
        }
    }

    int N = (int)intersection.size();
    if (N == 0)
    {
        return INTERSECT_NONE;
    }

    // Get rid of duplicated points
    int Nstride = N;
    cv::AutoBuffer<float, 100> distPt(N * N);
    cv::AutoBuffer<int> ptDistRemap(N);
    for (int i = 0; i < N; ++i)
    {
        const Point2f pt0 = intersection[i];
        ptDistRemap[i] = i;
        for (int j = i + 1; j < N; )
        {
            const Point2f pt1 = intersection[j];
            float d2 = normL2Sqr<float>(pt1 - pt0);
            if(d2 <= samePointEps)
            {
                if (j < N - 1)
                    intersection[j] =  intersection[N - 1];
                N--;
                continue;
            }
            distPt[i*Nstride + j] = d2;
            ++j;
        }
    }
    while (N > 8) // we still have duplicate points after samePointEps threshold (eliminate closest points)
    {
        int minI = 0;
        int minJ = 1;
        float minD = distPt[1];
        for (int i = 0; i < N - 1; ++i)
        {
            float* pDist = distPt.data() + Nstride * ptDistRemap[i];
            for (int j = i + 1; j < N; ++j)
            {
                float d = pDist[ptDistRemap[j]];
                if (d < minD)
                {
                    minD = d;
                    minI = i;
                    minJ = j;
                }
            }
        }
        CV_Assert(fabs(normL2Sqr<float>(intersection[minI] - intersection[minJ]) - minD) < 1e-6);  // ptDistRemap is not corrupted
        // drop minJ point
        if (minJ < N - 1)
        {
            intersection[minJ] =  intersection[N - 1];
            ptDistRemap[minJ] = ptDistRemap[N - 1];
        }
        N--;
    }

    // order points
    for (int i = 0; i < N - 1; ++i)
    {
        Point2f diffI = intersection[i + 1] - intersection[i];
        for (int j = i + 2; j < N; ++j)
        {
            Point2f diffJ = intersection[j] - intersection[i];
            if (diffI.cross(diffJ) < 0)
            {
                std::swap(intersection[i + 1], intersection[j]);
                diffI = diffJ;
            }
        }
    }

    intersection.resize(N);

    // Move intersection to the original position of rect1 and rect2
    for (auto &point : intersection)
    {
        point.x += averageCenter.x;
        point.y += averageCenter.y;
    }

    Mat(intersection).copyTo(intersectingRegion);

    return ret;
}

} // end namespace
