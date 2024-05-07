#pragma once 
#include "utils.h"
#include "particle_system.h"
#include "sph_base.h"
#include <Eigen/Dense>

namespace USTC_CG::node_sph_fluid {

using namespace Eigen;

class IISPH : public SPHBase {
   public:
    IISPH() = default;
    IISPH(const MatrixXd& X, const Vector3d& box_min, const Vector3d& box_max);
    ~IISPH() = default;

    void step() override;
    void compute_pressure() override;

    double pressure_solve_iteration();
    void predict_advection();

    void reset() override;

    inline int& max_iter()
    {
        return max_iter_;
    }
    inline double& omega()
    {
        return omega_;
    }

   protected:
    int max_iter_ = 100;
    VectorXd predict_density_;
    VectorXd aii_;
    VectorXd Api_;  // for debug
    double omega_ = 0.3;
    VectorXd last_pressure_;
};
}  // namespace USTC_CG::node_sph_fluid