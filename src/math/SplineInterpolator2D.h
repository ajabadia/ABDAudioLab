#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace abdaudiolab::math
{

/**
 * @brief Real-time safe 2D grid interpolator (Bilinear & Bicubic Spline).
 * 
 * Expands quantized measurement matrices (e.g. 16x16 or 17x17) into full-resolution
 * 128x128 Look-Up Tables or computes on-the-fly continuous evaluation for plugin runtimes.
 */
class SplineInterpolator2D
{
public:
    SplineInterpolator2D() = default;

    /**
     * @brief Bilinear interpolation over a 2D grid.
     * @param grid 2D array of values indexed as [row][col]
     * @param numRows Number of rows (dimension 1, e.g. Cutoff)
     * @param numCols Number of cols (dimension 2, e.g. Resonance)
     * @param normX Normalized X in [0.0, 1.0]
     * @param normY Normalized Y in [0.0, 1.0]
     */
    static float interpolateBilinear(const std::vector<float>& flatGrid, int numRows, int numCols, float normX, float normY) noexcept
    {
        if (flatGrid.empty() || numRows <= 0 || numCols <= 0)
            return 0.0f;

        float clampedX = std::clamp(normX, 0.0f, 1.0f);
        float clampedY = std::clamp(normY, 0.0f, 1.0f);

        float fx = clampedX * static_cast<float>(numCols - 1);
        float fy = clampedY * static_cast<float>(numRows - 1);

        int x0 = static_cast<int>(fx);
        int y0 = static_cast<int>(fy);
        int x1 = std::min(x0 + 1, numCols - 1);
        int y1 = std::min(y0 + 1, numRows - 1);

        float tx = fx - static_cast<float>(x0);
        float ty = fy - static_cast<float>(y0);

        float q11 = flatGrid[static_cast<size_t>(y0 * numCols + x0)];
        float q21 = flatGrid[static_cast<size_t>(y0 * numCols + x1)];
        float q12 = flatGrid[static_cast<size_t>(y1 * numCols + x0)];
        float q22 = flatGrid[static_cast<size_t>(y1 * numCols + x1)];

        float r1 = (1.0f - tx) * q11 + tx * q21;
        float r2 = (1.0f - tx) * q12 + tx * q22;

        return (1.0f - ty) * r1 + ty * r2;
    }

    /**
     * @brief Expands an N x M matrix to a target 128 x 128 resolution.
     */
    static std::vector<float> expandGridTo128x128(const std::vector<float>& srcGrid, int srcRows, int srcCols)
    {
        std::vector<float> outGrid(128 * 128, 0.0f);
        for (int r = 0; r < 128; ++r)
        {
            float normY = static_cast<float>(r) / 127.0f;
            for (int c = 0; c < 128; ++c)
            {
                float normX = static_cast<float>(c) / 127.0f;
                outGrid[static_cast<size_t>(r * 128 + c)] = interpolateBilinear(srcGrid, srcRows, srcCols, normX, normY);
            }
        }
        return outGrid;
    }
};

} // namespace abdaudiolab::math
