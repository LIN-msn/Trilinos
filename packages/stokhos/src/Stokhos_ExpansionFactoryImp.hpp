// $Id$ 
// $Source$ 
// @HEADER
// ***********************************************************************
// 
//                           Stokhos Package
//                 Copyright (2009) Sandia Corporation
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
// Questions? Contact Eric T. Phipps (etphipp@sandia.gov).
// 
// ***********************************************************************
// @HEADER

#include "Teuchos_TestForException.hpp"

#include "Stokhos_AlgebraicOrthogPolyExpansion.hpp"
#include "Stokhos_QuadOrthogPolyExpansion.hpp"
#include "Stokhos_ForUQTKOrthogPolyExpansion.hpp"
#include "Stokhos_DerivOrthogPolyExpansion.hpp"

template <typename ordinal_type, typename value_type>
Teuchos::RCP<Stokhos::OrthogPolyExpansion<ordinal_type, value_type> >
Stokhos::ExpansionFactory<ordinal_type, value_type>::
create(Teuchos::ParameterList& sgParams)
{
  Teuchos::ParameterList& basisParams = sgParams.sublist("Basis");
  Teuchos::RCP< const Stokhos::OrthogPolyBasis<ordinal_type,value_type> > basis = basisParams.template get< Teuchos::RCP<const Stokhos::OrthogPolyBasis<ordinal_type,value_type> > >("Stochastic Galerkin Basis");

  Teuchos::RCP<const Stokhos::Sparse3Tensor<ordinal_type,value_type> > Cijk =
    sgParams.template get<Teuchos::RCP<const Stokhos::Sparse3Tensor<ordinal_type,value_type> > >("Sparse Triple Product");

  Teuchos::ParameterList& expParams = sgParams.sublist("Expansion");
  std::string exp_type = expParams.get("Type", "Algebraic");
  Teuchos::RCP< Stokhos::OrthogPolyExpansion<ordinal_type,value_type> > expansion;
  if (exp_type == "Algebraic")
    expansion = 
      Teuchos::rcp(new Stokhos::AlgebraicOrthogPolyExpansion<ordinal_type,value_type>(basis, Cijk));
  else if (exp_type == "Quadrature") {
    Teuchos::ParameterList& quadParams = sgParams.sublist("Quadrature");
    Teuchos::RCP<const Stokhos::Quadrature<ordinal_type,value_type> > quad =
      quadParams.template get<Teuchos::RCP<const Stokhos::Quadrature<ordinal_type,value_type> > >("Stochastic Galerkin Quadrature");
    expansion = 
      Teuchos::rcp(new Stokhos::QuadOrthogPolyExpansion<ordinal_type,value_type>(basis, Cijk, quad));
  }
  else if (exp_type == "For UQTK") {
#ifdef HAVE_STOKHOS_FORUQTK
    typename Stokhos::ForUQTKOrthogPolyExpansion<ordinal_type,value_type>::EXPANSION_METHOD method = 
      expParams.get("ForUQTK Expansion Method", 
		    Stokhos::ForUQTKOrthogPolyExpansion<ordinal_type,value_type>::TAYLOR);
    value_type rtol = expParams.get("ForUQTK Expansion Tolerance", 1e-12);
    expansion = 
      Teuchos::rcp(new Stokhos::ForUQTKOrthogPolyExpansion<ordinal_type,value_type>(basis, method, rtol));
#else
    TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
		       std::endl << 
		       "Error!  Stokhos::ExpansionFactory::create():  " <<
		       "ForUQTK expansion requires ForUQTK!" << std::endl);
#endif
  }
  else if (exp_type == "Derivative") {
    Teuchos::RCP<const Stokhos::DerivBasis<ordinal_type,value_type> > deriv_basis = Teuchos::rcp_dynamic_cast<const Stokhos::DerivBasis<ordinal_type,value_type> >(basis, true);
    Teuchos::RCP<Teuchos::SerialDenseMatrix<ordinal_type,value_type> > Bij;
    if (sgParams.template isType<Teuchos::RCP<const Teuchos::SerialDenseMatrix<ordinal_type,value_type> > >("Derivative Double Product"))
      Bij = sgParams.template get<const Teuchos::RCP<Teuchos::SerialDenseMatrix<ordinal_type,value_type> > >("Derivative Double Product");
    else
      Bij = deriv_basis->computeDerivDoubleProductTensor();
    Teuchos::RCP<const Stokhos::Dense3Tensor<ordinal_type,value_type> > Dijk;
    if (sgParams.template isType<Teuchos::RCP<const Stokhos::Dense3Tensor<ordinal_type,value_type> > >("Derivative Triple Product"))
      Dijk = sgParams.template get<Teuchos::RCP<const Stokhos::Dense3Tensor<ordinal_type,value_type> > >("Derivative Triple Product");
    else
      Dijk = deriv_basis->computeDerivTripleProductTensor(Bij, Cijk);
    expansion = 
      Teuchos::rcp(new 
		   Stokhos::DerivOrthogPolyExpansion<ordinal_type,value_type>(
		     deriv_basis, Bij, Cijk, Dijk));
  }
  else
    TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
		       std::endl << 
		       "Error!  Stokhos::ExpansionFactory::create():  " <<
		       "Invalid expansion type  " << exp_type << std::endl);

  expParams.set("Stochastic Galerkin Expansion", expansion);
  return expansion;
}
