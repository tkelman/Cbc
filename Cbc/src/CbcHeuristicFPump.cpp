// Copyright (C) 2004, International Business Machines
// Corporation and others.  All Rights Reserved.
#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif
#include <cassert>
#include <cmath>
#include <cfloat>

#include "OsiSolverInterface.hpp"
#include "CbcModel.hpp"
#include "CbcMessage.hpp"
#include "CbcHeuristicFPump.hpp"
#include "CbcBranchActual.hpp"
#include "CoinHelperFunctions.hpp"
#include "CoinTime.hpp"


// Default Constructor
CbcHeuristicFPump::CbcHeuristicFPump() 
  :CbcHeuristic(),
   startTime_(0.0),
   maximumTime_(0.0),
   downValue_(0.5),
   fakeCutoff_(COIN_DBL_MAX),
   absoluteIncrement_(0.0),
   relativeIncrement_(0.0),
   initialWeight_(0.0),
   weightFactor_(0.1),
   maximumPasses_(100),
   maximumRetries_(1),
   accumulate_(0),
   roundExpensive_(false)
{
  setWhen(1);
}

// Constructor from model
CbcHeuristicFPump::CbcHeuristicFPump(CbcModel & model,
				     double downValue,bool roundExpensive)
  :CbcHeuristic(model),
   startTime_(0.0),
   maximumTime_(0.0),
   downValue_(downValue),
   fakeCutoff_(COIN_DBL_MAX),
   absoluteIncrement_(0.0),
   relativeIncrement_(0.0),
   initialWeight_(0.0),
   weightFactor_(0.1),
   maximumPasses_(100),
   maximumRetries_(1),
   accumulate_(0),
   roundExpensive_(roundExpensive)
{
  setWhen(1);
}

// Destructor 
CbcHeuristicFPump::~CbcHeuristicFPump ()
{
}

// Clone
CbcHeuristic *
CbcHeuristicFPump::clone() const
{
  return new CbcHeuristicFPump(*this);
}
// Create C++ lines to get to current state
void 
CbcHeuristicFPump::generateCpp( FILE * fp) 
{
  CbcHeuristicFPump other;
  fprintf(fp,"0#include \"CbcHeuristicFPump.hpp\"\n");
  fprintf(fp,"3  CbcHeuristicFPump heuristicFPump(*cbcModel);\n");
  if (when_!=other.when_)
    fprintf(fp,"3  heuristicFPump.setWhen(%d);\n",when_);
  else
    fprintf(fp,"4  heuristicFPump.setWhen(%d);\n",when_);
  if (maximumPasses_!=other.maximumPasses_)
    fprintf(fp,"3  heuristicFPump.setMaximumPasses(%d);\n",maximumPasses_);
  else
    fprintf(fp,"4  heuristicFPump.setMaximumPasses(%d);\n",maximumPasses_);
  if (maximumRetries_!=other.maximumRetries_)
    fprintf(fp,"3  heuristicFPump.setMaximumRetries(%d);\n",maximumRetries_);
  else
    fprintf(fp,"4  heuristicFPump.setMaximumRetries(%d);\n",maximumRetries_);
  if (accumulate_!=other.accumulate_)
    fprintf(fp,"3  heuristicFPump.setAccumulate(%d);\n",accumulate_);
  else
    fprintf(fp,"4  heuristicFPump.setAccumulate(%d);\n",accumulate_);
  if (maximumTime_!=other.maximumTime_)
    fprintf(fp,"3  heuristicFPump.setMaximumTime(%g);\n",maximumTime_);
  else
    fprintf(fp,"4  heuristicFPump.setMaximumTime(%g);\n",maximumTime_);
  if (fakeCutoff_!=other.fakeCutoff_)
    fprintf(fp,"3  heuristicFPump.setFakeCutoff(%g);\n",fakeCutoff_);
  else
    fprintf(fp,"4  heuristicFPump.setFakeCutoff(%g);\n",fakeCutoff_);
  if (absoluteIncrement_!=other.absoluteIncrement_)
    fprintf(fp,"3  heuristicFPump.setAbsoluteIncrement(%g);\n",absoluteIncrement_);
  else
    fprintf(fp,"4  heuristicFPump.setAbsoluteIncrement(%g);\n",absoluteIncrement_);
  if (relativeIncrement_!=other.relativeIncrement_)
    fprintf(fp,"3  heuristicFPump.setRelativeIncrement(%g);\n",relativeIncrement_);
  else
    fprintf(fp,"4  heuristicFPump.setRelativeIncrement(%g);\n",relativeIncrement_);
  fprintf(fp,"3  cbcModel->addHeuristic(&heuristicFPump);\n");
  if (initialWeight_!=other.initialWeight_)
    fprintf(fp,"3  heuristicFPump.setInitialWeight(%g);\n",initialWeight_);
  else
    fprintf(fp,"4  heuristicFPump.setInitialWeight(%g);\n",initialWeight_);
  if (weightFactor_!=other.weightFactor_)
    fprintf(fp,"3  heuristicFPump.setWeightFactor(%g);\n",weightFactor_);
  else
    fprintf(fp,"4  heuristicFPump.setWeightFactor(%g);\n",weightFactor_);
}

// Copy constructor 
CbcHeuristicFPump::CbcHeuristicFPump(const CbcHeuristicFPump & rhs)
:
  CbcHeuristic(rhs),
  startTime_(rhs.startTime_),
  maximumTime_(rhs.maximumTime_),
  downValue_(rhs.downValue_),
  fakeCutoff_(rhs.fakeCutoff_),
  absoluteIncrement_(rhs.absoluteIncrement_),
  relativeIncrement_(rhs.relativeIncrement_),
  initialWeight_(rhs.initialWeight_),
  weightFactor_(rhs.weightFactor_),
  maximumPasses_(rhs.maximumPasses_),
  maximumRetries_(rhs.maximumRetries_),
  accumulate_(rhs.accumulate_),
  roundExpensive_(rhs.roundExpensive_)
{
  setWhen(rhs.when());
}
// Resets stuff if model changes
void 
CbcHeuristicFPump::resetModel(CbcModel * model)
{
}

/**************************BEGIN MAIN PROCEDURE ***********************************/

// See if feasibility pump will give better solution
// Sets value of solution
// Returns 1 if solution, 0 if not
int
CbcHeuristicFPump::solution(double & solutionValue,
			 double * betterSolution)
{
  if (!when()||(when()==1&&model_->phase()!=1))
    return 0; // switched off
  // See if at root node
  bool atRoot = model_->getNodeCount()==0;
  int passNumber = model_->getCurrentPassNumber();
  // just do once
  if (!atRoot||passNumber!=1)
    return 0;
  // probably a good idea
  if (model_->getSolutionCount()) return 0;
  // loop round doing repeated pumps
  double cutoff;
  model_->solver()->getDblParam(OsiDualObjectiveLimit,cutoff);
  double direction = model_->solver()->getObjSense();
  cutoff *= direction;
  cutoff = CoinMin(cutoff,solutionValue);
  // check plausible and space for rounded solution
  int numberColumns = model_->getNumCols();
  int numberIntegers = model_->numberIntegers();
  const int * integerVariableOrig = model_->integerVariable();

  // 1. initially check 0-1
  int i,j;
  int general=0;
  int * integerVariable = new int[numberIntegers];
  const double * lower = model_->solver()->getColLower();
  const double * upper = model_->solver()->getColUpper();
  bool doGeneral = (accumulate_&(4+8))!=0;
  j=0;
  for (i=0;i<numberIntegers;i++) {
    int iColumn = integerVariableOrig[i];
#ifndef NDEBUG
    const OsiObject * object = model_->object(i);
    const CbcSimpleInteger * integerObject = 
      dynamic_cast<const  CbcSimpleInteger *> (object);
    const OsiSimpleInteger * integerObject2 = 
      dynamic_cast<const  OsiSimpleInteger *> (object);
    assert(integerObject||integerObject2);
#endif
    if (upper[iColumn]-lower[iColumn]>1.000001) {
      general++;
      if (doGeneral)
	integerVariable[j++]=iColumn;
    } else {
      integerVariable[j++]=iColumn;
    }
  }
  if (general*3>2*numberIntegers&&!doGeneral) {
    delete [] integerVariable;
    return 0;
  } else if ((accumulate_&8)==0) {
    doGeneral=false;
  }
  if (!general)
    doGeneral=false;
  int numberIntegersOrig = numberIntegers;
  numberIntegers = j;
  double * newSolution = new double [numberColumns];
  double newSolutionValue=COIN_DBL_MAX;
  bool solutionFound=false;
  char * usedColumn = NULL;
  double * lastSolution=NULL;
  int fixContinuous=0;
  bool fixInternal=false;
  if (when_>=11&&when_<=15) {
    fixInternal = when_ >11&&when_<15;
    if (when_<13)
      fixContinuous = 0;
    else if (when_!=14)
      fixContinuous=1;
    else
      fixContinuous=2;
    when_=1;
    usedColumn = new char [numberColumns];
    memset(usedColumn,0,numberColumns);
    model_->solver()->resolve();
    assert (model_->solver()->isProvenOptimal());
    lastSolution = CoinCopyOfArray(model_->solver()->getColSolution(),numberColumns);
  }
  int finalReturnCode=0;
  int totalNumberPasses=0;
  int numberTries=0;
  while (true) {
    int numberPasses=0;
    numberTries++;
    // Clone solver - otherwise annoys root node computations
    OsiSolverInterface * solver = model_->solver()->clone();
    // if cutoff exists then add constraint
    if (fabs(cutoff)<1.0e20&&(fakeCutoff_!=COIN_DBL_MAX||numberTries>1)) {
      cutoff = CoinMin(cutoff,fakeCutoff_);
      const double * objective = solver->getObjCoefficients();
      int numberColumns = solver->getNumCols();
      int * which = new int[numberColumns];
      double * els = new double[numberColumns];
      int nel=0;
      for (int i=0;i<numberColumns;i++) {
	double value = objective[i];
	if (value) {
	  which[nel]=i;
	  els[nel++] = direction*value;
	}
      }
      solver->addRow(nel,which,els,-COIN_DBL_MAX,cutoff);
      delete [] which;
      delete [] els;
    }
    solver->setDblParam(OsiDualObjectiveLimit,1.0e50);
    solver->resolve();
    // Solver may not be feasible
    if (!solver->isProvenOptimal()) {
      break;
    }
    const double * lower = solver->getColLower();
    const double * upper = solver->getColUpper();
    const double * solution = solver->getColSolution();
    if (lastSolution)
      memcpy(lastSolution,solution,numberColumns*sizeof(double));
    double primalTolerance;
    solver->getDblParam(OsiPrimalTolerance,primalTolerance);
    
    
    // 2 space for last rounded solutions
#define NUMBER_OLD 4
    double ** oldSolution = new double * [NUMBER_OLD];
    for (j=0;j<NUMBER_OLD;j++) {
      oldSolution[j]= new double[numberColumns];
      for (i=0;i<numberColumns;i++) oldSolution[j][i]=-COIN_DBL_MAX;
    }
    
    // 3. Replace objective with an initial 0-valued objective
    double * saveObjective = new double [numberColumns];
    memcpy(saveObjective,solver->getObjCoefficients(),numberColumns*sizeof(double));
    for (i=0;i<numberColumns;i++) {
      solver->setObjCoeff(i,0.0);
    }
    bool finished=false;
    double direction = solver->getObjSense();
    int returnCode=0;
    bool takeHint;
    OsiHintStrength strength;
    solver->getHintParam(OsiDoDualInResolve,takeHint,strength);
    solver->setHintParam(OsiDoDualInResolve,false);
    solver->messageHandler()->setLogLevel(0);
    
    // 4. Save objective offset so we can see progress
    double saveOffset;
    solver->getDblParam(OsiObjOffset,saveOffset);
    // Get amount for original objective
    double scaleFactor = 0.0;
    for (i=0;i<numberColumns;i++)
      scaleFactor += saveObjective[i]*saveObjective[i];
    if (scaleFactor)
      scaleFactor = (initialWeight_*sqrt((double) numberIntegers))/sqrt(scaleFactor);
    // 5. MAIN WHILE LOOP
    bool newLineNeeded=false;
    while (!finished) {
      returnCode=0;
      // see what changed
      if (usedColumn) {
	for (i=0;i<numberColumns;i++) {
	  if (fabs(solution[i]-lastSolution[i])>1.0e-8) 
	    usedColumn[i]=1;
	  lastSolution[i]=solution[i];
	}
      }
      if (numberPasses>=maximumPasses_) {
	break;
      }
      if (maximumTime_>0.0&&CoinCpuTime()>=startTime_+maximumTime_) break;
      numberPasses++;
      memcpy(newSolution,solution,numberColumns*sizeof(double));
      int flip;
      returnCode = rounds(newSolution,saveObjective,numberIntegers,integerVariable,
			  roundExpensive_,downValue_,&flip);
      if (returnCode) {
	// SOLUTION IS INTEGER
	// Put back correct objective
	for (i=0;i<numberColumns;i++)
	  solver->setObjCoeff(i,saveObjective[i]);

	// solution - but may not be better
	// Compute using dot product
	solver->setDblParam(OsiObjOffset,saveOffset);
	newSolutionValue = -saveOffset;
	for (  i=0 ; i<numberColumns ; i++ )
	  newSolutionValue += saveObjective[i]*newSolution[i];
	newSolutionValue *= direction;
	if (model_->logLevel())
	  printf(" - solution found of %g",newSolutionValue);
	newLineNeeded=false;
	if (newSolutionValue<solutionValue) {
	  double saveValue = newSolutionValue;
	  if (!doGeneral) {
	    int numberLeft=0;
	    for (i=0;i<numberIntegersOrig;i++) {
	      int iColumn = integerVariableOrig[i];
	      double value = floor(newSolution[iColumn]+0.5);
	      if(solver->isBinary(iColumn)) {
		solver->setColLower(iColumn,value);
		solver->setColUpper(iColumn,value);
	      } else {
		if (fabs(value-newSolution[iColumn])>1.0e-7) 
		  numberLeft++;
	      }
	    }
	    if (numberLeft) {
	      returnCode = smallBranchAndBound(solver,200,newSolution,newSolutionValue,
					       solutionValue,"CbcHeuristicFpump");
	    }
	  }
	  if (returnCode) {
	    memcpy(betterSolution,newSolution,numberColumns*sizeof(double));
	    if ((accumulate_&1)!=0)
	      model_->incrementUsed(betterSolution); // for local search
	    solutionValue=newSolutionValue;
	    solutionFound=true;
	    if (general&&saveValue!=newSolutionValue)
	      printf(" - cleaned solution of %g\n",solutionValue);
	    else
	      printf("\n");
	  } else {
	  if (model_->logLevel()) 
	    printf(" - not good enough after small branch and bound\n");
	  }
	} else {
	  if (model_->logLevel()) 
	    printf(" - not good enough\n");
	  newLineNeeded=false;
	  returnCode=0;
	}      
	break;
      } else {
	// SOLUTION IS not INTEGER
	// 1. check for loop
	bool matched;
	for (int k = NUMBER_OLD-1; k > 0; k--) {
  	  double * b = oldSolution[k];
          matched = true;
          for (i = 0; i <numberIntegers; i++) {
	    int iColumn = integerVariable[i];
	    if (newSolution[iColumn]!=b[iColumn]) {
	      matched=false;
	      break;
	    }
	  }
	  if (matched) break;
	}
	if (matched || numberPasses%100 == 0) {
	  // perturbation
	  if (model_->logLevel())
	    printf("Perturbation applied");
	  newLineNeeded=true;
	  for (i=0;i<numberIntegers;i++) {
	    int iColumn = integerVariable[i];
	    double value = max(0.0,CoinDrand48()-0.3);
	    double difference = fabs(solution[iColumn]-newSolution[iColumn]);
	    if (difference+value>0.5) {
	      if (newSolution[iColumn]<lower[iColumn]+primalTolerance) {
		newSolution[iColumn] += 1.0;
	      } else if (newSolution[iColumn]>upper[iColumn]-primalTolerance) {
		newSolution[iColumn] -= 1.0;
	      } else {
		// general integer
		if (difference+value>0.75)
		  newSolution[iColumn] += 1.0;
		else
		  newSolution[iColumn] -= 1.0;
	      }
	    }
	  }
	} else {
	  for (j=NUMBER_OLD-1;j>0;j--) {
	    for (i = 0; i < numberColumns; i++) oldSolution[j][i]=oldSolution[j-1][i];
	  }
	  for (j = 0; j < numberColumns; j++) oldSolution[0][j] = newSolution[j];
	}
	
	// 2. update the objective function based on the new rounded solution
	double offset=0.0;
	double costValue = (1.0-scaleFactor)*solver->getObjSense();
	
	for (i=0;i<numberColumns;i++) {
	  // below so we can keep original code and allow for objective
	  int iColumn = i;
	  if(!solver->isBinary(iColumn)&&!doGeneral)
	    continue;
	  // deal with fixed variables (i.e., upper=lower)
	  if (fabs(lower[iColumn]-upper[iColumn]) < primalTolerance) {
	    //if (lower[iColumn] > 1. - primalTolerance) solver->setObjCoeff(iColumn,-costValue);
	    //else                                       solver->setObjCoeff(iColumn,costValue);
	    continue;
	  }
	  if (newSolution[iColumn]<lower[iColumn]+primalTolerance) {
	    solver->setObjCoeff(iColumn,costValue+scaleFactor*saveObjective[iColumn]);
	  } else {
	    if (newSolution[iColumn]>upper[iColumn]-primalTolerance) {
	      solver->setObjCoeff(iColumn,-costValue+scaleFactor*saveObjective[iColumn]);
	    } else {
	      solver->setObjCoeff(iColumn,0.0);
	    }
	  }
	  offset += costValue*newSolution[iColumn];
	}
	solver->setDblParam(OsiObjOffset,-offset);
	if (!general&&false) {
	  // Solve in two goes - first keep satisfied ones fixed
	  double * saveLower = new double [numberIntegers];
	  double * saveUpper = new double [numberIntegers];
	  for (i=0;i<numberIntegers;i++) {
	    int iColumn = integerVariable[i];
	    saveLower[i]=COIN_DBL_MAX;
	    saveUpper[i]=-COIN_DBL_MAX;
	    if (solution[iColumn]<lower[iColumn]+primalTolerance) {
	      saveUpper[i]=upper[iColumn];
	      solver->setColUpper(iColumn,lower[iColumn]);
	    } else if (solution[iColumn]>upper[iColumn]-primalTolerance) {
	      saveLower[i]=lower[iColumn];
	      solver->setColLower(iColumn,upper[iColumn]);
	    }
	  }
	  solver->resolve();
	  assert (solver->isProvenOptimal());
	  for (i=0;i<numberIntegers;i++) {
	    int iColumn = integerVariable[i];
	    if (saveLower[i]!=COIN_DBL_MAX)
	      solver->setColLower(iColumn,saveLower[i]);
	    if (saveUpper[i]!=-COIN_DBL_MAX)
	      solver->setColUpper(iColumn,saveUpper[i]);
	    saveUpper[i]=-COIN_DBL_MAX;
	  }
	  memcpy(newSolution,solution,numberColumns*sizeof(double));
	  int flip;
	  returnCode = rounds(newSolution,saveObjective,numberIntegers,integerVariable,
			      roundExpensive_,downValue_,&flip);
	  if (returnCode) {
	    // solution - but may not be better
	    // Compute using dot product
	    double newSolutionValue = -saveOffset;
	    for (  i=0 ; i<numberColumns ; i++ )
	      newSolutionValue += saveObjective[i]*newSolution[i];
	    newSolutionValue *= direction;
	    if (model_->logLevel())
	      printf(" - intermediate solution found of %g",newSolutionValue);
	    if (newSolutionValue<solutionValue) {
	      memcpy(betterSolution,newSolution,numberColumns*sizeof(double));
	      if ((accumulate_&1)!=0)
		model_->incrementUsed(betterSolution); // for local search
	      solutionValue=newSolutionValue;
	      solutionFound=true;
	    } else {
	      returnCode=0;
	    }
	  }      
	}
	if (!doGeneral) {
	  solver->resolve();
	  assert (solver->isProvenOptimal());
	  // in case very dubious solver
	  lower = solver->getColLower();
	  upper = solver->getColUpper();
	  solution = solver->getColSolution();
	} else {
	  int * addStart = new int[2*general+1];
	  int * addIndex = new int[4*general];
	  double * addElement = new double[4*general];
	  double * addLower = new double[2*general];
	  double * addUpper = new double[2*general];
	  double * obj = new double[general];
	  int nAdd=0;
	  for (i=0;i<numberIntegers;i++) {
	    int iColumn = integerVariable[i];
	    if (newSolution[iColumn]>lower[iColumn]+primalTolerance&&
		newSolution[iColumn]<upper[iColumn]-primalTolerance) {
	      obj[nAdd]=1.0;
	      addLower[nAdd]=0.0;
	      addUpper[nAdd]=COIN_DBL_MAX;
	      nAdd++;
	    }
	  }
	  OsiSolverInterface * solver2 = solver;
	  if (nAdd) {
	    CoinZeroN(addStart,nAdd+1);
	    solver2 = solver->clone();
	    solver2->addCols(nAdd,addStart,NULL,NULL,addLower,addUpper,obj);
	    // feasible solution
	    double * sol = new double[nAdd+numberColumns];
	    memcpy(sol,solution,numberColumns*sizeof(double));
	    // now rows
	    int nAdd=0;
	    int nEl=0;
	    int nAddRow=0;
	    for (i=0;i<numberIntegers;i++) {
	      int iColumn = integerVariable[i];
	      if (newSolution[iColumn]>lower[iColumn]+primalTolerance&&
		  newSolution[iColumn]<upper[iColumn]-primalTolerance) {
		addLower[nAddRow]=-newSolution[iColumn];;
		addUpper[nAddRow]=COIN_DBL_MAX;
		addIndex[nEl] = iColumn;
		addElement[nEl++]=-1.0;
		addIndex[nEl] = numberColumns+nAdd;
		addElement[nEl++]=1.0;
		nAddRow++;
		addStart[nAddRow]=nEl;
		addLower[nAddRow]=newSolution[iColumn];;
		addUpper[nAddRow]=COIN_DBL_MAX;
		addIndex[nEl] = iColumn;
		addElement[nEl++]=1.0;
		addIndex[nEl] = numberColumns+nAdd;
		addElement[nEl++]=1.0;
		nAddRow++;
		addStart[nAddRow]=nEl;
		sol[nAdd+numberColumns] = fabs(sol[iColumn]-newSolution[iColumn]);
		nAdd++;
	      }
	    }
	    solver2->setColSolution(sol);
	    delete [] sol;
	    solver2->addRows(nAddRow,addStart,addIndex,addElement,addLower,addUpper);
	  }
	  delete [] addStart;
	  delete [] addIndex;
	  delete [] addElement;
	  delete [] addLower;
	  delete [] addUpper;
	  delete [] obj;
	  solver2->resolve();
	  assert (solver2->isProvenOptimal());
	  if (nAdd) {
	    solver->setColSolution(solver2->getColSolution());
	    delete solver2;
	  }
	}
	if (model_->logLevel())
	  printf("\npass %3d: obj. %10.5f --> ", numberPasses+totalNumberPasses,solver->getObjValue());
	newLineNeeded=true;
	
      }
      // reduce scale factor
      scaleFactor *= weightFactor_;
    } // END WHILE
    if (newLineNeeded&&model_->logLevel())
      printf(" - no solution found\n");
    delete solver;
    for ( j=0;j<NUMBER_OLD;j++) 
      delete [] oldSolution[j];
    delete [] oldSolution;
    delete [] saveObjective;
    if (usedColumn) {
      OsiSolverInterface * newSolver = model_->continuousSolver()->clone();
      const double * colLower = newSolver->getColLower();
      const double * colUpper = newSolver->getColUpper();
      int i;
      int nFix=0;
      int nFixI=0;
      int nFixC=0;
      int nFixC2=0;
      for (i=0;i<numberIntegersOrig;i++) {
	int iColumn=integerVariableOrig[i];
	//const OsiObject * object = model_->object(i);
	//double originalLower;
	//double originalUpper;
	//getIntegerInformation( object,originalLower, originalUpper); 
	//assert(colLower[iColumn]==originalLower);
	//newSolver->setColLower(iColumn,CoinMax(colLower[iColumn],originalLower));
	newSolver->setColLower(iColumn,colLower[iColumn]);
	//assert(colUpper[iColumn]==originalUpper);
	//newSolver->setColUpper(iColumn,CoinMin(colUpper[iColumn],originalUpper));
	newSolver->setColUpper(iColumn,colUpper[iColumn]);
	if (!usedColumn[iColumn]) {
	  double value=lastSolution[iColumn];
	  double nearest = floor(value+0.5);
	  if (fabs(value-nearest)<1.0e-7) {
	    if (nearest==colLower[iColumn]) {
	      newSolver->setColUpper(iColumn,colLower[iColumn]);
	      nFix++;
	    } else if (nearest==colUpper[iColumn]) {
	      newSolver->setColLower(iColumn,colUpper[iColumn]);
	      nFix++;
	    } else if (fixInternal) {
	      newSolver->setColLower(iColumn,nearest);
	      newSolver->setColUpper(iColumn,nearest);
	      nFix++;
	      nFixI++;
	    }
	  }
	}
      }
      if (fixContinuous) {
	for (int iColumn=0;iColumn<numberColumns;iColumn++) {
	  if (!newSolver->isInteger(iColumn)&&!usedColumn[iColumn]) {
	    double value=lastSolution[iColumn];
	    if (value<colLower[iColumn]+1.0e-8) {
	      newSolver->setColUpper(iColumn,colLower[iColumn]);
	      nFixC++;
	    } else if (value>colUpper[iColumn]-1.0e-8) {
	      newSolver->setColLower(iColumn,colUpper[iColumn]);
	      nFixC++;
	    } else if (fixContinuous==2) {
	      newSolver->setColLower(iColumn,value);
	      newSolver->setColUpper(iColumn,value);
	      nFixC++;
	      nFixC2++;
	    }
	  }
	}
      }
      newSolver->initialSolve();
      if (!newSolver->isProvenOptimal()) {
	newSolver->writeMps("bad.mps");
	assert (newSolver->isProvenOptimal());
	break;
      }
      printf("%d integers at bound fixed and %d continuous",
	     nFix,nFixC);
      if (nFixC2+nFixI==0)
	printf("\n");
      else
	printf(" of which %d were internal integer and %d internal continuous\n",
	     nFixI,nFixC2);
      double saveValue = newSolutionValue;
      returnCode = smallBranchAndBound(newSolver,200,newSolution,newSolutionValue,
				       newSolutionValue,"CbcHeuristicLocalAfterFPump");
      if (returnCode) {
	printf("old sol of %g new of %g\n",saveValue,newSolutionValue);
	memcpy(betterSolution,newSolution,numberColumns*sizeof(double));
	if (fixContinuous) {
	  // may be able to do even better
	  const double * lower = model_->solver()->getColLower();
	  const double * upper = model_->solver()->getColUpper();
	  for (int iColumn=0;iColumn<numberColumns;iColumn++) {
	    if (newSolver->isInteger(iColumn)) {
	      double value=floor(newSolution[iColumn]+0.5);
	      newSolver->setColLower(iColumn,value);
	      newSolver->setColUpper(iColumn,value);
	    } else {
	      newSolver->setColLower(iColumn,lower[iColumn]);
	      newSolver->setColUpper(iColumn,upper[iColumn]);
	    }
	  }
	  newSolver->initialSolve();
	  if (newSolver->isProvenOptimal()) {
	    double value = newSolver->getObjValue()*newSolver->getObjSense();
	    if (value<newSolutionValue) {
	      printf("freeing continuous gives a solution of %g\n", value);
	      newSolutionValue=value;
	      memcpy(betterSolution,newSolver->getColSolution(),numberColumns*sizeof(double));
	    }
	  } else {
	    newSolver->writeMps("bad3.mps");
	  }
	} 
	if ((accumulate_&1)!=0)
	  model_->incrementUsed(betterSolution); // for local search
	solutionValue=newSolutionValue;
	solutionFound=true;
      }
      delete newSolver;
    }
    if (solutionFound) finalReturnCode=1;
    cutoff = CoinMin(cutoff,solutionValue);
    if (numberTries>=maximumRetries_||!solutionFound) {
      break;
    } else if (absoluteIncrement_>0.0||relativeIncrement_>0.0) {
      solutionFound=false;
      double gap = relativeIncrement_*fabs(solutionValue);
      cutoff -= CoinMax(CoinMax(gap,absoluteIncrement_),model_->getCutoffIncrement());
      printf("round again with cutoff of %g\n",cutoff);
      if ((accumulate_&3)<2&&usedColumn)
	memset(usedColumn,0,numberColumns);
      totalNumberPasses += numberPasses;
    } else {
      break;
    }
  }
  delete [] usedColumn;
  delete [] lastSolution;
  delete [] newSolution;
  delete [] integerVariable;
  return finalReturnCode;
}

/**************************END MAIN PROCEDURE ***********************************/

// update model
void CbcHeuristicFPump::setModel(CbcModel * model)
{
  model_ = model;
}

/* Rounds solution - down if < downValue
   returns 1 if current is a feasible solution
*/
int 
CbcHeuristicFPump::rounds(double * solution,
			  const double * objective,
			  int numberIntegers, const int * integerVariable,
			  bool roundExpensive, double downValue, int *flip)
{
  OsiSolverInterface * solver = model_->solver();
  double integerTolerance = model_->getDblParam(CbcModel::CbcIntegerTolerance);
  double primalTolerance ;
  solver->getDblParam(OsiPrimalTolerance,primalTolerance) ;

  int i;

  static int iter = 0;
  int numberColumns = model_->getNumCols();
  // tmp contains the current obj coefficients 
  double * tmp = new double [numberColumns];
  memcpy(tmp,solver->getObjCoefficients(),numberColumns*sizeof(double));
  int flip_up = 0;
  int flip_down  = 0;
  double  v = CoinDrand48() * 20;
  int nn = 10 + (int) v;
  int nnv = 0;
  int * list = new int [nn];
  double * val = new double [nn];
  for (i = 0; i < nn; i++) val[i] = .001;

  // return rounded solution
  for (i=0;i<numberIntegers;i++) {
    int iColumn = integerVariable[i];
    double value=solution[iColumn];
    double round = floor(value+primalTolerance);
    if (value-round > .5) round += 1.;
    if (round < integerTolerance && tmp[iColumn] < -1. + integerTolerance) flip_down++;
    if (round > 1. - integerTolerance && tmp[iColumn] > 1. - integerTolerance) flip_up++;
    if (flip_up + flip_down == 0) { 
       for (int k = 0; k < nn; k++) {
           if (fabs(value-round) > val[k]) {
              nnv++;
              for (int j = nn-2; j >= k; j--) {
                  val[j+1] = val[j];
                  list[j+1] = list[j];
              } 
              val[k] = fabs(value-round);
              list[k] = iColumn;
              break;
           }
       }
    }
    solution[iColumn] = round;
  }

  if (nnv > nn) nnv = nn;
  if (iter != 0&&model_->logLevel())
    printf("up = %5d , down = %5d", flip_up, flip_down); fflush(stdout);
  *flip = flip_up + flip_down;
  delete [] tmp;

  const double * columnLower = solver->getColLower();
  const double * columnUpper = solver->getColUpper();
  if (*flip == 0 && iter != 0) {
    if(model_->logLevel())
      printf(" -- rand = %4d (%4d) ", nnv, nn);
     for (i = 0; i < nnv; i++) {
       // was solution[list[i]] = 1. - solution[list[i]]; but does that work for 7>=x>=6
       int index = list[i];
       double value = solution[index];
       if (value<=1.0) {
	 solution[index] = 1.0-value;
       } else if (value<columnLower[index]+integerTolerance) {
	 solution[index] = value+1.0;
       } else if (value>columnUpper[index]-integerTolerance) {
	 solution[index] = value-1.0;
       } else {
	 solution[index] = value-1.0;
       }
     }
     *flip = nnv;
  } else if (model_->logLevel()) {
    printf(" ");
  }
  delete [] list; delete [] val;
  iter++;
    
  const double * rowLower = solver->getRowLower();
  const double * rowUpper = solver->getRowUpper();

  int numberRows = solver->getNumRows();
  // get row activities
  double * rowActivity = new double[numberRows];
  memset(rowActivity,0,numberRows*sizeof(double));
  solver->getMatrixByCol()->times(solution,rowActivity) ;
  double largestInfeasibility =0.0;
  for (i=0 ; i < numberRows ; i++) {
    largestInfeasibility = max(largestInfeasibility,
			       rowLower[i]-rowActivity[i]);
    largestInfeasibility = max(largestInfeasibility,
			       rowActivity[i]-rowUpper[i]);
  }
  delete [] rowActivity;
  return (largestInfeasibility>primalTolerance) ? 0 : 1;
}
// Set maximum Time (default off) - also sets starttime to current
void 
CbcHeuristicFPump::setMaximumTime(double value)
{
  startTime_=CoinCpuTime();
  maximumTime_=value;
}

  
