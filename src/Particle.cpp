#include "Particle.h"
#include "PicParams.h"

#include <iostream>

using namespace std;



// ---------------------------------------------------------------------------------------------------------------------
// Constructor for Particle (takes the dimension of the particle as input parameter)
// ---------------------------------------------------------------------------------------------------------------------
Particle::Particle(int nDim)
{
	int particle_memory_size=(2*nDim+3+1)*sizeof(double)+sizeof(short);
	
	buf  = new char[particle_memory_size];
	
	Position = new (buf)								double[nDim];
	Momentum = new (buf+nDim*sizeof(double))			double[3];
	Weight	 = new (buf+(nDim+3)*sizeof(double))		double[1];

	Position_old = new (buf+(nDim+3+1)*sizeof(double))	double[nDim];

	Charge = new (buf+(2*nDim+3+1)*sizeof(double))		short[1];

	Position[0]     = 0.;
	Position_old[0] = 0.;
	for (unsigned int i=0 ; i<3 ; i++ ) Momentum[i] = 0.;
	Weight[0] = 0.;
	Charge[0] = 0;

}


// ---------------------------------------------------------------------------------------------------------------------
// Destructor for Particle
// ---------------------------------------------------------------------------------------------------------------------
Particle::~Particle()
{
	delete [] buf;
}


// ---------------------------------------------------------------------------------------------------------------------
// Method used to print the Particle properties
// ---------------------------------------------------------------------------------------------------------------------
void Particle::Print(PicParams* params)
{
	for (unsigned int i=0 ; i<params->nDim_field ; i++ ) cout << position(i) << " ";
	for (unsigned int i=0 ; i<3 ; i++ )                  cout << momentum(i) << " ";
	cout <<  weight() << " " << charge() << endl;
}
