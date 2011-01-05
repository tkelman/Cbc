// $Id$
// Copyright (C) 2006, International Business Machines
// Corporation and others.  All Rights Reserved.
// This code is licensed under the terms of the Eclipse Public License (EPL).

// edwin 12/5/09 carved out of CbcHeuristicRINS

#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif
#include <cassert>
#include <cstdlib>
#include <cmath>
#include <cfloat>

#include "OsiSolverInterface.hpp"
#include "CbcModel.hpp"
#include "CbcMessage.hpp"
#include "CbcHeuristicRENS.hpp"
#include "CoinWarmStartBasis.hpp"
#include "CoinSort.hpp"
#include "CbcBranchActual.hpp"
#include "CbcStrategy.hpp"
#include "CglPreProcess.hpp"

// Default Constructor
CbcHeuristicRENS::CbcHeuristicRENS()
        : CbcHeuristic()
{
    numberTries_ = 0;
    rensType_ = 0;
    whereFrom_ = 256 + 1;
}

// Constructor with model - assumed before cuts

CbcHeuristicRENS::CbcHeuristicRENS(CbcModel & model)
        : CbcHeuristic(model)
{
    numberTries_ = 0;
    rensType_ = 0;
    whereFrom_ = 256 + 1;
}

// Destructor
CbcHeuristicRENS::~CbcHeuristicRENS ()
{
}

// Clone
CbcHeuristic *
CbcHeuristicRENS::clone() const
{
    return new CbcHeuristicRENS(*this);
}

// Assignment operator
CbcHeuristicRENS &
CbcHeuristicRENS::operator=( const CbcHeuristicRENS & rhs)
{
    if (this != &rhs) {
        CbcHeuristic::operator=(rhs);
        numberTries_ = rhs.numberTries_;
	rensType_ = rhs.rensType_;
    }
    return *this;
}

// Copy constructor
CbcHeuristicRENS::CbcHeuristicRENS(const CbcHeuristicRENS & rhs)
        :
        CbcHeuristic(rhs),
        numberTries_(rhs.numberTries_),
	rensType_(rhs.rensType_)
{
}
// Resets stuff if model changes
void
CbcHeuristicRENS::resetModel(CbcModel * )
{
}
int
CbcHeuristicRENS::solution(double & solutionValue,
                           double * betterSolution)
{
    int returnCode = 0;
    const double * bestSolution = model_->bestSolution();
    if ((numberTries_&&(rensType_&16)==0) || numberTries_>1 || (when() < 2 && bestSolution))
        return 0;
    numberTries_++;
    OsiSolverInterface * solver = model_->solver();

    int numberIntegers = model_->numberIntegers();
    const int * integerVariable = model_->integerVariable();

    const double * currentSolution = solver->getColSolution();
    OsiSolverInterface * newSolver = cloneBut(3); // was model_->continuousSolver()->clone();
    newSolver->resolve();
    double direction = newSolver->getObjSense();
    double cutoff=model_->getCutoff();
    //newSolver->getDblParam(OsiDualObjectiveLimit, cutoff);
    //cutoff *= direction;
    double gap = cutoff - newSolver->getObjValue() * direction ;
    double tolerance;
    newSolver->getDblParam(OsiDualTolerance, tolerance) ;
    if (gap > 0.0 || !newSolver->isProvenOptimal()) {
      gap += 100.0 * tolerance;
      int nFix = newSolver->reducedCostFix(gap);
      if (nFix) {
	char line [200];
	sprintf(line, "Reduced cost fixing fixed %d variables", nFix);
	model_->messageHandler()->message(CBC_FPUMP1, model_->messages())
	  << line
	  << CoinMessageEol;
      }
    } else {
      return 0; // finished?
    }
    int numberColumns = solver->getNumCols();
    double * dj = CoinCopyOfArray(solver->getReducedCost(),numberColumns);
    int type = rensType_&15;
    double djTolerance = (type!=1) ? -1.0e30 : 1.0e-4;
    const double * colLower = newSolver->getColLower();
    const double * colUpper = newSolver->getColUpper();
    int numberFixed = 0;
    if (type==3) {
      double total=0.0;
      int n=0;
      CoinWarmStartBasis * basis =
	dynamic_cast<CoinWarmStartBasis *>(solver->getWarmStart()) ;
      if (basis&&basis->getNumArtificial()) {
	for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
	  if (colUpper[iColumn]>colLower[iColumn]&&
	      basis->getStructStatus(iColumn) !=
	      CoinWarmStartBasis::basic) {
	    n++;
	    total += fabs(dj[iColumn]);
	  }
	}
	if (n)
	  djTolerance = (0.01*total)/static_cast<double>(n);
	delete basis;
      }
    } else if (type>=5&&type<=10) {
      /* 5 fix sets at one
	 6 fix on dj but leave unfixed SOS slacks
	 7 fix sets at one but use pi
	 8 fix all at zero but leave unfixed SOS slacks
	 9 as 8 but only fix all at zero if just one in set nonzero
	 10 as 7 but pi other way
      */
      // SOS type fixing
      bool fixSets = (type==5)||(type==7)||(type==10);
      CoinWarmStartBasis * basis =
	dynamic_cast<CoinWarmStartBasis *>(solver->getWarmStart()) ;
      if (basis&&basis->getNumArtificial()) {
	//const double * rowLower = solver->getRowLower();
	const double * rowUpper = solver->getRowUpper();
	
	int numberRows = solver->getNumRows();
	// Column copy
	const CoinPackedMatrix * matrix = solver->getMatrixByCol();
	const double * element = matrix->getElements();
	const int * row = matrix->getIndices();
	const CoinBigIndex * columnStart = matrix->getVectorStarts();
	const int * columnLength = matrix->getVectorLengths();
	double * bestDj = new double [numberRows];
	for (int i=0;i<numberRows;i++) {
	  if (rowUpper[i]==1.0)
	    bestDj[i]=1.0e20;
	  else
	    bestDj[i]=1.0e30;
	}
	for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
	  if (colUpper[iColumn]>colLower[iColumn]) {
	    CoinBigIndex j;
	    if (currentSolution[iColumn]>1.0e-6&&
		currentSolution[iColumn]<0.999999) {
	      for (j = columnStart[iColumn];
		   j < columnStart[iColumn] + columnLength[iColumn]; j++) {
		int iRow = row[j];
		if (bestDj[iRow]<1.0e30) {
		  if (element[j] != 1.0)
		    bestDj[iRow]=1.0e30;
		  else
		    bestDj[iRow]=1.0e25;
		}
	      }
	    } else if ( basis->getStructStatus(iColumn) !=
	      CoinWarmStartBasis::basic) {
	      for (j = columnStart[iColumn];
		   j < columnStart[iColumn] + columnLength[iColumn]; j++) {
		int iRow = row[j];
		if (bestDj[iRow]<1.0e25) {
		  if (element[j] != 1.0)
		    bestDj[iRow]=1.0e30;
		  else
		    bestDj[iRow]=CoinMin(fabs(dj[iColumn]),bestDj[iRow]);
		}
	      }
	    }
	  }
	}
	int nSOS=0;
	double * sort = new double [numberRows];
	const double * pi = newSolver->getRowPrice();
	for (int i=0;i<numberRows;i++) {
	  if (bestDj[i]<1.0e30) {
	    if (type==5)
	      sort[nSOS++]=bestDj[i];
	    else if (type==7)
	      sort[nSOS++]=-fabs(pi[i]);
	    else
	      sort[nSOS++]=fabs(pi[i]);
	  }
	}
	if (10*nSOS>8*numberRows) {
	  std::sort(sort,sort+nSOS);
	  int last = static_cast<int>(nSOS*0.9*fractionSmall_);
	  double tolerance = sort[last];
	  for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
	    if (colUpper[iColumn]>colLower[iColumn]) {
	      CoinBigIndex j;
	      if (currentSolution[iColumn]<=1.0e-6||
		  currentSolution[iColumn]>=0.999999) {
		if (fixSets) {
		  for (j = columnStart[iColumn];
		       j < columnStart[iColumn] + columnLength[iColumn]; j++) {
		    int iRow = row[j];
		    double useDj;
		    if (type==5) 
		      useDj = bestDj[iRow];
		    else if (type==7)
		      useDj= -fabs(pi[iRow]);
		    else
		      useDj= fabs(pi[iRow]);
		    if (bestDj[iRow]<1.0e30&&useDj>=tolerance) {
		      numberFixed++;
		      if (currentSolution[iColumn]<=1.0e-6)
			newSolver->setColUpper(iColumn,0.0);
		      else if (currentSolution[iColumn]>=0.999999) 
			newSolver->setColLower(iColumn,1.0);
		    }
		  }
		} else if (columnLength[iColumn]==1) {
		  // leave more slacks
		  int iRow = row[columnStart[iColumn]];
		  if (bestDj[iRow]<1.0e30) {
		    // fake dj
		    dj[iColumn] *= 0.000001;
		  }
		} else if (type==8||type==9) {
		  if (currentSolution[iColumn]<=1.0e-6) {
		    if (type==8) {
		      dj[iColumn] *= 1.0e6;
		    } else {
		      bool fix=false;
		      for (j = columnStart[iColumn];
			   j < columnStart[iColumn] + columnLength[iColumn]; j++) {
			int iRow = row[j];
			if (bestDj[iRow]<1.0e25) {
			  fix=true;
			  break;
			}
		      }
		      if (fix) {
			dj[iColumn] *= 1.0e6;
		      }
		    }
		  } else {
		    dj[iColumn] *= 0.000001;
		  }
		}
	      }
	    }
	  }
	  if (fixSets)
	    djTolerance = 1.0e30;
	}
	delete basis;
	delete [] sort;
	delete [] bestDj;
      }
    }
    // Do dj to get right number
    if (type==4||type==6||type>7) {
      double * sort = new double [numberColumns];
      for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
	sort[iColumn]=1.0e30;
	if (colUpper[iColumn]>colLower[iColumn]) {
	  sort[iColumn] = fabs(dj[iColumn]);
	}
      }
      std::sort(sort,sort+numberColumns);
      int last = static_cast<int>(numberColumns*fractionSmall_);
      djTolerance = CoinMax(sort[last],1.0e-5);
      delete [] sort;
    }
    
    double primalTolerance;
    solver->getDblParam(OsiPrimalTolerance, primalTolerance);

    int i;
    int numberTightened = 0;
    int numberAtBound = 0;
    int numberContinuous = numberColumns - numberIntegers;

    for (i = 0; i < numberIntegers; i++) {
        int iColumn = integerVariable[i];
        double value = currentSolution[iColumn];
        double lower = colLower[iColumn];
        double upper = colUpper[iColumn];
        value = CoinMax(value, lower);
        value = CoinMin(value, upper);
	double djValue=dj[iColumn]*direction;
#define RENS_FIX_ONLY_LOWER
#ifndef RENS_FIX_ONLY_LOWER
        if (fabs(value - floor(value + 0.5)) < 1.0e-8) {
            value = floor(value + 0.5);
            if (value == lower || value == upper)
                numberAtBound++;
            newSolver->setColLower(iColumn, value);
            newSolver->setColUpper(iColumn, value);
            numberFixed++;
        } else if (colUpper[iColumn] - colLower[iColumn] >= 2.0) {
            numberTightened++;
            newSolver->setColLower(iColumn, floor(value));
            newSolver->setColUpper(iColumn, ceil(value));
        }
#else
        if (fabs(value - floor(value + 0.5)) < 1.0e-8 &&
                floor(value + 0.5) == lower &&
	    djValue > djTolerance ) {
	  value = floor(value + 0.5);
	  numberAtBound++;
	  newSolver->setColLower(iColumn, value);
	  newSolver->setColUpper(iColumn, value);
	  numberFixed++;
        } else if (fabs(value - floor(value + 0.5)) < 1.0e-8 &&
                floor(value + 0.5) == upper &&
		   -djValue > djTolerance && (djTolerance > 0.0||type==2)) {
	  value = floor(value + 0.5);
	  numberAtBound++;
	  newSolver->setColLower(iColumn, value);
	  newSolver->setColUpper(iColumn, value);
	  numberFixed++;
        } else if (colUpper[iColumn] - colLower[iColumn] >= 2.0 &&
		   djTolerance <0.0) {
            numberTightened++;
            if (fabs(value - floor(value + 0.5)) < 1.0e-8) {
                value = floor(value + 0.5);
                if (value < upper) {
                    newSolver->setColLower(iColumn, CoinMax(value - 1.0, lower));
                    newSolver->setColUpper(iColumn, CoinMin(value + 1.0, upper));
                } else {
                    newSolver->setColLower(iColumn, upper - 1.0);
                }
            } else {
                newSolver->setColLower(iColumn, floor(value));
                newSolver->setColUpper(iColumn, ceil(value));
            }
        }
#endif
    }
    delete [] dj;
    if (numberFixed > numberIntegers / 5) {
        if ( numberFixed < numberColumns / 5) {
#define RENS_FIX_CONTINUOUS
#ifdef RENS_FIX_CONTINUOUS
            const double * colLower = newSolver->getColLower();
            //const double * colUpper = newSolver->getColUpper();
            int nAtLb = 0;
            double sumDj = 0.0;
            const double * dj = newSolver->getReducedCost();
            for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
                if (!newSolver->isInteger(iColumn)) {
                    double value = currentSolution[iColumn];
                    if (value < colLower[iColumn] + 1.0e-8) {
                        double djValue = dj[iColumn] * direction;
                        nAtLb++;
                        sumDj += djValue;
                    }
                }
            }
            if (nAtLb) {
                // fix some continuous
                double * sort = new double[nAtLb];
                int * which = new int [nAtLb];
                double threshold = CoinMax((0.01 * sumDj) / static_cast<double>(nAtLb), 1.0e-6);
                int nFix2 = 0;
                for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
                    if (!newSolver->isInteger(iColumn)) {
                        double value = currentSolution[iColumn];
                        if (value < colLower[iColumn] + 1.0e-8) {
                            double djValue = dj[iColumn] * direction;
                            if (djValue > threshold) {
                                sort[nFix2] = -djValue;
                                which[nFix2++] = iColumn;
                            }
                        }
                    }
                }
                CoinSort_2(sort, sort + nFix2, which);
                nFix2 = CoinMin(nFix2, (numberColumns - numberFixed) / 2);
                for (int i = 0; i < nFix2; i++) {
                    int iColumn = which[i];
                    newSolver->setColUpper(iColumn, colLower[iColumn]);
                }
                delete [] sort;
                delete [] which;
#ifdef CLP_INVESTIGATE2
                printf("%d integers fixed (%d tightened) (%d at bound), and %d continuous fixed at lb\n",
                       numberFixed, numberTightened, numberAtBound, nFix2);
#endif
            }
#endif
        }
#ifdef COIN_DEVELOP
        printf("%d integers fixed and %d tightened\n", numberFixed, numberTightened);
#endif
        returnCode = smallBranchAndBound(newSolver, numberNodes_, betterSolution, solutionValue,
                                         model_->getCutoff(), "CbcHeuristicRENS");
        if (returnCode < 0) {
            returnCode = 0; // returned on size
#ifdef RENS_FIX_CONTINUOUS
            if (numberContinuous > numberIntegers && numberFixed >= numberColumns / 5) {
                const double * colLower = newSolver->getColLower();
                //const double * colUpper = newSolver->getColUpper();
                int nAtLb = 0;
                double sumDj = 0.0;
                const double * dj = newSolver->getReducedCost();
                double direction = newSolver->getObjSense();
                for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
                    if (!newSolver->isInteger(iColumn)) {
                        double value = currentSolution[iColumn];
                        if (value < colLower[iColumn] + 1.0e-8) {
                            double djValue = dj[iColumn] * direction;
                            nAtLb++;
                            sumDj += djValue;
                        }
                    }
                }
                if (nAtLb) {
                    // fix some continuous
                    double * sort = new double[nAtLb];
                    int * which = new int [nAtLb];
                    double threshold = CoinMax((0.01 * sumDj) / static_cast<double>(nAtLb), 1.0e-6);
                    int nFix2 = 0;
                    for (int iColumn = 0; iColumn < numberColumns; iColumn++) {
                        if (!newSolver->isInteger(iColumn)) {
                            double value = currentSolution[iColumn];
                            if (value < colLower[iColumn] + 1.0e-8) {
                                double djValue = dj[iColumn] * direction;
                                if (djValue > threshold) {
                                    sort[nFix2] = -djValue;
                                    which[nFix2++] = iColumn;
                                }
                            }
                        }
                    }
                    CoinSort_2(sort, sort + nFix2, which);
                    nFix2 = CoinMin(nFix2, (numberColumns - numberFixed) / 2);
                    for (int i = 0; i < nFix2; i++) {
                        int iColumn = which[i];
                        newSolver->setColUpper(iColumn, colLower[iColumn]);
                    }
                    delete [] sort;
                    delete [] which;
#ifdef CLP_INVESTIGATE2
                    printf("%d integers fixed (%d tightened) (%d at bound), and %d continuous fixed at lb\n",
                           numberFixed, numberTightened, numberAtBound, nFix2);
#endif
                }
                returnCode = smallBranchAndBound(newSolver, numberNodes_, betterSolution, solutionValue,
                                                 model_->getCutoff(), "CbcHeuristicRENS");
#endif
            }
        }
        //printf("return code %d",returnCode);
        if ((returnCode&2) != 0) {
            // could add cut
            returnCode &= ~2;
#ifdef COIN_DEVELOP
            if (!numberTightened && numberFixed == numberAtBound)
                printf("could add cut with %d elements\n", numberFixed);
#endif
        } else {
            //printf("\n");
        }
    }

    delete newSolver;
    return returnCode;
}
// update model
void CbcHeuristicRENS::setModel(CbcModel * model)
{
    model_ = model;
}

