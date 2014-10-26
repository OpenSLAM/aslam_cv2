#ifndef TRIANGULATION_TRIANGULATION_TOOLBOX_H_
#define TRIANGULATION_TRIANGULATION_TOOLBOX_H_

#include <vector>

#include <aslam/common/memory.h>
#include <aslam/common/pose-types.h>
#include <Eigen/Dense>
#include <Eigen/QR>
#include <glog/logging.h>

namespace aslam {

/// brief Triangulate a 3d point from a set of n keypoint measurements on the normalized camera
///       plane.
/// @param measurements Keypoint measurements on normalized camera plane.
/// @param T_W_B Pose of the body frame of reference w.r.t. the global frame, expressed
///              in the global frame.
/// @param T_B_C Pose of the camera w.r.t. the body frame expressed in the body frame of reference.
/// @param G_point Triangulated point in global frame.
/// @return Was the triangulation successful?
inline bool linearTriangulateFromNViews(
    const Aligned<std::vector, Eigen::Vector2d>::type& measurements,
    const Aligned<std::vector, aslam::Transformation>::type& T_G_B,
    const aslam::Transformation& T_B_C, Eigen::Vector3d* G_point) {
  CHECK_NOTNULL(G_point);
  CHECK_EQ(measurements.size(), T_G_B.size());
  if (measurements.size() < 2u) {
    return false;
  }

  const size_t rows = 3 * measurements.size();
  const size_t cols = 3 + measurements.size();
  Eigen::MatrixXd A = Eigen::MatrixXd::Zero(rows, cols);
  Eigen::VectorXd b = Eigen::VectorXd::Zero(rows);

  const Eigen::Matrix3d I_R_C = T_B_C.getRotationMatrix();

  // Fill in A and b.
  for (size_t i = 0; i < measurements.size(); ++i) {
    Eigen::Vector3d v(measurements[i](0), measurements[i](1), 1.);
    Eigen::Matrix3d R_G_I = T_G_B[i].getRotationMatrix();
    Eigen::Vector3d p_G_I = T_G_B[i].getPosition();
    A.block<3, 3>(3 * i, 0) = Eigen::Matrix3d::Identity();
    A.block<3, 1>(3 * i, 3 + i) = -R_G_I * I_R_C * v;
    b.segment<3>(3 * i) = p_G_I + R_G_I * T_B_C.getPosition();
  }

  *G_point = A.colPivHouseholderQr().solve(b).head<3>();
  return true;
}
}  // namespace aslam
#endif  // TRIANGULATION_TRIANGULATION_TOOLBOX_H_