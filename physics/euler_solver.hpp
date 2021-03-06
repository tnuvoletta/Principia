﻿
#pragma once

#include "geometry/grassmann.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/r3_element.hpp"
#include "geometry/rotation.hpp"
#include "quantities/named_quantities.hpp"
#include "quantities/quantities.hpp"

namespace principia {
namespace physics {
namespace internal_euler_solver {

using geometry::AngularVelocity;
using geometry::Bivector;
using geometry::Instant;
using geometry::R3Element;
using geometry::Rotation;
using quantities::Angle;
using quantities::AngularFrequency;
using quantities::AngularMomentum;
using quantities::MomentOfInertia;
using quantities::NaN;
using quantities::Product;
using quantities::Time;

// A solver for Euler's rotation equations.  It follows Celledoni, Fassò,
// Säfström and Zanna, 2007, The exact computation of the free rigid body motion
// and its use in splitting method [CFSZ07].  See documentation/Celledoni.pdf
// for corrections and adaptations.
template<typename InertialFrame, typename PrincipalAxesFrame>
class EulerSolver {
  static_assert(InertialFrame::is_inertial);

 public:
  using AngularMomentumBivector = Bivector<AngularMomentum, PrincipalAxesFrame>;
  using AttitudeRotation = Rotation<PrincipalAxesFrame, InertialFrame>;

  // Constructs a solver for a body with the given moments_of_inertia in its
  // principal axes frame.  The moments must be in increasing order.  At
  // initial_time the angular momentum is initial_angular_momentum and the
  // attitude initial_attitude.
  EulerSolver(R3Element<MomentOfInertia> const& moments_of_inertia,
              AngularMomentumBivector const& initial_angular_momentum,
              AttitudeRotation const& initial_attitude,
              Instant const& initial_time);

  // Computes the angular momentum at the given time.
  AngularMomentumBivector AngularMomentumAt(Instant const& time) const;

  AngularVelocity<PrincipalAxesFrame> AngularVelocityFor(
      AngularMomentumBivector const& angular_momentum) const;

  // Computes the attitude at the given time, using the angular momentum
  // computed by the previous function.
  AttitudeRotation AttitudeAt(AngularMomentumBivector const& angular_momentum,
                              Instant const& time) const;

 private:
  struct ℬₜ;
  struct ℬʹ;

  // The formula to use, following [CFSZ07], Section 2.2.  They don't have a
  // formula for the spherical case.
  enum class Formula {
    i,
    ii,
    iii,
    Sphere
  };

  Rotation<PrincipalAxesFrame, ℬₜ> Compute𝒫ₜ(
      AngularMomentumBivector const& angular_momentum,
      bool& ṁ_is_zero) const;

  // If m is constant in the principal axes frames, we cannot construct ℬₜ using
  // ṁ as specified after the demonstration of proposition 2.2 in [CFSZ07].
  // Instead, we use a constant v orthogonal to m.  This member is set when
  // initializating ℛ_.
  bool ṁ_is_zero_ = false;

  // Construction parameters.
  R3Element<MomentOfInertia> const moments_of_inertia_;
  AngularMomentumBivector const initial_angular_momentum_;
  Instant const initial_time_;
  Rotation<ℬʹ, InertialFrame> const ℛ_;

  // Amusingly, the formula to use is a constant of motion.
  Formula formula_;

  // Only the parameters needed for the selected formula are non-NaN after
  // construction.

  AngularFrequency λ_ = NaN<AngularFrequency>();

  AngularMomentum G_ = NaN<AngularMomentum>();
  AngularMomentum B₂₃_ = NaN<AngularMomentum>();
  AngularMomentum B₁₃_ = NaN<AngularMomentum>();
  AngularMomentum B₃₁_ = NaN<AngularMomentum>();
  AngularMomentum B₂₁_ = NaN<AngularMomentum>();

  AngularMomentum σB₁₃_ = NaN<AngularMomentum>();
  AngularMomentum σB₃₁_ = NaN<AngularMomentum>();
  AngularMomentum σʹB₁₃_ = NaN<AngularMomentum>();
  AngularMomentum σʺB₃₁_ = NaN<AngularMomentum>();

  double n_ = NaN<double>();
  double mc_ = NaN<double>();
  Angle ν_ = NaN<Angle>();
  Angle ψ_Π_offset_ = NaN<Angle>();
  double ψ_Π_multiplier_ = NaN<double>();
  AngularFrequency ψ_t_multiplier_ = NaN<AngularFrequency>();
};

}  // namespace internal_euler_solver

using internal_euler_solver::EulerSolver;

}  // namespace physics
}  // namespace principia

#include "physics/euler_solver_body.hpp"
