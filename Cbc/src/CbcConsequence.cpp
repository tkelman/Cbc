// Edwin 11/12/2009 carved from CbcBranchBase
#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif
#include <cassert>
#include <cstdlib>
#include <cmath>
#include <cfloat>

#include "CbcConsequence.hpp"

// Default constructor
CbcConsequence::CbcConsequence()
{
}


// Destructor
CbcConsequence::~CbcConsequence ()
{
}

// Copy constructor
CbcConsequence::CbcConsequence ( const CbcConsequence & /*rhs*/)
{
}

// Assignment operator
CbcConsequence &
CbcConsequence::operator=( const CbcConsequence & rhs)
{
    if (this != &rhs) {
    }
    return *this;
}
