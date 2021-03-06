/*
 * TpsdGeomOptimiser.h
 *
 * Two-point Step Size Gradient Methods - Barzilai and Borwein
 * IMA Journal of Numerical Analysis (1988) 8, 141-148
 *
 *
 *  Created on: Aug 17, 2011
 *      Author: Martin Uhrin
 */

#ifndef SPL__POTENTIAL__TPSD_GEOM_OPTIMISER_H_
#define SPL__POTENTIAL__TPSD_GEOM_OPTIMISER_H_

// INCLUDES /////////////////////////////////////////////
#include <limits>

#include <armadillo>

#include "spl/potential/GeomOptimiser.h"
#include "spl/potential/IPotential.h"
#include "spl/potential/IPotentialEvaluator.h"

#include "spl/common/Structure.h"
#include "spl/common/Types.h"

// DEFINES //////////////////////////////////////////////

namespace spl {

// FORWARD DECLARATIONS ////////////////////////////////////
namespace common {
class UnitCell;
}
namespace potential {

class TpsdGeomOptimiser : public GeomOptimiser
{
public:
  typedef UniquePtr< IPotential>::Type PotentialPtr;

  static const unsigned int DEFAULT_MAX_ITERATIONS;
  static const double DEFAULT_ENERGY_TOLERANCE;
  static const double DEFAULT_FORCE_TOLERANCE;
  static const double DEFAULT_STRESS_TOLERANCE;
  static const double DEFAULT_MAX_DELTA_POS_FACTOR;
  static const double DEFAULT_MAX_DELTA_LATTICE_FACTOR;

  TpsdGeomOptimiser(PotentialPtr potential);

  // IGeomOptimiser interface //////////////////////////////
  virtual IPotential *
  getPotential();
  virtual const IPotential *
  getPotential() const;

  virtual OptimisationOutcome
  optimise(common::Structure & structure,
      const OptimisationSettings & options) const;
  virtual OptimisationOutcome
  optimise(common::Structure & structure, OptimisationData & data,
      const OptimisationSettings & options) const;
  // End IGeomOptimiser interface

  OptimisationOutcome
  optimise(common::Structure & structure, OptimisationData & optimistaionData,
      IPotentialEvaluator & evaluator,
      const OptimisationSettings & options) const;

  OptimisationOutcome
  optimise(common::Structure & structure, common::UnitCell & unitCell,
      OptimisationData & optimistaionData, IPotentialEvaluator & evaluator,
      const OptimisationSettings & options) const;

private:
  static const unsigned int CHECK_CELL_EVERY_N_STEPS;
  static const double CELL_MIN_NORM_VOLUME;
  static const double CELL_MAX_ANGLE_SUM;
  static const double MAX_STEPSIZE;
  static const int MAX_FAILED_POTENTIAL_EVALUATIONS;

  bool
  cellReasonable(const common::UnitCell & unitCell) const;
  void
  populateOptimistaionData(OptimisationData & optData,
      const common::Structure & structure, const PotentialData & potData) const;

  bool
  hasConverged(const double deltaEnergyPerIon, const double maxForceSq,
      const double maxStress, const OptimisationSettings & options) const;
  bool
  capStepsize(const common::Structure & structure,
      const arma::mat * const deltaPos, const arma::mat * const deltaLatticeCar,
      const OptimisationSettings & settings, double * const stepsize) const;

  PotentialPtr myPotential;
};

}
}

#endif /* SPL__POTENTIAL__TPSD_GEOM_OPTIMISER_H_ */
