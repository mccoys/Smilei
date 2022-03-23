#ifndef ENVELOPEBC_H
#define ENVELOPEBC_H

#include <vector>

#include "Patch.h"
#include "ElectroMagn.h"

class Params;
class Patch;
class LaserEnvelope;
class Field;
class Solver;

class EnvelopeBC
{
public:
    EnvelopeBC( Params &params, Patch *patch, unsigned int i_boundary );
    virtual ~EnvelopeBC();
    void clean();

    virtual void apply( LaserEnvelope *envelope, ElectroMagn *EMfields, double time_dual, Patch *patch ) = 0;

    virtual cField* getA2Dnp1PML() { ERROR("Not using PML");return NULL;}
    virtual cField* getA2DnPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getA2Dnm1PML() { ERROR("Not using PML");return NULL;}
    virtual Field* getPhi2DPML() { ERROR("Not using PML");return NULL;}

    virtual cField* getu1np1xPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu2np1xPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu3np1xPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu1nm1xPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu2nm1xPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu3nm1xPML() { ERROR("Not using PML");return NULL;}

    virtual cField* getu1np1yPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu2np1yPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu3np1yPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu1nm1yPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu2nm1yPML() { ERROR("Not using PML");return NULL;}
    virtual cField* getu3nm1yPML() { ERROR("Not using PML");return NULL;}

protected:

    //! time-step
    double dt;

    //! Number of nodes on the primal grid in the x-direction
    unsigned int nx_p;

    //! Number of nodes on the primal grid in the y-direction
    unsigned int ny_p;

    //! Spatial step dx for 2D3V cartesian simulations
    double dx;

    //! Spatial step dy for 2D3V cartesian simulations
    double dy;

    //! Ratio of the time-step by the spatial-step dt/dx for 2D3V cartesian simulations
    double dt_ov_dx;

    //! Ratio of the time-step by the spatial-step dt/dy for 2D3V cartesian simulations
    double dt_ov_dy;

    //! Ratio of the spatial-step by the time-step dx/dt for 2D3V cartesian simulations
    double dx_ov_dt;

    //! Ratio of the spatial-step by the time-step dy/dt for 2D3V cartesian simulations
    double dy_ov_dt;

    // side of BC is applied 0:xmin 1:xmax 2:ymin 3:ymax 4:zmin 5:zmax
    unsigned int i_boundary_;

    Solver* pml_solver_envelope_ ;

};

#endif
