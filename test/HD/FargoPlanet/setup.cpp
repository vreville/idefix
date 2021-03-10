#include <algorithm>
#include "idefix.hpp"
#include "setup.hpp"

real sigma0Glob;
real sigmaSlopeGlob;
real h0Glob;
real betaGlob;
real HidealGlob;
real AmMidGlob;
real gammaGlob;
real densityFloorGlob;
real alphaGlob;
real qpGlob;
real RpGlob;
real thicknessSmoothingGlob;

/*********************************************/
/**
Customized random number generator
Allow one to have consistant random numbers
generators on different architectures.
**/
/*********************************************/
real randm(void) {
    const int a    =    16807;
    const int m =    2147483647;
    static int in0 = 13763 + 2417*idfx::prank;
    int q;

    /* find random number  */
    q= (int) fmod((double) a * in0, m);
    in0=q;

    return((real) ((double) q/(double)m));
}


void MySoundSpeed(DataBlock &data, const real t, IdefixArray3D<real> &cs) {
  IdefixArray1D<real> x1=data.x[IDIR];
  real h0 = h0Glob;
  idefix_for("MySoundSpeed",0,data.np_tot[KDIR],0,data.np_tot[JDIR],0,data.np_tot[IDIR],
              KOKKOS_LAMBDA (int k, int j, int i) {
                real R = x1(i);
                cs(k,j,i) = h0/sqrt(R);
              });
}

void MyViscosity(DataBlock &data, const real t, IdefixArray3D<real> &eta1, IdefixArray3D<real> &eta2) {
  IdefixArray4D<real> Vc=data.hydro.Vc;
  IdefixArray1D<real> x1=data.x[IDIR];
  real h0 = h0Glob;
  real alpha = alphaGlob;
  idefix_for("MyViscosity",0,data.np_tot[KDIR],0,data.np_tot[JDIR],0,data.np_tot[IDIR],
              KOKKOS_LAMBDA (int k, int j, int i) {
                real R = x1(i);
                real cs = h0/sqrt(R);
                eta1(k,j,i) = alpha*cs*h0*R*Vc(RHO,k,j,i);
                eta2(k,j,i) = ZERO_F;
              });

}

// User-defined boundaries
void UserdefBoundary(DataBlock& data, int dir, BoundarySide side, real t) {
  IdefixArray4D<real> Vc = data.hydro.Vc;
  IdefixArray1D<real> x1 = data.x[IDIR];
  IdefixArray1D<real> x3 = data.x[KDIR];
  real h0=h0Glob;
  real sigma0=sigma0Glob;
  real sigmaSlope=sigmaSlopeGlob;
    if(dir==IDIR) {


        int ighost,ibeg,iend;
        if(side == left) {
            ighost = data.beg[IDIR];
            ibeg = 0;
            iend = data.beg[IDIR];
            //return;
        }
        else if(side==right) {
            ighost = data.end[IDIR]-1;
            ibeg=data.end[IDIR];
            iend=data.np_tot[IDIR];
        }


        idefix_for("UserDefBoundary",0,data.np_tot[KDIR],0,data.np_tot[JDIR],ibeg,iend,
                    KOKKOS_LAMBDA (int k, int j, int i) {
                        real R=x1(i);
                        real z=x3(k);
                        real Vk = 1.0/sqrt(R);
                        real cs2=(h0*Vk)*(h0*Vk);

                        Vc(RHO,k,j,i) = sigma0/sqrt(2.0*M_PI)/(h0*R)*pow(R,-sigmaSlope) * exp(1.0/(cs2) * (1.0/sqrt(R*R+z*z)-1.0/R)) ;
                        Vc(VX1,k,j,i) = Vc(VX1,k,j,ighost);
                        Vc(VX2,k,j,i) = Vk*sqrt(R/sqrt(R*R + z*z)-(2.0+sigmaSlope)*h0*h0);
                        Vc(VX3,k,j,i) = Vc(VX3,k,j,ighost);
                    });
    }

    if( dir==KDIR) {
        IdefixArray4D<real> Vc = data.hydro.Vc;
        int kghost;
        int kbeg,kend;
        if(side == left) {
            kghost = data.beg[KDIR];
            kbeg = 0;
            kend = data.beg[KDIR];
            //return;
        }
        else if(side==right) {
            kghost = data.end[KDIR]-1;
            kbeg=data.end[KDIR];
            kend=data.np_tot[KDIR];
        }


        idefix_for("UserDefBoundary",kbeg,kend,0,data.np_tot[KDIR],0,data.np_tot[IDIR],
                    KOKKOS_LAMBDA (int k, int j, int i) {
                      real R=x1(i);
                      real z=x3(k);
                      real Vk = 1.0/sqrt(R);
                      real cs2=(h0*Vk)*(h0*Vk);

                      Vc(RHO,k,j,i) = Vc(RHO,kghost,j,i) ;
                      // Vc(RHO,k,j,i) = sigma0/sqrt(2.0*M_PI)/(h0*R)*pow(R,-sigmaSlope) * exp(1.0/(cs2) * (1.0/sqrt(R*R+z*z)-1.0/R)) ;
                      Vc(VX1,k,j,i) = Vc(VX1,kghost,j,i);
                      Vc(VX3,k,j,i) = ZERO_F;
                      Vc(VX2,k,j,i) = Vc(VX2,kghost,j,i);
                      // Vc(VX2,k,j,i) = Vk*sqrt(R/sqrt(R*R + z*z)-(1+sigmaSlope)*h0*h0);

                    });
    }

}

void Potential(DataBlock& data, const real t, IdefixArray1D<real>& x1, IdefixArray1D<real>& x2, IdefixArray1D<real>& x3, IdefixArray3D<real>& phi) {
  // const int subcycling = 5;

  real h0 = h0Glob;
  real qp = qpGlob;


  real Rp = RpGlob;
  real phiPlanet = sqrt((1.0+qp)/(Rp*Rp*Rp))*t;
  real xp = Rp * cos(phiPlanet);
  real yp = Rp * sin(phiPlanet);
  real zp = ZERO_F;

  real thicknessSmoothing = thicknessSmoothingGlob;
  real smoothing = h0*pow(Rp,0.0)*Rp*thicknessSmoothing;
  smoothing*=smoothing;
  // GWF do not forget indirect term due to planets (what about indirect term due gas)

    idefix_for("Potential",0,data.np_tot[KDIR], 0, data.np_tot[JDIR], 0, data.np_tot[IDIR],
        KOKKOS_LAMBDA (int k, int j, int i) {
        phi(k,j,i) = -1.0/sqrt(x1(i)*x1(i)+x3(k)*x3(k));
        real xc=x1(i)*cos(x2(j));
        real yc=x1(i)*sin(x2(j));
        real zc=x3(k);
        real dist = ((xc-xp)*(xc-xp)+
                    (yc-yp)*(yc-yp)+
                    (zc-zp)*(zc-zp));
        // term due to planet
        phi(k,j,i) += -qp/sqrt(dist+smoothing);
        // indirect term due to planet
        phi(k,j,i) += qp*(xc*xp+yc*yp+zc*zp)/(Rp*Rp*Rp);
    });

}

void FargoVelocity(DataBlock &data, IdefixArray2D<real> &Vphi) {
  IdefixArray1D<real> x1 = data.x[IDIR];

  idefix_for("FargoVphi",0,data.np_tot[KDIR], 0, data.np_tot[IDIR],
      KOKKOS_LAMBDA (int k, int i) {
      Vphi(k,i) = 1.0/sqrt(x1(i));
  });
}



// Analyse data to produce an ascii output
void Analysis(DataBlock & data) {
  // Mirror data on Host
  DataBlockHost d(data);

  // Sync it
  d.SyncFromDevice();

  // Get the minmax of the density field
  real minrho{INT_MAX};
  real maxrho{INT_MIN};
  for(int k = 0; k < d.np_tot[KDIR] ; k++) {
      for(int j = 0; j < d.np_tot[JDIR] ; j++) {
          for(int i = 0; i < d.np_tot[IDIR] ; i++) {
              if(d.Vc(RHO,k,j,i) < minrho) {
                  minrho = d.Vc(RHO,k,j,i);
              }
              if(d.Vc(RHO,k,j,i) > maxrho) {
                  maxrho = d.Vc(RHO,k,j,i);
              }
          }
      }
  }

  // Write the data in ascii to our file
  std::ofstream f;
  f.open("dataminmax.csv",std::ios::app);
  f.precision(10);
  f << std::scientific << minrho << "," << maxrho << std::endl;
  f.close();
}

// Default constructor
// Initialisation routine. Can be used to allocate
// Arrays or variables which are used later on
Setup::Setup(Input &input, Grid &grid, DataBlock &data, Output &output)// : m_planet(0)//, Planet &planet)
{
  // Set the function for userdefboundary
  data.hydro.EnrollUserDefBoundary(&UserdefBoundary);
  data.hydro.EnrollGravPotential(&Potential);
  data.hydro.EnrollIsoSoundSpeed(&MySoundSpeed);
  data.hydro.viscosity.EnrollViscousDiffusivity(&MyViscosity);
  data.hydro.fargo.EnrollVelocity(&FargoVelocity);
  // Enroll the analysis function
  output.EnrollAnalysis(&Analysis);
  sigma0Glob = input.GetReal("Setup","sigma0",0);
  sigmaSlopeGlob = input.GetReal("Setup","sigmaSlope",0);
  h0Glob = input.GetReal("Setup","h0",0);
  alphaGlob = input.GetReal("Setup","alpha",0);
  qpGlob = input.GetReal("Planet","qpl",0);
  RpGlob = input.GetReal("Planet","Rpl",0);
  thicknessSmoothingGlob = input.GetReal("Planet","thicknessSmoothing",0);
  idfx::cout << "alpha= " << alphaGlob << std::endl;

}

// This routine initialize the flow
// Note that data is on the device.
// One can therefore define locally
// a datahost and sync it, if needed
void Setup::InitFlow(DataBlock &data) {
    // Create a host copy
    DataBlockHost d(data);
    real x,y,z;
    real vphi,f;
    real h0=h0Glob;
    real sigma0=sigma0Glob;
    real sigmaSlope=sigmaSlopeGlob;

    for(int k = 0; k < d.np_tot[KDIR] ; k++) {
        for(int j = 0; j < d.np_tot[JDIR] ; j++) {
            for(int i = 0; i < d.np_tot[IDIR] ; i++) {
                real R=d.x[IDIR](i);
                real z=d.x[KDIR](k);
                real Vk=1.0/sqrt(R);

                real cs2=(h0*Vk)*(h0*Vk);

                d.Vc(RHO,k,j,i) = sigma0/sqrt(2.0*M_PI)/(h0*R)*pow(R,-sigmaSlope) * exp(1.0/ (cs2) * (1.0/sqrt(R*R+z*z)-1.0/R)) ;
                d.Vc(VX1,k,j,i) = 0.0;//+sin(8*d.x[JDIR](j));
                d.Vc(VX3,k,j,i) = 0.0;
                d.Vc(VX2,k,j,i) = Vk*sqrt( R / sqrt(R*R + z*z) -(2.0+sigmaSlope)*h0*h0);
            }
        }
    }

    // Send it all, if needed
    d.SyncToDevice();
}

// Analyse data to produce an output
void MakeAnalysis(DataBlock & data) {

}
