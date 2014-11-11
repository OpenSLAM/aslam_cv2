#ifndef ASLAM_CAMERAS_PINHOLE_CAMERA_H_
#define ASLAM_CAMERAS_PINHOLE_CAMERA_H_

#include <aslam/cameras/camera.h>
#include <aslam/common/crtp-clone.h>
#include <aslam/common/types.h>
#include <aslam/cameras/distortion.h>
#include <aslam/common/macros.h>

namespace aslam {

// Forward declarations.
class MappedUndistorter;

/// \class PinholeCamera
/// \brief An implementation of the pinhole camera model with (optional) distortion.
///
/// The usual model of a pinhole camera follows these steps:
///    - Transformation: Transform the point into a coordinate frame associated with the camera
///    - Normalization:  Project the point onto the normalized image plane: \f$\mathbf y := \left[ x/z,y/z\right] \f$
///    - Distortion:     apply a nonlinear transformation to \f$y\f$ to account for radial and tangential distortion of the lens
///    - Projection:     Project the point into the image using a standard \f$3 \time 3\f$ projection matrix
///
///  Intrinsic parameters ordering: fu, fv, cu, cv
///  Reference: http://en.wikipedia.org/wiki/Pinhole_camera_model
class PinholeCamera : public aslam::Cloneable<Camera, PinholeCamera> {
  enum { kNumOfParams = 4 };
 public:
  ASLAM_POINTER_TYPEDEFS(PinholeCamera);

  enum { CLASS_SERIALIZATION_VERSION = 1 };
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  // TODO(slynen) Enable commented out PropertyTree support
  // PinholeCamera(const sm::PropertyTree& config);

  //////////////////////////////////////////////////////////////
  /// \name Constructors/destructors and operators
  /// @{

 protected:
  /// \brief Empty constructor for serialization interface.
  PinholeCamera();

 public:
  /// Copy constructor for clone operation.
  PinholeCamera(const PinholeCamera& other) = default;
  void operator=(const PinholeCamera&) = delete;

 public:
  /// \brief Construct a PinholeCamera with distortion.
  /// @param[in] intrinsics   Vector containing the intrinsic parameters (fu,fv,cu,cv).
  /// @param[in] image_width  Image width in pixels.
  /// @param[in] image_height Image height in pixels.
  /// @param[in] distortion   Pointer to the distortion model.
  PinholeCamera(const Eigen::VectorXd& intrinsics, uint32_t image_width, uint32_t image_height,
                aslam::Distortion::UniquePtr& distortion);

  /// \brief Construct a PinholeCamera without distortion.
  /// @param[in] intrinsics   Vector containing the intrinsic parameters (fu,fv,cu,cv).
  /// @param[in] image_width  Image width in pixels.
  /// @param[in] image_height Image height in pixels.
  PinholeCamera(const Eigen::VectorXd& intrinsics, uint32_t image_width, uint32_t image_height);

  /// \brief Construct a PinholeCamera with distortion.
  /// @param[in] focallength_cols Focal length in pixels; cols (width-direction).
  /// @param[in] focallength_rows Focal length in pixels; rows (height-direction).
  /// @param[in] imagecenter_cols Image center in pixels; cols (width-direction).
  /// @param[in] imagecenter_rows Image center in pixels; rows (height-direction).
  /// @param[in] image_width      Image width in pixels.
  /// @param[in] image_height     Image height in pixels.
  /// @param[in] distortion       Pointer to the distortion model.
  PinholeCamera(double focallength_cols, double focallength_rows,
                double imagecenter_cols, double imagecenter_rows,
                uint32_t image_width, uint32_t image_height,
                aslam::Distortion::UniquePtr& distortion);

  /// \brief Construct a PinholeCamera without distortion.
  /// @param[in] focallength_cols Focal length in pixels; cols (width-direction).
  /// @param[in] focallength_rows Focal length in pixels; rows (height-direction).
  /// @param[in] imagecenter_cols Image center in pixels; cols (width-direction).
  /// @param[in] imagecenter_rows Image center in pixels; rows (height-direction).
  /// @param[in] image_width      Image width in pixels.
  /// @param[in] image_height     Image height in pixels.
  PinholeCamera(double focallength_cols, double focallength_rows,
                double imagecenter_cols, double imagecenter_rows,
                uint32_t image_width, uint32_t image_height);

  virtual ~PinholeCamera() {};

  /// \brief Compare this camera to another camera object.
  virtual bool operator==(const Camera& other) const;

  /// \brief Convenience function to print the state using streams.
  std::ostream& operator<<(std::ostream& out) {
    this->printParameters(out, std::string(""));
    return out;
  };

  /// @}

  //////////////////////////////////////////////////////////////
  /// \name Methods to project and back-project euclidean points
  /// @{

  /// \brief Compute the 3d bearing vector in euclidean coordinates given a keypoint in
  ///        image coordinates. Uses the projection (& distortion) models.
  ///        The result might be in normalized image plane for some camera implementations but not
  ///        for the general case.
  /// @param[in]  keypoint     Keypoint in image coordinates.
  /// @param[out] out_point_3d Bearing vector in euclidean coordinates
  virtual bool backProject3(const Eigen::Vector2d& keypoint,
                            Eigen::Vector3d* out_point_3d) const;

  /// \brief Checks the success of a projection operation and returns the result in a
  ///        ProjectionResult object.
  /// @param[in] keypoint Keypoint in image coordinates.
  /// @param[in] point_3d Projected point in euclidean.
  /// @return The ProjectionResult object contains details about the success of the projection.
  template <typename DerivedKeyPoint, typename DerivedPoint3d>
  inline const ProjectionResult evaluateProjectionResult(
      const Eigen::MatrixBase<DerivedKeyPoint>& keypoint,
      const Eigen::MatrixBase<DerivedPoint3d>& point_3d) const;

  /// @}

  //////////////////////////////////////////////////////////////
  /// \name Functional methods to project and back-project points
  /// @{

  // Get the overloaded non-virtual project3Functional(..) from base into scope.
  using Camera::project3Functional;

  /// \brief Template version of project3Functional.
  template <typename ScalarType, typename DistortionType,
            typename MIntrinsics, typename MDistortion>
  const ProjectionResult project3Functional(
      const Eigen::Matrix<ScalarType, 3, 1>& point_3d,
      const Eigen::MatrixBase<MIntrinsics>& intrinsics_external,
      const Eigen::MatrixBase<MDistortion>& distortion_coefficients_external,
      Eigen::Matrix<ScalarType, 2, 1>* out_keypoint) const;

  /// \brief This function projects a point into the image using the intrinsic parameters
  ///        that are passed in as arguments. If any of the Jacobians are nonnull, they
  ///        should be filled in with the Jacobian with respect to small changes in the argument.
  /// @param[in]  point_3d                The point in euclidean coordinates.
  /// @param[in]  intrinsics_external     External intrinsic parameter vector.
  ///                                     NOTE: If nullptr, use internal distortion parameters.
  /// @param[in]  distortion_coefficients_external External distortion parameter vector.
  ///                                     Parameter is ignored is no distortion is active.
  ///                                     NOTE: If nullptr, use internal distortion parameters.
  /// @param[out] out_keypoint            The keypoint in image coordinates.
  /// @param[out] out_jacobian_point      The Jacobian wrt. to changes in the euclidean point.
  ///                                       nullptr: calculation is skipped.
  /// @param[out] out_jacobian_intrinsics The Jacobian wrt. to changes in the intrinsics.
  ///                                       nullptr: calculation is skipped.
  /// @param[out] out_jacobian_distortion The Jacobian wrt. to changes in the distortion parameters.
  ///                                       nullptr: calculation is skipped.
  /// @return Contains information about the success of the projection. Check
  ///         \ref ProjectionResult for more information.
  virtual const ProjectionResult project3Functional(
      const Eigen::Vector3d& point_3d,
      const Eigen::VectorXd* intrinsics_external,
      const Eigen::VectorXd* distortion_coefficients_external,
      Eigen::Vector2d* out_keypoint,
      Eigen::Matrix<double, 2, 3>* out_jacobian_point,
      Eigen::Matrix<double, 2, Eigen::Dynamic>* out_jacobian_intrinsics,
      Eigen::Matrix<double, 2, Eigen::Dynamic>* out_jacobian_distortion) const;

  /// @}

  //////////////////////////////////////////////////////////////
  /// \name Methods to support unit testing.
  /// @{

  /// \brief Creates a random valid keypoint..
  virtual Eigen::Vector2d createRandomKeypoint() const;

  /// \brief Creates a random visible point. Negative depth means random between
  ///        0 and 100 meters.
  virtual Eigen::Vector3d createRandomVisiblePoint(double depth) const;

  /// \brief Get a set of border rays
  void getBorderRays(Eigen::MatrixXd& rays) const;

  /// \brief Create a test camera object for unit testing.
  template<typename DistortionType>
  static PinholeCamera::Ptr createTestCamera() {
    aslam::Distortion::UniquePtr distortion = DistortionType::createTestDistortion();
    aslam::PinholeCamera::Ptr camera(new PinholeCamera(400, 400, 320, 240, 640, 480, distortion));
    aslam::CameraId id;
    id.randomize();
    camera->setId(id);
    return camera;
  }

  /// \brief Create a test camera object for unit testing. (without distortion)
  static PinholeCamera::Ptr createTestCamera() {
    aslam::PinholeCamera::Ptr camera(new PinholeCamera(400, 400, 320, 240, 640, 480));
    aslam::CameraId id;
    id.randomize();
    camera->setId(id);
    return camera;
  }

  /// @}

  //////////////////////////////////////////////////////////////
  /// \name Methods to create an undistorter for this camera.
  /// @{

 public:
  /// \brief Factory method to create a mapped undistorter for this camera geometry.
  ///        NOTE: The undistorter stores a copy of this camera and changes to this geometry
  ///              are not connected with the undistorter!
  /// @param[in] alpha Free scaling parameter between 0 (when all the pixels in the undistorted image
  ///                  will be valid) and 1 (when all the source image pixels will be retained in the
  ///                  undistorted image)
  /// @param[in] scale Output image size scaling parameter wrt. to input image size.
  /// @param[in] interpolation_type Check \ref MappedUndistorter to see the available types.
  /// @return Pointer to the created mapped undistorter.
  virtual std::unique_ptr<MappedUndistorter> createMappedUndistorter(float alpha, float scale,
      aslam::InterpolationMethod interpolation_type) const;

  /// @}

  //////////////////////////////////////////////////////////////
  /// \name Methods to access intrinsics.
  /// @{

  /// \brief Returns the camera matrix for the pinhole projection.
  Eigen::Matrix3d getCameraMatrix() const {
    Eigen::Matrix3d K;
    K << fu(), 0.0,  cu(),
         0.0,  fv(), cv(),
         0.0,  0.0,  1.0;
    return K;
  }

  /// \brief The horizontal focal length in pixels.
  double fu() const { return intrinsics_[0]; };
  /// \brief The vertical focal length in pixels.
  double fv() const { return intrinsics_[1]; };
  /// \brief The horizontal image center in pixels.
  double cu() const { return intrinsics_[2]; };
  /// \brief The vertical image center in pixels.
  double cv() const { return intrinsics_[3]; };

  /// \brief Returns the number of intrinsic parameters used in this camera model.
  inline static constexpr int parameterCount() {
      return kNumOfParams;
  }

  /// \brief Returns the number of intrinsic parameters used in this camera model.
  inline virtual int getParameterSize() const {
      return kNumOfParams;
  }

  /// Function to check wheter the given intrinic parameters are valid for this model.
  virtual bool intrinsicsValid(const Eigen::VectorXd& intrinsics);

  /// \brief Print the internal parameters of the camera in a human-readable form
  /// Print to the ostream that is passed in. The text is extra
  /// text used by the calling function to distinguish cameras
  virtual void printParameters(std::ostream& out, const std::string& text) const;

  /// @}

 private:
  /// \brief Minimal depth for a valid projection.
  static const double kMinimumDepth;
};

}  // namespace aslam

#include "aslam/cameras/camera-pinhole-inl.h"

#endif  // ASLAM_CAMERAS_PINHOLE_CAMERA_H_
