/**
 * @file SplineInterpolator2D.h
 * @brief Real-time safe 2D grid interpolator (Bilinear & Bicubic Catmull-Rom Spline).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace abdaudiolab::math
{

/**
 * @class SplineInterpolator2D
 * @brief Real-time safe 2D grid interpolator (Bilinear & Bicubic Catmull-Rom Spline).
 * 
 * Expands quantized measurement matrices (e.g. 8x8, 16x16) into full-resolution
 * 128x128 Look-Up Tables or computes on-the-fly continuous evaluation for plugin runtimes.
 */
class SplineInterpolator2D
{
public:
    SplineInterpolator2D() = default;

    /**
     * @brief Configures 2D grid data from parameter axes and 2D value matrix.
     * @param p1Grid Primary parameter axis grid values.
     * @param p2Grid Secondary parameter axis grid values.
     * @param values 2D matrix of measured values indexed as [row][col].
     * @return true if grid dimensions match, false otherwise.
     */
    bool setGridData(const std::vector<float>& p1Grid,
                     const std::vector<float>& p2Grid,
                     const std::vector<std::vector<float>>& values)
    {
        if (p1Grid.empty() || p2Grid.empty() || values.size() != p1Grid.size())
            return false;

        rows = static_cast<int>(p1Grid.size());
        cols = static_cast<int>(p2Grid.size());

        gridValues.clear();
        gridValues.reserve(static_cast<size_t>(rows * cols));

        for (int r = 0; r < rows; ++r)
        {
            if (values[static_cast<size_t>(r)].size() != static_cast<size_t>(cols))
                return false;
            for (int c = 0; c < cols; ++c)
            {
                gridValues.push_back(values[static_cast<size_t>(r)][static_cast<size_t>(c)]);
            }
        }
        ready = true;
        return true;
    }

    /**
     * @brief Returns true if grid data is configured and valid for evaluation.
     */
    [[nodiscard]] bool isReady() const noexcept { return ready; }

    /**
     * @brief Evaluates bicubic interpolation at normalized coordinates (normX, normY) in [0.0, 1.0].
     */
    [[nodiscard]] float evaluate(float normX, float normY) const noexcept
    {
        if (!ready) return 0.0f;
        return interpolateBicubic(gridValues, rows, cols, normX, normY);
    }

    /**
     * @brief Bilinear interpolation over a 2D grid.
     * @param flatGrid 1D flattened array of values indexed as [row * numCols + col].
     * @param numRows Number of rows (dimension 1, e.g. Cutoff).
     * @param numCols Number of cols (dimension 2, e.g. Resonance).
     * @param normX Normalized X in [0.0, 1.0].
     * @param normY Normalized Y in [0.0, 1.0].
     * @return Interpolated float value.
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
     * @brief Expands an N x M matrix to a target 128 x 128 resolution using bilinear interpolation.
     * @param srcGrid Source flattened matrix.
     * @param srcRows Source row count.
     * @param srcCols Source column count.
     * @return std::vector<float> Flattened 128x128 (16,384 elements) interpolated grid.
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

    /**
     * @brief Catmull-Rom cubic basis function kernel.
     * @param t Distance parameter from kernel center.
     * @return Kernel weight.
     */
    static float catmullRomBasis(float t) noexcept
    {
        float absT = std::abs(t);
        if (absT <= 1.0f)
            return 1.5f * absT * absT * absT - 2.5f * absT * absT + 1.0f;
        else if (absT <= 2.0f)
            return -0.5f * absT * absT * absT + 2.5f * absT * absT - 4.0f * absT + 2.0f;
        return 0.0f;
    }

    /**
     * @brief Bicubic (Catmull-Rom) interpolation over a 2D grid using a 4x4 kernel.
     * @param flatGrid Flattened 2D array [row * numCols + col].
     * @param numRows Number of rows.
     * @param numCols Number of cols.
     * @param normX Normalized X in [0.0, 1.0].
     * @param normY Normalized Y in [0.0, 1.0].
     * @return Interpolated float value.
     */
    static float interpolateBicubic(const std::vector<float>& flatGrid, int numRows, int numCols, float normX, float normY) noexcept
    {
        if (flatGrid.empty() || numRows < 2 || numCols < 2)
            return interpolateBilinear(flatGrid, numRows, numCols, normX, normY);

        float clampedX = std::clamp(normX, 0.0f, 1.0f);
        float clampedY = std::clamp(normY, 0.0f, 1.0f);

        float fx = clampedX * static_cast<float>(numCols - 1);
        float fy = clampedY * static_cast<float>(numRows - 1);

        int ix = static_cast<int>(fx);
        int iy = static_cast<int>(fy);
        float tx = fx - static_cast<float>(ix);
        float ty = fy - static_cast<float>(iy);

        float result = 0.0f;

        for (int m = -1; m <= 2; ++m)
        {
            float wy = catmullRomBasis(ty - static_cast<float>(m));
            int row = std::clamp(iy + m, 0, numRows - 1);

            for (int n = -1; n <= 2; ++n)
            {
                float wx = catmullRomBasis(static_cast<float>(n) - tx);
                int col = std::clamp(ix + n, 0, numCols - 1);

                result += flatGrid[static_cast<size_t>(row * numCols + col)] * wx * wy;
            }
        }

        return result;
    }

    /**
     * @brief Expands an N x M matrix to a target 128 x 128 resolution using bicubic Catmull-Rom interpolation.
     * @param srcGrid Source flattened matrix.
     * @param srcRows Source row count.
     * @param srcCols Source column count.
     * @return std::vector<float> Flattened 128x128 interpolated grid.
     */
    static std::vector<float> expandGridTo128x128Bicubic(const std::vector<float>& srcGrid, int srcRows, int srcCols)
    {
        std::vector<float> outGrid(128 * 128, 0.0f);
        for (int r = 0; r < 128; ++r)
        {
            float normY = static_cast<float>(r) / 127.0f;
            for (int c = 0; c < 128; ++c)
            {
                float normX = static_cast<float>(c) / 127.0f;
                outGrid[static_cast<size_t>(r * 128 + c)] = interpolateBicubic(srcGrid, srcRows, srcCols, normX, normY);
            }
        }
        return outGrid;
    }

private:
    std::vector<float> gridValues;
    int rows { 0 };
    int cols { 0 };
    bool ready { false };
};

} // namespace abdaudiolab::math

