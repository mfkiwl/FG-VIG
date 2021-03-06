/*
 * testInvDepthFactorVariant4.cpp
 *
 *  Created on: Apr 13, 2012
 *      Author: cbeall3
 */

#include <CppUnitLite/TestHarness.h>
#include <gtsam/geometry/Cal3_S2.h>
#include <gtsam/geometry/PinholeCamera.h>
#include <gtsam/geometry/Point2.h>
#include <gtsam/geometry/Point3.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearEquality.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>

#include "InvDepthFactorVariant4.h"

using namespace std;
using namespace gtsam;

/* ************************************************************************* */
TEST(InvDepthFactorVariant4, optimize) {
  // Create two poses looking in the x-direction
  Pose3 pose1_wc(Rot3::Ypr(-M_PI / 2, 0., -M_PI / 2), gtsam::Point3(0, 0, 1.0));
  Pose3 pose2_wc(Rot3::Ypr(-M_PI / 2, 0., -M_PI / 2), gtsam::Point3(0, 0, 1.5));

  static Pose3 kExtrPose_ic(Rot3::Rodrigues(-0.01, 0.02, 0.025),
                            Point3(0.5, -1.10, 1.20));
  Pose3 pose1_wi = pose1_wc * kExtrPose_ic.inverse();
  Pose3 pose2_wi = pose2_wc * kExtrPose_ic.inverse();

  // Create a landmark 5 meters in front of pose1 (camera center at (0,0,1))
  Point3 landmark(5, 4, 9);

  // Create image observations
  // Cal3_S2::shared_ptr K(new Cal3_S2(1500, 1200, 0, 640, 480));
  Cal3_S2::shared_ptr K(new Cal3_S2(1, 1, 0, 0, 0));
  PinholeCamera<Cal3_S2> camera1(pose1_wi.compose(kExtrPose_ic), *K);
  PinholeCamera<Cal3_S2> camera2(pose2_wi.compose(kExtrPose_ic), *K);
  Point2 pixel1 = camera1.project(landmark);
  Point2 pixel2 = camera2.project(landmark);

  // Create expected landmark
  Point3 landmark_p1 = camera1.pose().transformTo(landmark);
  //   // landmark_p1.print("Landmark in Pose1 Frame:\n");
  //   double theta = atan2(landmark_p1.x(), landmark_p1.z());
  //   double phi = atan2(landmark_p1.y(), sqrt(landmark_p1.x() *
  //   landmark_p1.x() +
  //                                            landmark_p1.z() *
  //                                            landmark_p1.z()));
  //   double rho = 1. / landmark_p1.norm();
  //   Vector3 expected((Vector(3) << theta, phi, rho).finished());
  double rho = 1. / landmark_p1.z();
  assert(rho > 0);

  // Create a factor graph with two inverse depth factors and two pose priors
  Key poseKey1(1);
  Key poseKey2(2);
  Key landmarkKey(100);
  SharedNoiseModel sigma(noiseModel::Unit::Create(2));
  NonlinearFactor::shared_ptr factor1(
      new NonlinearEquality<Pose3>(poseKey1, pose1_wi, 100000));
  NonlinearFactor::shared_ptr factor2(
      new NonlinearEquality<Pose3>(poseKey2, pose2_wi, 100000));
  NonlinearFactor::shared_ptr factor4(new InvDepthFactorVariant4c(
      poseKey1, poseKey2, landmarkKey, pixel1, pixel2, sigma, kExtrPose_ic));
  NonlinearFactorGraph graph;
  graph.push_back(factor1);
  graph.push_back(factor2);
  graph.push_back(factor4);

  // Create a values with slightly incorrect initial conditions
  Values values;
  Pose3 pose1_wi_InitValue =
      pose1_wc.retract(
          (Vector(6) << +0.01, -0.02, +0.03, -0.10, +0.20, -0.30).finished()) *
      kExtrPose_ic.inverse();
  Pose3 pose2_wi_InitValue =
      pose2_wc.retract(
          (Vector(6) << +0.01, +0.02, -0.03, -0.10, +0.20, +0.30).finished()) *
      kExtrPose_ic.inverse();
  values.insert(poseKey1, pose1_wi_InitValue);
  values.insert(poseKey2, pose2_wi_InitValue);
  values.insert(landmarkKey, double(rho + 0.00));
  values.print("values:");

  // Optimize the graph to recover the actual landmark position
  LevenbergMarquardtParams params;
  Values result = LevenbergMarquardtOptimizer(graph, values, params).optimize();
  double actual = result.at<double>(landmarkKey);

  // Test that the correct landmark parameters have been recovered
  EXPECT(assert_equal(rho, actual, 1e-9));
}

/* ************************************************************************* */
int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */
