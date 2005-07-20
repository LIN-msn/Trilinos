// @HEADER
// ***********************************************************************
// 
//                IFPACK
//                 Copyright (2004) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

#include "Ifpack_ConfigDefs.h"
#if defined(HAVE_IFPACK_AZTECOO) && defined(HAVE_IFPACK_AMESOS) && defined(HAVE_IFPACK_TEUCHOS)
#ifdef HAVE_MPI
#include "Epetra_MpiComm.h"
#else
#include "Epetra_SerialComm.h"
#endif
#include "Epetra_CrsMatrix.h"
#include "Epetra_Vector.h"
#include "Epetra_LinearProblem.h"
#include "Trilinos_Util_CrsMatrixGallery.h"
#include "Teuchos_ParameterList.hpp"
#include "Ifpack_AdditiveSchwarz.h"
#include "Ifpack_AdditiveSchwarz.h"
#include "AztecOO.h"
#include "Ifpack_PointRelaxation.h"
#include "Ifpack_BlockRelaxation.h"
#include "Ifpack_DenseContainer.h"
#include "Ifpack_SparseContainer.h"
#include "Ifpack_Graph.h"
#include "Ifpack_Graph_Epetra_RowMatrix.h"
#include "Ifpack_Amesos.h"
#include "Ifpack_Utils.h"
#include "Ifpack_Partitioner.h"
#include "Ifpack_LinearPartitioner.h"
#include "Ifpack_GreedyPartitioner.h"
#include "Ifpack_METISPartitioner.h"

using namespace Trilinos_Util;

template <class T>
bool Test(Epetra_RowMatrix* Matrix, Teuchos::ParameterList& List)
{

  int NumVectors = 1;
  bool UseTranspose = false;

  Epetra_MultiVector LHS(Matrix->OperatorDomainMap(),NumVectors);
  Epetra_MultiVector RHS(Matrix->OperatorRangeMap(),NumVectors);
  Epetra_MultiVector LHSexact(Matrix->OperatorDomainMap(),NumVectors);

  LHS.PutScalar(0.0);
  LHSexact.Random();
  Matrix->Multiply(UseTranspose,LHSexact,RHS);

  Epetra_LinearProblem Problem(Matrix,&LHS,&RHS);

  T* Prec;
  
  Prec = new T(Matrix);
  assert(Prec != 0);

  IFPACK_CHK_ERR(Prec->SetParameters(List));
  IFPACK_CHK_ERR(Prec->Initialize());
  IFPACK_CHK_ERR(Prec->Compute());

  // create the AztecOO solver
  AztecOO AztecOOSolver(Problem);

  // specify solver
  AztecOOSolver.SetAztecOption(AZ_solver,AZ_gmres);
  AztecOOSolver.SetAztecOption(AZ_output,32);

  AztecOOSolver.SetPrecOperator(Prec);

  // solver. The solver should converge in one iteration,
  // or maximum two (numerical errors)
  AztecOOSolver.Iterate(1550,1e-8);

  cout << *Prec;
  delete Prec;
  
  vector<double> Norm(NumVectors);
  LHS.Update(1.0,LHSexact,-1.0);
  LHS.Norm2(&Norm[0]);
  for (int i = 0 ; i < NumVectors ; ++i) {
    cout << "Norm[" << i << "] = " << Norm[i] << endl;
    if (Norm[i] > 1e-3)
      return(false);
  }
  return(true);

}

int main(int argc, char *argv[])
{

#ifdef HAVE_MPI
  MPI_Init(&argc,&argv);
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm Comm;
#endif

  bool verbose = (Comm.MyPID() == 0);

  // size of the global matrix. 
  const int NumPoints = 900;

  CrsMatrixGallery Gallery("laplace_2d", Comm);
  Gallery.Set("problem_size", NumPoints);
  Gallery.Set("map_type", "linear");
  Epetra_RowMatrix* Matrix = Gallery.GetMatrix();

  Teuchos::ParameterList List, DefaultList;

  // test the preconditioner
  int TestPassed = true;


  if (!Test<Ifpack_Amesos>(Matrix,List)) {
    TestPassed = false;
  }

  // FIXME
#if 0
  if (!Test<Ifpack_AdditiveSchwarz<Ifpack_BlockRelaxation<Ifpack_DenseContainer> > >(Matrix,List)) {
    TestPassed = false;
  }
#endif

  // this is ok as long as just one sweep is applied
  List = DefaultList;
  List.set("relaxation: type", "Gauss-Seidel");
  if (!Test<Ifpack_PointRelaxation>(Matrix,List)) {
    TestPassed = false;
  }

  // this is ok as long as just one sweep is applied
  List = DefaultList;
  List.set("relaxation: type", "symmetric Gauss-Seidel");
  List.set("relaxation: sweeps", 5);
  List.set("partitioner: local parts", 128);
  List.set("partitioner: type", "linear");
  if (!Test<Ifpack_BlockRelaxation<Ifpack_DenseContainer> >(Matrix,List)) {
    TestPassed = false;
  }
 
  // this is ok as long as just one sweep is applied
  List = DefaultList;
  List.set("relaxation: type", "symmetric Gauss-Seidel");
  List.set("partitioner: local parts", 128);
  List.set("partitioner: type", "linear");
  if (!Test<Ifpack_BlockRelaxation<Ifpack_SparseContainer<Ifpack_Amesos> > >(Matrix,List)) {
    TestPassed = false;
  }

  // this is ok as long as just one sweep is applied
  List = DefaultList;
  List.set("relaxation: type", "symmetric Gauss-Seidel");
  List.set("partitioner: local parts", 128);
  List.set("partitioner: type", "linear");
  if (!Test<Ifpack_AdditiveSchwarz<Ifpack_BlockRelaxation<Ifpack_SparseContainer<Ifpack_Amesos> > > >(Matrix,List)) {
    TestPassed = false;
  }
  if (!TestPassed) {
    cerr << "Test `TestAll.exe' FAILED!" << endl;
    exit(EXIT_FAILURE);
  }

#ifdef HAVE_MPI
  MPI_Finalize(); 
#endif
  if (verbose)
    cout << "Test `TestAll.exe' passed!" << endl;

  exit(EXIT_SUCCESS);
}

#else

#ifdef HAVE_MPI
#include "Epetra_MpiComm.h"
#else
#include "Epetra_SerialComm.h"
#endif

int main(int argc, char *argv[])
{

#ifdef HAVE_MPI
  MPI_Init(&argc,&argv);
  Epetra_MpiComm Comm( MPI_COMM_WORLD );
#else
  Epetra_SerialComm Comm;
#endif

  puts("please configure IFPACK with --eanble-aztecoo --enable-teuchos");
  puts("--enable-amesos to run this test");

#ifdef HAVE_MPI
  MPI_Finalize() ;
#endif
  return(EXIT_SUCCESS);
}

#endif
