#include "PusherBoris.h"

#include <iostream>
#include <cmath>
#ifdef _GPU
#include <accelmath.h>
#endif

#include "Species.h"

#include "Particles.h"

using namespace std;

PusherBoris::PusherBoris( Params &params, Species *species )
    : Pusher( params, species )
{
}

PusherBoris::~PusherBoris()
{
}

/***********************************************************************
    Lorentz Force -- leap-frog (Boris) scheme
***********************************************************************/

void PusherBoris::operator()( Particles &particles, SmileiMPI *smpi, int istart, int iend, int ithread, int ipart_buffer_offset )
{
    std::vector<double> *Epart = &( smpi->dynamics_Epart[ithread] );
    std::vector<double> *Bpart = &( smpi->dynamics_Bpart[ithread] );
    double *invgf = &( smpi->dynamics_invgf[ithread][0] );
    
    double charge_over_mass_dts2;
    double inv_det_T, Tx, Ty, Tz;
    double umx, umy, umz, upx, upy, upz;
    double pxsm, pysm, pzsm;
    double local_invgf;
    
    double* position_x = particles.getPtrPosition(0);
    double* position_y = NULL;
    double* position_z = NULL;
    if (nDim_>1) {
        position_y = particles.getPtrPosition(1);
        if (nDim_>2) {
            position_z = particles.getPtrPosition(2);
        }
    }
    double* momentum_x = particles.getPtrMomentum(0);
    double* momentum_y = particles.getPtrMomentum(1);
    double* momentum_z = particles.getPtrMomentum(2);

    short *charge = particles.getPtrCharge();

    int nparts = particles.last_index.back();
    double *Ex = &( ( *Epart )[0*nparts] );
    double *Ey = &( ( *Epart )[1*nparts] );
    double *Ez = &( ( *Epart )[2*nparts] );
    double *Bx = &( ( *Bpart )[0*nparts] );
    double *By = &( ( *Bpart )[1*nparts] );
    double *Bz = &( ( *Bpart )[2*nparts] );
    
    #pragma omp simd
    for( int ipart=istart ; ipart<iend; ipart++ ) {
        
        charge_over_mass_dts2 = ( double )( charge[ipart] - ipart_buffer_offset )*one_over_mass_*dts2;
        
        // init Half-acceleration in the electric field
        pxsm = charge_over_mass_dts2*( *( Ex+ipart-ipart_buffer_offset ) );
        pysm = charge_over_mass_dts2*( *( Ey+ipart-ipart_buffer_offset ) );
        pzsm = charge_over_mass_dts2*( *( Ez+ipart-ipart_buffer_offset ) );
        
        //(*this)(particles, ipart, (*Epart)[ipart], (*Bpart)[ipart] , (*invgf)[ipart]);
        umx = momentum_x[ipart] + pxsm;
        umy = momentum_y[ipart] + pysm;
        umz = momentum_z[ipart] + pzsm;
        
        // Rotation in the magnetic field
        local_invgf = charge_over_mass_dts2 / sqrt( 1.0 + umx*umx + umy*umy + umz*umz );
        Tx    = local_invgf * ( *( Bx+ipart-ipart_buffer_offset ) );
        Ty    = local_invgf * ( *( By+ipart-ipart_buffer_offset ) );
        Tz    = local_invgf * ( *( Bz+ipart-ipart_buffer_offset ) );
        inv_det_T = 1.0/( 1.0+Tx*Tx+Ty*Ty+Tz*Tz );

        
        upx = ( ( 1.0+Tx*Tx-Ty*Ty-Tz*Tz )* umx  +      2.0*( Tx*Ty+Tz )* umy  +      2.0*( Tz*Tx-Ty )* umz )*inv_det_T;
        upy = ( 2.0*( Tx*Ty-Tz )* umx  + ( 1.0-Tx*Tx+Ty*Ty-Tz*Tz )* umy  +      2.0*( Ty*Tz+Tx )* umz )*inv_det_T;
        upz = ( 2.0*( Tz*Tx+Ty )* umx  +      2.0*( Ty*Tz-Tx )* umy  + ( 1.0-Tx*Tx-Ty*Ty+Tz*Tz )* umz )*inv_det_T;
        
        // finalize Half-acceleration in the electric field
        pxsm += upx;
        pysm += upy;
        pzsm += upz;
        invgf[ipart-ipart_buffer_offset] = 1. / sqrt( 1.0 + pxsm*pxsm + pysm*pysm + pzsm*pzsm );
        
        momentum_x[ipart] = pxsm;
        momentum_y[ipart] = pysm;
        momentum_z[ipart] = pzsm;
        
        // Move the particle

        position_x[ipart] += dt*momentum_x[ipart]*invgf[ipart-ipart_buffer_offset];
        if (nDim_>1) {
            position_y[ipart] += dt*momentum_y[ipart]*invgf[ipart-ipart_buffer_offset];
            if (nDim_>2) {
                position_z[ipart] += dt*momentum_z[ipart]*invgf[ipart-ipart_buffer_offset];
            }
        }
    }
}
