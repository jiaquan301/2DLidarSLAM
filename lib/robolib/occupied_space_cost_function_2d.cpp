#include <ceres/ceres.h>
#include <ceres/cubic_interpolation.h>
#include <Eigen/Core>

#include "occupied_space_cost_function_2d.h"
#include "grid2d.h"

// Computes a cost for matching the 'point_cloud' to the 'grid' with
// a 'pose'. The cost increases with poorer correspondence of the grid and the
// point observation (e.g. points falling into less occupied space).
class OccupiedSpaceCostFunction2D
{
public:
  OccupiedSpaceCostFunction2D(const double scaling_factor,
                              const Eigen::MatrixXd &point_cloud,
                              const robo::Grid2D &grid)
    : scaling_factor_(scaling_factor), point_cloud_(point_cloud), grid_(grid)
  {
  }

  template <typename T>
  bool operator()(const T *const pose, T *residual) const
  {
    Eigen::Matrix<T, 2, 1> translation(pose[0], pose[1]);
    Eigen::Rotation2D<T> rotation(pose[2]);
    Eigen::Matrix<T, 2, 2> rotation_matrix = rotation.toRotationMatrix();
    //Eigen::Matrix<T, 2, 2> rotation_matrix;
    //rotation_matrix << cos_yaw, -sin_yaw, sin_yaw, cos_yaw;
    Eigen::Matrix<T, 3, 3> transform;
    transform << rotation_matrix, translation, T(0.), T(0.), T(1.);

    const GridArrayAdapter adapter(grid_);
    ceres::BiCubicInterpolator<GridArrayAdapter> interpolator(adapter);
    //const MapLimits &limits = grid_.limits();
    const double grid_max_world_x{ 70. };
    const double grid_max_world_y{ 45. };
    const double grid_resolution{ 1. };

    for (unsigned int i = 0; i < point_cloud_.cols(); ++i)
    {
      // Note that this is a 2D point. The third component is a scaling factor.
      const Eigen::Matrix<T, 3, 1> point((T(point_cloud_(0, i))),
                                         (T(point_cloud_(1, i))),
                                         T(1.));
      const Eigen::Matrix<T, 3, 1> world = transform * point;
      interpolator.Evaluate(
          (grid_max_world_x - world[0]) / grid_resolution - 0.5 +
              static_cast<double>(kPadding),
          (grid_max_world_y - world[1]) / grid_resolution- 0.5 +
              static_cast<double>(kPadding),
          &residual[i]);
      residual[i] = scaling_factor_ * residual[i];
    }
    return true;
  }

private:
  static constexpr int kPadding = INT_MAX / 4;
  class GridArrayAdapter
  {
  public:
    enum
    {
      DATA_DIMENSION = 1
    };

    explicit GridArrayAdapter(const robo::Grid2D &grid) : grid_(grid)
    {
    }

    void GetValue(const int row, const int column, double *const value) const
    {
      if (row < kPadding || column < kPadding || row >= NumRows() - kPadding ||
          column >= NumCols() - kPadding)
      {
        //*value = kMaxCorrespondenceCost;
        *value = grid_.getMaxCorrespondence();
      }
      else
      {
        //*value = static_cast<double>(grid_.GetCorrespondenceCost(
            //Eigen::Array2i(column - kPadding, row - kPadding)));
        *value = static_cast<double>(
            grid_.getCorrespondenceCost(column - kPadding, row - kPadding));
      }
    }

    int NumRows() const
    {
      return grid_.getMapPixelRows() + 2 * kPadding;
    }

    int NumCols() const
    {
      return grid_.getMapPixelCols() + 2 * kPadding;
    }

  private:
    const robo::Grid2D &grid_;
  };

  OccupiedSpaceCostFunction2D(const OccupiedSpaceCostFunction2D &) = delete;
  OccupiedSpaceCostFunction2D &
  operator=(const OccupiedSpaceCostFunction2D &) = delete;

  const double scaling_factor_;
  const Eigen::MatrixXd &point_cloud_;
  const robo::Grid2D &grid_;
};

ceres::CostFunction *CreateOccupiedSpaceCostFunction2D(
    const double scaling_factor, const Eigen::MatrixXd &point_cloud,
    const robo::Grid2D &grid)
{
  return new ceres::AutoDiffCostFunction<OccupiedSpaceCostFunction2D,
                                         ceres::DYNAMIC /* residuals */,
                                         3 /* pose variables */>(
      new OccupiedSpaceCostFunction2D(scaling_factor, point_cloud, grid),
      point_cloud.cols());
}