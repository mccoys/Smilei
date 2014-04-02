
#include "Species.h"


#include "PusherFactory.h"
#include "IonizationFactory.h"

#include "PartBoundCond.h"
#include "BoundaryConditionType.h"

#include "ElectroMagn.h"
#include "Interpolator.h"
#include "Projector.h"

#include "SmileiMPI.h"

// #include "Field.h"
#include "Field1D.h"
#include "Field2D.h"
#include "Field3D.h"
#include "Tools.h"

#include <iostream>
#include <cmath>
#include <ctime>
#include <cstdlib>

using namespace std;

// ---------------------------------------------------------------------------------------------------------------------
// Creator for Species
// input: simulation parameters & Species index
// ---------------------------------------------------------------------------------------------------------------------
Species::Species(PicParams* params, int ispec, SmileiMPI* smpi) {
    
    // -------------------
    // Variable definition
    // -------------------
	
    name_str=params->species_param[ispec].species_type;
	
    DEBUG(name_str);
	
    electron_species = NULL;
	
    // species index
    speciesNumber = ispec;
	
    // number of spatial dimensions for the particles
    ndim = params->nDim_particle;
	
    // Local minimum for definition of bin clusters
    min_loc = smpi->getDomainLocalMin(ndim-1);
	
    // time over which particles remain frozen
    time_frozen = params->species_param[ispec].time_frozen;
	
    // atomic number
    atomic_number = params->species_param[ispec].atomic_number;
	
    //particle mass
    part_mass = params->species_param[ispec].mass;
	
    // field containing the density distribution (always 3d)
    Field3D density(params->n_space);
	
    // field containing the temperature distribution along all 3 momentum coordinates (always 3d * 3)
    Field3D temperature[3];
	
    // field containing the temperature distribution along all 3 momentum coordinates (always 3d * 3)
    Field3D velocity[3];
	
    for (unsigned int i=0; i<3; i++) {
        velocity[i].allocateDims(params->n_space);
        temperature[i].allocateDims(params->n_space);
    }
	
    oversize.resize(params->nDim_field, 0);
    for (unsigned int i=0 ; i<params->nDim_field ; i++) {
        oversize[i] = params->oversize[i];
    }
    cell_length = params->cell_length;
	
	
    // Arrays of the min and max indices of the particle bins
    bmin.resize(params->n_space[ndim-1]);
    bmax.resize(params->n_space[ndim-1]);
	
    //Size of the buffer on which each bin are projected
    //In 1D the particles of a same bin can be projected on 6 different nodes at the second order (oversize = 2)
    size_proj_buffer = 2 + 2 * oversize[0];
    for (unsigned int i=1; i< ndim; i++) {
        size_proj_buffer *= params->n_space[i-1] + 2 * oversize[i-1];
    }
    cout << "size_proj_buffer = " << size_proj_buffer << endl;
    //Allocate buffer *********************************************
    b_Jx = (double *) calloc(3 * size_proj_buffer, sizeof(double));
    b_Jy = b_Jx + size_proj_buffer ;
    b_Jz = b_Jy + size_proj_buffer ;
    if (ndim > 1) {
        b_dim0 =  2 + 2 * oversize[0];
        if (ndim > 2) {
            b_dim1 = params->n_space[1] + 2 * oversize[1] ;
        }
    }
	
	if (!params->restart) {
		// ---------------------------------------------------------
		// Calculate density and number of particles for the species
		// ---------------------------------------------------------
		
		unsigned int npart_effective=0;
		
		// does a loop over all cells in the simulation
		// considering a 3d volume with size n_space[0]*n_space[1]*n_space[2]
		
		vector<int> cell_index(3,0);
		for (unsigned int i=0 ; i<params->nDim_field ; i++) {
			if (params->cell_length[i]!=0)
				cell_index[i] = round (smpi->getDomainLocalMin(i)/params->cell_length[i]);
		}
		
        npart_effective = 0;
		for (unsigned int k=0; k<params->n_space[2]; k++) {
			for (unsigned int j=0; j<params->n_space[1]; j++) {
				for (unsigned int i=0; i<params->n_space[0]; i++) {
					
					vector<double> x_cell(3,0);
					x_cell[0] = (cell_index[0]+i+0.5)*params->cell_length[0];
					x_cell[1] = (cell_index[1]+j+0.5)*params->cell_length[1];
					x_cell[2] = (cell_index[2]+k+0.5)*params->cell_length[2];
					
					// assign density its correct value in the cell
					density(i,j,k) = params->species_param[ispec].density
					*                density_profile(params, x_cell);
					
					// for non-zero density define temperature & mean-velocity and increment the nb of particles
					if (density(i,j,k)!=0.0) {
						
						// assign the temperature & mean-velocity their correct value in the cell
						for (unsigned int m=0; m<3; m++)	{
							temperature[m](i,j,k) = params->species_param[ispec].temperature[m];
							velocity[m](i,j,k) = params->species_param[ispec].mean_velocity[m];
						}
						
						// increment the effective number of particle by n_part_per_cell
						// for each cell with as non-zero density
						npart_effective += params->species_param[ispec].n_part_per_cell;
						//DEBUG(10,"Specie "<< ispec <<" # part "<<npart_effective<<" "<<i<<" "<<j<<" "<<k);
						
					}//ENDif non-zero density
					
                    //				// ------------------------
                    //				// Constant density profile
                    //				// ------------------------
                    //				if (params->plasma_geometry=="constant") {
                    //
                    //					if (((params->cell_length[0]==0.0) || (
                    //                                                           (cell_index[0]+i+0.5)*params->cell_length[0] > params->vacuum_length[0] &&
                    //                                                           (cell_index[0]+i+0.5)*params->cell_length[0] < params->vacuum_length[0]+params->plasma_length[0]
                    //                                                           )) &&
                    //						((params->cell_length[1]==0.0) || (
                    //                                                           (cell_index[1]+j+0.5)*params->cell_length[1] > params->vacuum_length[1] &&
                    //                                                           (cell_index[1]+j+0.5)*params->cell_length[1] < params->vacuum_length[1]+params->plasma_length[1]
                    //                                                           )) &&
                    //						((params->cell_length[2]==0.0) || (
                    //                                                           (cell_index[2]+k+0.5)*params->cell_length[2] > params->vacuum_length[2] &&
                    //                                                           (cell_index[2]+k+0.5)*params->cell_length[2] < params->vacuum_length[2]+params->plasma_length[2])
                    //                         )) {
                    //
                    //                            // assign density its correct value in the cell
                    //                            density(i,j,k) = params->species_param[ispec].density;
                    //
                    //
                    //                            // assign the temperature & mean-velocity their correct value in the cell
                    //                            for (unsigned int m=0; m<3;m++)	{
                    //                                temperature[m](i,j,k) = params->species_param[ispec].temperature[m];
                    //                                velocity[m](i,j,k) = params->species_param[ispec].mean_velocity[m];
                    //                            }
                    //
                    //                            // increment the effective number of particle by n_part_per_cell
                    //                            // for each cell with as non-zero density
                    //                            npart_effective += params->species_param[ispec].n_part_per_cell;
                    //                            //DEBUG(10,"Specie "<< ispec <<" # part "<<npart_effective<<" "<<i<<" "<<j<<" "<<k);
                    //
                    //                        } else {
                    //                            density(i,j,k)=0.0;
                    //                        }
                    //
                    //				} else { // plasma_geometry
                    //
                    //					// ---------------------------
                    //					// Not defined density profile
                    //					// ---------------------------
                    //					ERROR("geometry not implemented");
                    //
                    //				}//END if plasma_geometry
				}//i
			}//j
		}//k end the loop on all cells
		
		// defines npart_effective for the Species & create the corresponding particles
		// -----------------------------------------------------------------------
		particles.reserve(round( params->species_param[ispec].c_part_max * npart_effective ), ndim);
		particles.initialize(npart_effective, ndim);
		
		// define Maxwell-Juettner related quantities
		// ------------------------------------------
		
		// Maxwell-Juettner cumulative function (array)
		std::vector<double> max_jutt_cumul;
		
		if (params->species_param[ispec].initialization_type=="maxwell-juettner") {
			//! \todo{Pass this parameters in a code constants class (MG)}
			nE     = 20000;
			muEmax = 20.0;
			
			max_jutt_cumul.resize(nE);
			double mu=params->species_param[ispec].mass/params->species_param[ispec].temperature[0];
			double Emax=muEmax/mu;
			dE=Emax/nE;
			
			double fl=0;
			double fr=0;
			max_jutt_cumul[0]=0.0;
			for (unsigned  i=1; i<nE; i++ ) {
				//! \todo{this is just the isotropic case, generalise to non-isotropic (MG)}
				fr=(1+i*dE)*sqrt(pow(1.0+i*dE,2)-1.0) * exp(-mu*i*dE);
				max_jutt_cumul[i]=max_jutt_cumul[i-1] + 0.5*dE*(fr+fl);
				fl=fr;
			}
			for (unsigned int i=0; i<nE; i++) max_jutt_cumul[i]/=max_jutt_cumul[nE-1];
			
		}
		
		// Initialization of the particles properties
		// ------------------------------------------
		unsigned int iPart=0;
		unsigned int *indexes=new unsigned int[ndim];
		double *temp=new double[ndim];
		double *vel=new double[ndim];
		
		// start a loop on all cells
		
		// Rappel :
		// int cell_index[0] = process_coord_x*(params->n_space[0]);
		// int cell_index[1] = process_coord_y*(params->n_space[1]);
		// int cell_index[2] = process_coord_z*(params->n_space[2]);
		//
		//bmin[bin] point to begining of bin (first particle)
		//bmax[bin] point to end of bin (= bmin[bin+1])
		//if bmax = bmin, bin is empty of particle.
		
		for (unsigned int k=0; k<params->n_space[2]; k++) {
			if (ndim == 3) {
				bmin[k] = iPart;
			}
			for (unsigned int j=0; j<params->n_space[1]; j++) {
				if (ndim == 2) {
					bmin[j] = iPart;
				}
				for (unsigned int i=0; i<params->n_space[0]; i++) {
					if (ndim == 1) {
						bmin[i] = iPart;
					}
					// initialize particles in meshes where the density is non-zero
					if (density(i,j,k)>0) {
						//DEBUG(0,i);
						indexes[0]=i+cell_index[0];
						temp[0]=temperature[0](i,j,k);
						vel[0]=velocity[0](i,j,k);
						if (ndim > 1) {
							indexes[1]=j+cell_index[1];
							temp[1]=temperature[1](i,j,k);
							vel[1]=velocity[1](i,j,k);
							if (ndim > 2) {
								indexes[2]=k+cell_index[2];
								temp[2]=temperature[2](i,j,k);
								vel[2]=velocity[2](i,j,k);
							}//ndim > 2
						}//ndim > 1
						
						initPosition(params->species_param[ispec].n_part_per_cell,iPart, indexes, ndim,
									 params->cell_length, params->species_param[ispec].initialization_type);
						initMomentum(params->species_param[ispec].n_part_per_cell,iPart, temp, vel,
									 params->species_param[ispec].initialization_type, max_jutt_cumul);
						initWeight(params, ispec, iPart, density(i,j,k));
						initCharge(params, ispec, iPart, density(i,j,k));
						
						//calculate new iPart (jump to next cell)
						iPart+=params->species_param[ispec].n_part_per_cell;
					}//END if density > 0
					if (ndim == 1) {
						bmax[i] = iPart;
					}
				}//i
				if (ndim == 2) {
					bmax[j] = iPart;
				}
			}//j
			if (ndim == 3) {
				bmax[k] = iPart;
			}
		}//k end the loop on all cells
		
		delete [] indexes;
		delete [] temp;
		delete [] vel;
		
		
		// Recalculate former position using the particle velocity
		// (necessary to calculate currents at time t=0 using the Esirkepov projection scheme)
		for (unsigned int iPart=0; iPart<npart_effective; iPart++) {
			for (unsigned int i=0; i<ndim; i++) {
				particles.position_old(i,iPart) -= particles.momentum(i,iPart)/particles.lor_fac(iPart) * params->timestep;
			}
		}
		PMESSAGE( 1, smpi->getRank(),"Species "<< ispec <<" # part "<< npart_effective );
	}
	//    cout<<"position_old-> " <<particles[0]->position_old(0)/(2*M_PI)<<endl;
	//    cout<<"momentum-> " <<particles[0]->momentum(0)<<endl;
    // assign the correct Pusher to Push
    Push = PusherFactory::create( params, ispec );
	
    // assign the Ionization model (if needed) to Ionize
    Ionize = IonizationFactory::create( params, ispec );
    if (Ionize) DEBUG("----------- IONIZE CREATED ----------- " <<ispec);
	
    // define limits for BC and functions applied and for domain decomposition
    partBoundCond = new PartBoundCond( params, ispec, smpi);
	
	
	
}//END Species creator


// ---------------------------------------------------------------------------------------------------------------------
// Destructor for Species
// ---------------------------------------------------------------------------------------------------------------------
Species::~Species()
{
    delete Push;
    if (Ionize) delete Ionize;
    if (partBoundCond) delete partBoundCond;
    DEBUG(10,"Species deleted ");
}



// ---------------------------------------------------------------------------------------------------------------------
// For all (np) particles in a mesh initialize its numerical weight (equivalent to a number density)
// ---------------------------------------------------------------------------------------------------------------------
void Species::initWeight(PicParams* params, unsigned int ispec, unsigned int iPart, double density)
{
    for (unsigned  p= iPart; p<iPart+params->species_param[ispec].n_part_per_cell; p++) {
        particles.weight(p) = density / params->species_param[ispec].n_part_per_cell;
        /*
         particles.weight(p) = density * params->species_param[ispec].charge
         /                        params->species_param[ispec].n_part_per_cell;
         */
    }
}



// ---------------------------------------------------------------------------------------------------------------------
// For all (np) particles in a mesh initialize its charge state
// ---------------------------------------------------------------------------------------------------------------------
void Species::initCharge(PicParams* params, unsigned int ispec, unsigned int iPart, double density)
{
    for (unsigned  p= iPart; p<iPart+params->species_param[ispec].n_part_per_cell; p++) {
        particles.charge(p) = params->species_param[ispec].charge;
    }
}



// ---------------------------------------------------------------------------------------------------------------------
// For all (np) particles in a mesh initialize their position
//   - either using regular distribution in the mesh (regular)
//   - or using uniform random distribution (for cold and maxwell-juettner distribution)
// ---------------------------------------------------------------------------------------------------------------------
void Species::initPosition(unsigned int np, unsigned int iPart, unsigned int *indexes, unsigned int ndim,
                           std::vector<double> cell_length, string initialization_type)
{
    for (unsigned  p= iPart; p<iPart+np; p++)
    {
        for (unsigned  i=0; i<ndim ; i++)
        {
            if (initialization_type == "regular") {
                particles.position(i,p)=indexes[i]*cell_length[i]+(p-iPart)*cell_length[i]/np;
            } else if (initialization_type == "cold" || initialization_type == "maxwell-juettner") {
                particles.position(i,p)=(indexes[i]+((double)rand() / RAND_MAX))*cell_length[i];
            }
            particles.position_old(i,p) = particles.position(i,p);
			//            cout<<"position new-> "<<particles.position(i,p)/(2*M_PI)<<endl;
        }// i
    }// p
}



// ---------------------------------------------------------------------------------------------------------------------
// For all (np) particles in a mesh initialize their momentum
//   - at zero if regular or cold
//   - using random distribution if maxwell-juettner
// ---------------------------------------------------------------------------------------------------------------------
void Species::initMomentum(unsigned int np, unsigned int iPart, double *temp, double *vel, string initialization_type,
                           vector<double>& max_jutt_cumul)
{
	
    // average mean-momentum (used to center the distribution)
    double pMean[3]= {0.0,0.0,0.0};
	
	
    if (initialization_type == "regular" || initialization_type == "cold")
    {
        // initialize momentum at 0 for regular or cold initialization type
        for (unsigned int p= iPart; p<iPart+np; p++) {
            for (unsigned int i=0; i<3 ; i++) {
                particles.momentum(i,p) = 0.0;
            }
			
			
        }
		
    } else if (initialization_type == "maxwell-juettner")
    {
        // initialize using the Maxwell-Juettner distribution function
        //! \todo{Generalize to non-isotrop temperature (MG)}
        for (unsigned int p= iPart; p<iPart+np; p++)
        {
            double Renergy=(double)rand() / RAND_MAX;
            double phi=acos(1.0-2.0*(double)rand() / RAND_MAX);
            double theta=2.0*M_PI*(double)rand() / RAND_MAX;
			
            int il=0;
            int ir=max_jutt_cumul.size();
            while (ir > il+1)  {
                int im=(il+ir)/2;
                if (Renergy > max_jutt_cumul[im]) {
                    il=im;
                } else {
                    ir=im;
                }
            }
            double right_w=(Renergy-max_jutt_cumul[il])/(max_jutt_cumul[il+1]);
            double left_w=1-right_w;
			
            double Ener=left_w*il*dE +right_w*(il+1)*dE;
            double psm = sqrt(pow(1.0+Ener,2)-1.0);
			
            particles.momentum(0,p) = psm*cos(theta)*sin(phi);
            particles.momentum(1,p) = psm*sin(theta)*sin(phi);
            particles.momentum(2,p) = psm*cos(phi);
            for (unsigned int i=0; i<3 ; i++)
            {
                pMean[i] += particles.momentum(i,p);
            }
        }//p
		
        // center the distribution function around pMean
        // \todo{Allow for non-zero mean-velocity (MG)}
        for (unsigned int p= iPart; p<iPart+np; p++)
        {
            for (unsigned int i=0; i<3 ; i++) {
                particles.momentum(i,p) -= pMean[i]/np;
            }
        }
		
    }//END if initialization_type
	
}//END initMomentum


double Species::density_profile(PicParams* params, vector<double> x_cell) {
	
    // ------------------//
    // 1D density profile
    // ------------------
    if (params->nDim_field==1) {
		
        // Constant density profile
        // ------------------------
        if (params->plasma_geometry=="constant") {
            if (   (x_cell[0]>params->vacuum_length[0])
				&& (x_cell[0]<params->vacuum_length[0]+params->plasma_length[0]) ) {
                return 1.0;
            } else {
                return 0.0;
            }
        }
		
        // Trapezoidal density profile
        // ---------------------------
        else if (params->plasma_geometry=="trap") {
            
            if(params->slope_length.size()!=0){
                
                // vacuum region
                if ( x_cell[0] < params->vacuum_length[0] ) {
                    return 0.0;
                }
                // linearly increasing density
                else if ( x_cell[0] < params->vacuum_length[0]+params->slope_length[0] ) {
                    return (x_cell[0]-params->vacuum_length[0]) / params->slope_length[0];
                }
                // density plateau
                else if ( x_cell[0] < params->vacuum_length[0]+params->plasma_length[0]-params->slope_length[0] ) {
                    return 1.0;
                }
                // linearly decreasing density
                else if ( x_cell[0] < params->vacuum_length[0]+params->plasma_length[0] ) {
                    return 1.0 - ( x_cell[0] - (params->vacuum_length[0]+params->plasma_length[0]-params->slope_length[0]) )
                    /            params->slope_length[0];
                }
                // beyond the plasma
                else {
                    return 0.0;
                }
            }
            else{
                // vacuum region
                if ( x_cell[0] < params->vacuum_length[0] ) {
                    return 0.0;
                }
                // linearly increasing density
                else if ( x_cell[0] < params->vacuum_length[0]+params->left_slope_length[0] ) {
                    return (x_cell[0]-params->vacuum_length[0]) / params->left_slope_length[0];
                }
                // density plateau
                else if ( x_cell[0] < params->vacuum_length[0]+params->plasma_length[0]-params->right_slope_length[0] ) {
                    return 1.0;
                }
                // linearly decreasing density
                else if ( x_cell[0] < params->vacuum_length[0]+params->plasma_length[0] ) {
                    return 1.0 - ( x_cell[0] - (params->vacuum_length[0]+params->plasma_length[0]-params->right_slope_length[0]) )
                    /            params->right_slope_length[0];
                }

                else{
                    return 0.0;
                }
            }
            
            
        }
		
        // Other density profile
        // ---------------------
        else {
            ERROR("Density profile " << params->plasma_geometry << " not yet defined in 1D");
        }
		
    }
    // ------------------
    // 2D density profile
    // ------------------
    else if (params->nDim_field==2) {
		
        double fx, fy;
		
        // Constant density profile
        // ------------------------
        if (params->plasma_geometry=="constant") {
            // x-direction
            if (   (x_cell[0]>params->vacuum_length[0])
				&& (x_cell[0]<params->vacuum_length[0]+params->plasma_length[0]) ) {
                fx = 1.0;
            } else {
                fx = 0.0;
            }
            // y-direction
            if (   (x_cell[1]>params->vacuum_length[1])
				&& (x_cell[1]<params->vacuum_length[1]+params->plasma_length[1]) ) {
                fy = 1.0;
            } else {
                fy = 0.0;
            }
            // x-y direction
            return fx*fy;
        }
		
        // Trapezoidal density profile
        // ---------------------------
        else if (params->plasma_geometry=="trap") {
			
            // x-direction
			
            // vacuum region
            if ( x_cell[0] < params->vacuum_length[0] ) {
                fx = 0.0;
            }
            // linearly increasing density
            else if ( x_cell[0] < params->vacuum_length[0]+params->slope_length[0] ) {
                fx = (x_cell[0]-params->vacuum_length[0]) / params->slope_length[0];
            }
            // density plateau
            else if ( x_cell[0] < params->vacuum_length[0]+params->plasma_length[0]-params->slope_length[0] ) {
                fx = 1.0;
            }
            // linearly decreasing density
            else if ( x_cell[0] < params->vacuum_length[0]+params->plasma_length[0] ) {
                fx = 1.0 - ( x_cell[0] - (params->vacuum_length[0]+params->plasma_length[0]-params->slope_length[0]) )
				/            params->slope_length[0];
            }
            // beyond the plasma
            else {
                fx = 0.0;
            }
			
            // y-direction
			
            // vacuum region
            if ( x_cell[1] < params->vacuum_length[1] ) {
                fy = 0.0;
            }
            // linearly increasing density
            else if ( x_cell[1] < params->vacuum_length[1]+params->slope_length[1] ) {
                fy = (x_cell[1]-params->vacuum_length[1]) / params->slope_length[1];
            }
            // density plateau
            else if ( x_cell[1] < params->vacuum_length[1]+params->plasma_length[1]-params->slope_length[1] ) {
                fy = 1.0;
            }
            // linearly decreasing density
            else if ( x_cell[1] < params->vacuum_length[1]+params->plasma_length[1] ) {
                fy = 1.0 - ( x_cell[1] - (params->vacuum_length[1]+params->plasma_length[1]-params->slope_length[1]) )
				/            params->slope_length[1];
            }
            // beyond the plasma
            else {
                fy = 0.0;
            }
			
            // x-y directions
            return fx*fy;
        }
		
        // Other profiles: not defined
        // ---------------------------
        else {
            ERROR("Density profile " << params->plasma_geometry <<" not yet defined in 2D");
            return 0.0;
        }
		
    }
    // ------------------
    // 3D density profile
    // ------------------
    else {//if (params->nDim_field==3) {
        ERROR("Density profile not yet defined in 3D");
        return 0.0;
    }//ENDif ndim
	
}


// ---------------------------------------------------------------------------------------------------------------------
// For all particles of the species
//   - interpolate the fields at the particle position
//   - calculate the new velocity
//   - calculate the new position
//   - apply the boundary conditions
//   - increment the currents (projection)
// ---------------------------------------------------------------------------------------------------------------------
void Species::dynamics(double time_dual, unsigned int ispec, ElectroMagn* EMfields, Interpolator* Interp,
                       Projector* Proj, SmileiMPI* smpi)
{
    // Electric field at the particle position
    LocalFields Epart;
    // Magnetic field at the particle position
    LocalFields Bpart;
	
    int iloc;
	
    // number of particles for this Species
    int unsigned nParticles = getNbrOfParticles();
    // Reset list of particles to exchange
    smpi->clearExchList();
	
    // -------------------------------
    // calculate the particle dynamics
    // -------------------------------
    if (time_dual>time_frozen) { // moving particle
        double gf = 1.0;
		
        // for all particles of the Species
        //#pragma omp parallel for shared (EMfields)
        for (unsigned int ibin = 0 ; ibin < bmin.size() ; ibin++) {
			
            // reset all current-buffers
            // *3 allows to also reset Jy & Jz which are contiguous in memory
            for (iloc = 0; iloc < 3*size_proj_buffer; iloc++) b_Jx[iloc] = 0.0;
			
            for (unsigned int iPart=bmin[ibin] ; iPart<bmax[ibin]; iPart++ ) {
				
                // Interpolate the fields at the particle position
                (*Interp)(EMfields, particles, iPart, &Epart, &Bpart);
				
                // Do the ionization
                if (Ionize && particles.charge(iPart) < (int) atomic_number) {
                    //!\todo Check if it is necessary to put to 0 or if LocalFields ensures it
					LocalFields Jion;
					Jion.x=0.0;
                    Jion.y=0.0;
                    Jion.z=0.0;
                    (*Ionize)(particles, iPart, Epart, Jion);
                    (*Proj)(EMfields->Jx_, EMfields->Jy_, EMfields->Jz_, particles, iPart, Jion);
                }
				
				
                // Push the particle
                (*Push)(particles, iPart, Epart, Bpart, gf);
				
                // Apply boundary condition on the particles
                // Boundary Condition may be physical or due to domain decomposition
                // apply returns 0 if iPart is no more in the domain local
                //	if omp, create a list per thread
                if ( !partBoundCond->apply( particles, iPart ) ) smpi->addPartInExchList( iPart );
				
                //if (ndim == 1) {
				//    //! \todo Sort projection : to be validaed
                //    (*Proj)(b_Jx, b_Jy, b_Jz, particles, iPart, gf, ibin, b_dim0);
                //} else {
				(*Proj)(EMfields->Jx_s[ispec], EMfields->Jy_s[ispec], EMfields->Jz_s[ispec], EMfields->rho_s[ispec],
						particles, iPart, gf);
				//}
				
            }// iPart
			
            // Copy buffer back to the global array and free buffer****************
            // this part is dimension dependant !! this is for dim = 1
            if (ndim == 1) {
                for (unsigned int i = 0; i < size_proj_buffer ; i++) {
                    iloc = ibin + i ;
                    // adding contribution to the total currents
                    //(*EMfields->Jx_)(iloc) += b_Jx[i];
                    //(*EMfields->Jy_)(iloc) += b_Jy[i];
                    //(*EMfields->Jz_)(iloc) += b_Jz[i];
                    // adding contribution to current species currents and density
                    //! \todo Below, operator(int) is virtual, to change
                    (*EMfields->Jx_s[ispec])(iloc) += b_Jx[i];
                    (*EMfields->Jy_s[ispec])(iloc) += b_Jy[i];
                    (*EMfields->Jz_s[ispec])(iloc) += b_Jz[i];
                }
            }
        }// ibin
		
        if (Ionize && electron_species) {
            for (unsigned int i=0; i < Ionize->new_electrons.size(); i++) {
                // electron_species->particles.push_back(Ionize->new_electrons[i]);
				
                int ibin = (int) ((Ionize->new_electrons).position(0,i) / cell_length[0]) - smpi->getCellStartingGlobalIndex(0) + oversize[0];
                // Copy Ionize->new_electrons(i) in electron_species->particles at position electron_species->bmin[ibin]
                Ionize->new_electrons.cp_particle(i, electron_species->particles, electron_species->bmin[ibin] );
				
                //Update bins status (ugly update, memory is allocated anywhere, OK with vectors per particles parameters)
                electron_species->bmax[ibin]++;
                for (int i=ibin+1; i<bmin.size(); i++) {
                    electron_species->bmin[i]++;
                    electron_species->bmax[i]++;
                }
            }
			
            //if (Ionize->new_electrons.size()) DEBUG("number of electrons " << electron_species->particles.size() << " " << );
            //cerr << "****************** " << speciesNumber << " " << Ionize->new_electrons.size() << " " << electron_species->particles.size() << endl;
            Ionize->new_electrons.clear();
        }
    }
    else { // immobile particle (at the moment only project density)
		
        //#pragma omp parallel for shared (EMfields)
        for (unsigned int iPart=0 ; iPart<nParticles; iPart++ ) {
            (*Proj)(EMfields->rho_s[ispec], particles, iPart);
        }
		
    }//END if time vs. time_frozen
	
	
}//END dynamic



// ---------------------------------------------------------------------------------------------------------------------
// Dump for the Species
//! \todo{Check if one wants to keep it like this (MG)}
// ---------------------------------------------------------------------------------------------------------------------
void Species::dump(std::ofstream& ofile)
{
    for (unsigned int i=0; i<particles.size(); i++ )
    {
        ofile << i ;
        for (unsigned int m=0; m<ndim; m++) ofile << "\t" << particles.position(m,i);
        for (unsigned int m=0; m<3; m++)    ofile << "\t" << particles.momentum(m,i);
        ofile << "\t" << particles.weight(i); //<< "\t" << Push->getMass() << "\t" << Push->getCharge();
        ofile << endl;
    }
    ofile << endl;
}
// It computes the method on the single specie. You can add here your parameter for a new diagnostic.
void Species::computeScalars() {
    double charge_tot=0.0;
    double ener_tot=0.0;
    if (getNbrOfParticles()>0) {
        for (unsigned int iPart=0 ; iPart<getNbrOfParticles(); iPart++ ) {
            charge_tot+=(double)particles.charge(iPart);
            ener_tot+=(particles.lor_fac(iPart)-1.0);
        }
        ener_tot*=part_mass;
    }
    scalars["charge_tot"]=charge_tot;
    scalars["part_number"]=getNbrOfParticles();
    scalars["energy_tot"]=ener_tot;
	
}

// ---------------------------------------------------------------------------------------------------------------------
// Sort particles
// ---------------------------------------------------------------------------------------------------------------------
void Species::sort_part(double dbin)
{
    //dbin is the width of one bin. dbin= dx in 1D, dy in 2D and dz in 3D.
	
    int p1,p2,bmin_init;
    double limit;
	
    //Backward pass
    for (unsigned int bin=0; bin<bmin.size()-1; bin++) { //Loop on the bins. To be parallelized with openMP.
        limit = min_loc + (bin+1)*dbin;
        p1 = bmax[bin]-1;
        //If first particles change bin, they do not need to be swapped.
        while (p1 == bmax[bin]-1 && p1 >= bmin[bin]) {
            if (particles.position(ndim-1,p1) > limit ) {
                bmax[bin]--;
            }
            p1--;
        }
        //         Now particles have to be swapped
        for( p2 = p1 ; p2 >= bmin[bin] ; p2-- ) { //Loop on the bin's particles.
            if (particles.position(ndim-1,p2) > limit ) {
                //This particle goes up one bin.
                particles.swap_part(p2,bmax[bin]-1);
                bmax[bin]--;
            }
        }
    }
    //Forward pass + Rebracketting
    for (unsigned int bin=1; bin<bmin.size(); bin++) { //Loop on the bins. To be parallelized with openMP.
        limit = min_loc + (bin)*dbin;
        bmin_init = bmin[bin];
        p1 = bmin[bin];
        while (p1 == bmin[bin] && p1 < bmax[bin]) {
            if (particles.position(ndim-1,p1) < limit ) {
                bmin[bin]++;
            }
            p1++;
        }
        for( p2 = p1 ; p2 < bmax[bin] ; p2++ ) { //Loop on the bin's particles.
            if (particles.position(ndim-1,p2) < limit ) {
                //This particle goes down one bin.
                particles.swap_part(p2,bmin[bin]);
                bmin[bin]++;
            }
        }
		
        //Rebracketting
        //Number of particles from bin going down is: bmin[bin]-bmin_init.
        //Number of particles from bin-1 going up is: bmin_init-bmax[bin-1].
        //Total number of particles we need to swap is the min of both.
        p2 = min(bmin[bin]-bmin_init,bmin_init-bmax[bin-1]);
        if (p2 >0) particles.swap_part(bmax[bin-1],bmin[bin]-p2,p2);
        bmax[bin-1] += bmin[bin] - bmin_init;
        bmin[bin] = bmax[bin-1];
    }
}

