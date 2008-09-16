// Copyright (C) 2004, International Business Machines
// Corporation and others.  All Rights Reserved.
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
#include "CbcHeuristicFPump.hpp"
#include "CbcBranchActual.hpp"
#include "CoinHelperFunctions.hpp"
#include "CoinWarmStartBasis.hpp"
#include "CoinTime.hpp"
#include "CbcEventHandler.hpp"
#ifdef COIN_HAS_CLP
#include "OsiClpSolverInterface.hpp"
#endif


// Default Constructor
CbcHeuristicFPump::CbcHeuristicFPump() 
  :CbcHeuristic(),
   startTime_(0.0),
   maximumTime_(0.0),
   fakeCutoff_(COIN_DBL_MAX),
   absoluteIncrement_(0.0),
   relativeIncrement_(0.0),
   defaultRounding_(0.49999),
   initialWeight_(0.0),
   weightFactor_(0.1),
   artificialCost_(COIN_DBL_MAX),
   iterationRatio_(0.0),
   maximumPasses_(100),
   maximumRetries_(1),
   accumulate_(0),
   fixOnReducedCosts_(1),
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
   fakeCutoff_(COIN_DBL_MAX),
   absoluteIncrement_(0.0),
   relativeIncrement_(0.0),
   defaultRounding_(downValue),
   initialWeight_(0.0),
   weightFactor_(0.1),
   artificialCost_(COIN_DBL_MAX),
   iterationRatio_(0.0),
   maximumPasses_(100),
   maximumRetries_(1),
   accumulate_(0),
   fixOnReducedCosts_(1),
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
  CbcHeuristic::generateCpp(fp,"heuristicFPump");
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
  if (fixOnReducedCosts_!=other.fixOnReducedCosts_)
    fprintf(fp,"3  heuristicFPump.setFixOnReducedCosts(%d);\n",fixOnReducedCosts_);
  else
    fprintf(fp,"4  heuristicFPump.setFixOnReducedCosts(%d);\n",fixOnReducedCosts_);
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
  if (defaultRounding_!=other.defaultRounding_)
    fprintf(fp,"3  heuristicFPump.setDefaultRounding(%g);\n",defaultRounding_);
  else
    fprintf(fp,"4  heuristicFPump.setDefaultRounding(%g);\n",defaultRounding_);
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
  fakeCutoff_(rhs.fakeCutoff_),
  absoluteIncrement_(rhs.absoluteIncrement_),
  relativeIncrement_(rhs.relativeIncrement_),
  defaultRounding_(rhs.defaultRounding_),
  initialWeight_(rhs.initialWeight_),
  weightFactor_(rhs.weightFactor_),
  artificialCost_(rhs.artificialCost_),
  iterationRatio_(rhs.iterationRatio_),
  maximumPasses_(rhs.maximumPasses_),
  maximumRetries_(rhs.maximumRetries_),
  accumulate_(rhs.accumulate_),
  fixOnReducedCosts_(rhs.fixOnReducedCosts_),
  roundExpensive_(rhs.roundExpensive_)
{
}

// Assignment operator 
CbcHeuristicFPump & 
CbcHeuristicFPump::operator=( const CbcHeuristicFPump& rhs)
{
  if (this!=&rhs) {
    CbcHeuristic::operator=(rhs);
    startTime_ = rhs.startTime_;
    maximumTime_ = rhs.maximumTime_;
    fakeCutoff_ = rhs.fakeCutoff_;
    absoluteIncrement_ = rhs.absoluteIncrement_;
    relativeIncrement_ = rhs.relativeIncrement_;
    defaultRounding_ = rhs.defaultRounding_;
    initialWeight_ = rhs.initialWeight_;
    weightFactor_ = rhs.weightFactor_;
    artificialCost_ = rhs.artificialCost_;
    iterationRatio_ = rhs.iterationRatio_;
    maximumPasses_ = rhs.maximumPasses_;
    maximumRetries_ = rhs.maximumRetries_;
    accumulate_ = rhs.accumulate_;
    fixOnReducedCosts_ = rhs.fixOnReducedCosts_;
    roundExpensive_ = rhs.roundExpensive_;
  }
  return *this;
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
  numCouldRun_++;
  double incomingObjective = solutionValue;
#define LEN_PRINT 200
  char pumpPrint[LEN_PRINT];
  pumpPrint[0]='\0';
  if (!when()||(when()==1&&model_->phase()!=1))
    return 0; // switched off
  // See if at root node
  bool atRoot = model_->getNodeCount()==0;
  int passNumber = model_->getCurrentPassNumber();
  // just do once
  if (!atRoot||passNumber!=1)
    return 0;
  // probably a good idea
  //if (model_->getSolutionCount()) return 0;
  // loop round doing repeated pumps
  double cutoff;
  model_->solver()->getDblParam(OsiDualObjectiveLimit,cutoff);
  double direction = model_->solver()->getObjSense();
  cutoff *= direction;
  int numberBandBsolutions=0;
  double firstCutoff = fabs(cutoff);
  cutoff = CoinMin(cutoff,solutionValue);
  // check plausible and space for rounded solution
  int numberColumns = model_->getNumCols();
  int numberIntegers = model_->numberIntegers();
  const int * integerVariableOrig = model_->integerVariable();
  double iterationLimit = -1.0;
  //iterationRatio_=1.0;
  if (iterationRatio_>0.0)
    iterationLimit = (2*model_->solver()->getNumRows()+2*numberColumns)*
      iterationRatio_;
  double totalNumberIterations=0.0;
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
    j=0;
    for (i=0;i<numberIntegers;i++) {
      int iColumn = integerVariableOrig[i];
      if (upper[iColumn]-lower[iColumn]<1.000001) 
	integerVariable[j++]=iColumn;
    }
  }
  if (!general)
    doGeneral=false;
#ifdef CLP_INVESTIGATE
  if (doGeneral)
    printf("DOing general with %d out of %d\n",general,numberIntegers);
#endif
  // For solution closest to feasible if none found
  int * closestSolution = general ? NULL : new int[numberIntegers];
  double closestObjectiveValue = COIN_DBL_MAX;
  
  int numberIntegersOrig = numberIntegers;
  numberIntegers = j;
  double * newSolution = new double [numberColumns];
  double newSolutionValue=COIN_DBL_MAX;
  bool solutionFound=false;
  int * usedColumn = NULL;
  double * lastSolution=NULL;
  int fixContinuous=0;
  bool fixInternal=false;
  bool allSlack=false;
  if (when_>=21&&when_<=25) {
    when_ -= 10;
    allSlack=true;
  }
  double time1 = CoinCpuTime();
  model_->solver()->resolve();
  if (!model_->solver()->isProvenOptimal()) {
    // presumably max time or some such
    return 0;
  }
  numRuns_++;
  if (cutoff<1.0e50&&false) {
    // Fix on djs
    double direction = model_->solver()->getObjSense() ;
    double gap = cutoff - model_->solver()->getObjValue()*direction ;
    double tolerance;
    model_->solver()->getDblParam(OsiDualTolerance,tolerance) ;
    if (gap>0.0) {
      gap += 100.0*tolerance;
      int nFix=model_->solver()->reducedCostFix(gap);
      printf("dj fixing fixed %d variables\n",nFix);
    }
  }
  CoinWarmStartBasis saveBasis;
  CoinWarmStartBasis * basis =
    dynamic_cast<CoinWarmStartBasis *>(model_->solver()->getWarmStart()) ;
  if (basis) {
    saveBasis = * basis;
    delete basis;
  }
  double continuousObjectiveValue = model_->solver()->getObjValue()*model_->solver()->getObjSense();
  double * firstPerturbedObjective = NULL;
  double * firstPerturbedSolution = NULL;
  if (when_>=11&&when_<=15) {
    fixInternal = when_ >11&&when_<15;
    if (when_<13)
      fixContinuous = 0;
    else if (when_!=14)
      fixContinuous=1;
    else
      fixContinuous=2;
    when_=1;
    usedColumn = new int [numberColumns];
    for (int i=0;i<numberColumns;i++)
      usedColumn[i]=-1;
    lastSolution = CoinCopyOfArray(model_->solver()->getColSolution(),numberColumns);
  }
  int finalReturnCode=0;
  int totalNumberPasses=0;
  int numberTries=0;
  CoinWarmStartBasis bestBasis;
  bool exitAll=false;
  double saveBestObjective = model_->getMinimizationObjValue();
  int numberSolutions=0;
  OsiSolverInterface * solver = NULL;
  double artificialFactor = 0.00001;
  // also try rounding!
  double * roundingSolution = new double[numberColumns];
  double roundingObjective = solutionValue;
  CbcRounding roundingHeuristic(*model_);
  int dualPass = 0;
  int secondPassOpt=0;
  if (feasibilityPumpOptions_>0) { 
    secondPassOpt = (feasibilityPumpOptions_/10)%10;
    /* 1 to 7 - re-use solution
       8 use dual and current solution(ish)
       9 use dual and allslack
       1 - primal and mod obj
       2 - dual and mod obj
       3 - primal and no mod obj
       add 3 to redo current solution
    */
    if (secondPassOpt>=8) {
      dualPass=secondPassOpt-7;
      secondPassOpt=0;
    }
  }
  while (!exitAll) {
    int numberPasses=0;
    artificialFactor *= 10.0;
    int lastMove= (!numberTries) ? -10 : 1000000;
    double lastSumInfeas=COIN_DBL_MAX;
    numberTries++;
    // Clone solver - otherwise annoys root node computations
    solver = model_->solver()->clone();
#ifdef COIN_HAS_CLP
    {
      OsiClpSolverInterface * clpSolver 
	= dynamic_cast<OsiClpSolverInterface *> (solver);
      if (clpSolver) {
	// better to clean up using primal?
	ClpSimplex * lp = clpSolver->getModelPtr();
	int options = lp->specialOptions();
	lp->setSpecialOptions(options|8192);
	//lp->setSpecialOptions(options|0x01000000);
      }
    }
#endif
    if (CoinMin(fakeCutoff_,cutoff)<1.0e50) {
      // Fix on djs
      double direction = solver->getObjSense() ;
      double gap = CoinMin(fakeCutoff_,cutoff) - solver->getObjValue()*direction ;
      double tolerance;
      solver->getDblParam(OsiDualTolerance,tolerance) ;
      if (gap>0.0&&(fixOnReducedCosts_==1||(numberTries==1&&fixOnReducedCosts_==2))) {
	gap += 100.0*tolerance;
	int nFix=solver->reducedCostFix(gap);
	if (nFix) {
	  sprintf(pumpPrint,"Reduced cost fixing fixed %d variables on major pass %d",nFix,numberTries);
	  model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	    << pumpPrint
	    <<CoinMessageEol;
	  //pumpPrint[0]='\0';
	}
      }
    }
    // if cutoff exists then add constraint 
    bool useCutoff = (fabs(cutoff)<1.0e20&&(fakeCutoff_!=COIN_DBL_MAX||numberTries>1));
    // but there may be a close one
    if (firstCutoff<2.0*solutionValue&&numberTries==1/*&&CoinMin(cutoff,fakeCutoff_)<1.0e20*/) 
      useCutoff=true;
    if (useCutoff) {
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
      double offset;
      solver->getDblParam(OsiObjOffset,offset);
#ifdef COIN_DEVELOP
      if (offset)
	printf("CbcHeuristicFPump obj offset %g\n",offset);
#endif
      solver->addRow(nel,which,els,-COIN_DBL_MAX,cutoff+offset*direction);
      delete [] which;
      delete [] els;
      bool takeHint;
      OsiHintStrength strength;
      solver->getHintParam(OsiDoDualInResolve,takeHint,strength);
      solver->setHintParam(OsiDoDualInResolve,true,OsiHintDo);
      solver->resolve();
      solver->setHintParam(OsiDoDualInResolve,takeHint,strength);
      if (!solver->isProvenOptimal()) {
	// presumably max time or some such
	exitAll=true;
	break;
      }
    }
    solver->setDblParam(OsiDualObjectiveLimit,1.0e50);
    solver->resolve();
    // Solver may not be feasible
    if (!solver->isProvenOptimal()) {
      exitAll=true;
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
    //solver->messageHandler()->setLogLevel(0);
    
    // 4. Save objective offset so we can see progress
    double saveOffset;
    solver->getDblParam(OsiObjOffset,saveOffset);
    // Get amount for original objective
    double scaleFactor = 0.0;
#ifdef COIN_DEVELOP
    double largestCost=0.0;
    int nArtificial=0;
#endif
    for (i=0;i<numberColumns;i++) {
      double value = saveObjective[i];
      scaleFactor += value*value;
#ifdef COIN_DEVELOP
      largestCost=CoinMax(largestCost,fabs(value));
      if (value*direction>=artificialCost_)
	nArtificial++;
#endif
    }
    if (scaleFactor)
      scaleFactor = (initialWeight_*sqrt((double) numberIntegers))/sqrt(scaleFactor);
#ifdef COIN_DEVELOP
    if (scaleFactor)
      printf("Using %g fraction of original objective - largest %g - %d artificials\n",scaleFactor,
	     largestCost,nArtificial);
#endif
    // 5. MAIN WHILE LOOP
    //bool newLineNeeded=false;
    while (!finished) {
      double newTrueSolutionValue=0.0;
      double newSumInfeas=0.0;
      int newNumberInfeas=0;
      returnCode=0;
      if (model_->getCurrentSeconds()>model_->getMaximumSeconds()) {
	exitAll=true;
	break;
      }
      // see what changed
      if (usedColumn) {
	for (i=0;i<numberColumns;i++) {
	  if (fabs(solution[i]-lastSolution[i])>1.0e-8) 
	    usedColumn[i]=numberPasses;
	  lastSolution[i]=solution[i];
	}
      }
      if (iterationLimit<0.0) {
	if (numberPasses>=maximumPasses_) {
	  // If going well then keep going if maximumPasses_ small
	  if (lastMove<numberPasses-4||lastMove==1000000)
	    break;
	  if (maximumPasses_>20||numberPasses>=40)
	    break;
	}
      } else if (totalNumberIterations>iterationLimit&&numberPasses>15) {
	  // exiting on iteration count
	break;
      } else if (maximumPasses_<30&&numberPasses>100) {
	// too many passes anyway
	break;
      }
      if (maximumTime_>0.0&&CoinCpuTime()>=startTime_+maximumTime_) break;
      memcpy(newSolution,solution,numberColumns*sizeof(double));
      int flip;
      if (numberPasses==0&&false) {
	// always use same seed
	randomNumberGenerator_.setSeed(987654321);
      }
      returnCode = rounds(solver, newSolution,saveObjective,numberIntegers,integerVariable,
			  pumpPrint,numberPasses,roundExpensive_,defaultRounding_,&flip);
      if (numberPasses==0&&false) {
	// Make sure random will be different
	for (i=1;i<numberTries;i++)
	  randomNumberGenerator_.randomDouble();
      }
      numberPasses++;
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
	sprintf(pumpPrint,"Solution found of %g",newSolutionValue);
	model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	  << pumpPrint
	  <<CoinMessageEol;
	//newLineNeeded=false;
	if (newSolutionValue<solutionValue) {
	  double saveValue = solutionValue;
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
	      sprintf(pumpPrint,"Branch and bound needed to clear up %d general integers",numberLeft);
	      model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
		<< pumpPrint
		<<CoinMessageEol;
	      returnCode = smallBranchAndBound(solver,numberNodes_,newSolution,newSolutionValue,
					       solutionValue,"CbcHeuristicFpump");
	      if (returnCode<0) {
		if (returnCode==-2)
		  exitAll=true;
		returnCode=0; // returned on size or event
	      }
	      if ((returnCode&2)!=0) {
		// could add cut
		returnCode &= ~2;
	      }
	      if (returnCode!=1)
		newSolutionValue=saveValue;
	      if (returnCode&&newSolutionValue<saveValue) 
		numberBandBsolutions++;
	    }
	  }
	  if (returnCode&&newSolutionValue<saveValue) {
	    memcpy(betterSolution,newSolution,numberColumns*sizeof(double));
	    solutionFound=true;
	    CoinWarmStartBasis * basis =
	      dynamic_cast<CoinWarmStartBasis *>(solver->getWarmStart()) ;
	    if (basis) {
	      bestBasis = * basis;
	      delete basis;
	      CbcEventHandler * handler = model_->getEventHandler();
	      if (handler) {
		double * saveOldSolution = CoinCopyOfArray(model_->bestSolution(),numberColumns);
		double saveObjectiveValue = model_->getMinimizationObjValue();
		model_->setBestSolution(betterSolution,numberColumns,newSolutionValue);
		int action = handler->event(CbcEventHandler::heuristicSolution);
		if (saveOldSolution&&saveObjectiveValue<model_->getMinimizationObjValue())
		  model_->setBestSolution(saveOldSolution,numberColumns,saveObjectiveValue);
		delete [] saveOldSolution;
		if (!action||model_->getCurrentSeconds()>model_->getMaximumSeconds()) {
		  exitAll=true; // exit
		  break;
		}
	      }
	    }
	    if ((accumulate_&1)!=0) {
	      model_->incrementUsed(betterSolution); // for local search
	      numberSolutions++;
	    }
	    solutionValue=newSolutionValue;
	    solutionFound=true;
	    if (general&&saveValue!=newSolutionValue) {
	      sprintf(pumpPrint,"Cleaned solution of %g",solutionValue);
	      model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
		<< pumpPrint
		<<CoinMessageEol;
	    }
	  } else {
	    sprintf(pumpPrint,"Mini branch and bound could not fix general integers");
	    model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	      << pumpPrint
	      <<CoinMessageEol;
	  }
	} else {
	  sprintf(pumpPrint,"After further testing solution no better than previous of %g",solutionValue);
	  model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	    << pumpPrint
	    <<CoinMessageEol;
	  //newLineNeeded=false;
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
	int numberPerturbed=0;
	if (matched || numberPasses%100 == 0) {
	  // perturbation
	  //sprintf(pumpPrint+strlen(pumpPrint)," perturbation applied");
	  //newLineNeeded=true;
	  double factorX[10]={0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0};
	  double factor=1.0;
	  double target=-1.0;
	  double * randomX = new double [numberIntegers];
	  for (i=0;i<numberIntegers;i++) 
	    randomX[i] = CoinMax(0.0,randomNumberGenerator_.randomDouble()-0.3);
	  for (int k=0;k<10;k++) {
#ifdef COIN_DEVELOP_x
	    printf("kpass %d\n",k);
#endif
	    int numberX[10]={0,0,0,0,0,0,0,0,0,0};
	    for (i=0;i<numberIntegers;i++) {
	      int iColumn = integerVariable[i];
	      double value = randomX[i];
	      double difference = fabs(solution[iColumn]-newSolution[iColumn]);
	      for (int j=0;j<10;j++) {
		if (difference+value*factorX[j]>0.5) 
		  numberX[j]++;
	      }
	    }
	    if (target<0.0) {
	      if (numberX[9]<=200)
		break; // not very many changes
	      target=CoinMax(200.0,CoinMin(0.05*numberX[9],1000.0));
	    }
	    int iX=-1;
	    int iBand=-1;
	    for (i=0;i<10;i++) {
#ifdef COIN_DEVELOP_x
	      printf("** %d changed at %g\n",numberX[i],factorX[i]);
#endif
	      if (numberX[i]>=target&&numberX[i]<2.0*target&&iX<0)
		iX=i;
	      if (iBand<0&&numberX[i]>target) {
		iBand=i;
		factor=factorX[i];
	      }
	    }
	    if (iX>=0) {
	      factor=factorX[iX];
	      break;
	    } else {
	      assert (iBand>=0);
	      double hi = factor;
	      double lo = (iBand>0) ? factorX[iBand-1] : 0.0;
	      double diff = (hi-lo)/9.0;
	      for (i=0;i<10;i++) {
		factorX[i]=lo;
		lo += diff;
	      }
	    }
	  }
	  for (i=0;i<numberIntegers;i++) {
	    int iColumn = integerVariable[i];
	    double value = randomX[i];
	    double difference = fabs(solution[iColumn]-newSolution[iColumn]);
	    if (difference+value*factor>0.5) {
	      numberPerturbed++;
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
	  delete [] randomX;
	} else {
	  for (j=NUMBER_OLD-1;j>0;j--) {
	    for (i = 0; i < numberColumns; i++) oldSolution[j][i]=oldSolution[j-1][i];
	  }
	  for (j = 0; j < numberColumns; j++) oldSolution[0][j] = newSolution[j];
	}
	
	// 2. update the objective function based on the new rounded solution
	double offset=0.0;
	double costValue = (1.0-scaleFactor)*solver->getObjSense();
	int numberChanged=0;
	const double * oldObjective = solver->getObjCoefficients();
	for (i=0;i<numberColumns;i++) {
	  // below so we can keep original code and allow for objective
	  int iColumn = i;
	  // Special code for "artificials"
	  if (direction*saveObjective[iColumn]>=artificialCost_) {
	    //solver->setObjCoeff(iColumn,scaleFactor*saveObjective[iColumn]);
	    solver->setObjCoeff(iColumn,(artificialFactor*saveObjective[iColumn])/artificialCost_);
	  }
	  if(!solver->isBinary(iColumn)&&!doGeneral)
	    continue;
	  // deal with fixed variables (i.e., upper=lower)
	  if (fabs(lower[iColumn]-upper[iColumn]) < primalTolerance||!solver->isInteger(iColumn)) {
	    //if (lower[iColumn] > 1. - primalTolerance) solver->setObjCoeff(iColumn,-costValue);
	    //else                                       solver->setObjCoeff(iColumn,costValue);
	    continue;
	  }
	  double newValue=0.0;
	  if (newSolution[iColumn]<lower[iColumn]+primalTolerance) {
	    newValue = costValue+scaleFactor*saveObjective[iColumn];
	  } else {
	    if (newSolution[iColumn]>upper[iColumn]-primalTolerance) {
	      newValue = -costValue+scaleFactor*saveObjective[iColumn];
	    }
	  }
	  if (newValue!=oldObjective[iColumn])
	    numberChanged++;
	  solver->setObjCoeff(iColumn,newValue);
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
	  if (!solver->isProvenOptimal()) {
	    // presumably max time or some such
	    exitAll=true;
	    break;
	  }
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
	  returnCode = rounds(solver, newSolution,saveObjective,numberIntegers,integerVariable,
			      pumpPrint,numberPasses,roundExpensive_,defaultRounding_,&flip);
	  numberPasses++;
	  if (returnCode) {
	    // solution - but may not be better
	    // Compute using dot product
	    double newSolutionValue = -saveOffset;
	    for (  i=0 ; i<numberColumns ; i++ )
	      newSolutionValue += saveObjective[i]*newSolution[i];
	    newSolutionValue *= direction;
	    sprintf(pumpPrint,"Intermediate solution found of %g",newSolutionValue);
	    model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	      << pumpPrint
	      <<CoinMessageEol;
	    if (newSolutionValue<solutionValue) {
	      memcpy(betterSolution,newSolution,numberColumns*sizeof(double));
	      CoinWarmStartBasis * basis =
		dynamic_cast<CoinWarmStartBasis *>(solver->getWarmStart()) ;
	      solutionFound=true;
	      if (basis) {
		bestBasis = * basis;
		delete basis;
		CbcEventHandler * handler = model_->getEventHandler();
		if (handler) {
		  double * saveOldSolution = CoinCopyOfArray(model_->bestSolution(),numberColumns);
		  double saveObjectiveValue = model_->getMinimizationObjValue();
		  model_->setBestSolution(betterSolution,numberColumns,newSolutionValue);
		  int action = handler->event(CbcEventHandler::heuristicSolution);
		  if (saveOldSolution&&saveObjectiveValue<model_->getMinimizationObjValue())
		    model_->setBestSolution(saveOldSolution,numberColumns,saveObjectiveValue);
		  delete [] saveOldSolution;
		  if (!action||model_->getCurrentSeconds()>model_->getMaximumSeconds()) {
		    exitAll=true; // exit
		    break;
		  }
		}
	      }
	      if ((accumulate_&1)!=0) {
		model_->incrementUsed(betterSolution); // for local search
		numberSolutions++;
	      }
	      solutionValue=newSolutionValue;
	      solutionFound=true;
	    } else {
	      returnCode=0;
	    }
	  }      
	}
	if (!doGeneral) {
	  // faster to do from all slack!!!!
	  if (allSlack) {
	    CoinWarmStartBasis dummy;
	    solver->setWarmStart(&dummy);
	  }
#ifdef COIN_DEVELOP
	  printf("%d perturbed out of %d columns (%d changed)\n",numberPerturbed,numberColumns,numberChanged);
#endif
	  bool takeHint;
	  OsiHintStrength strength;
	  solver->getHintParam(OsiDoDualInResolve,takeHint,strength);
	  if (dualPass) {
	    solver->setHintParam(OsiDoDualInResolve,true); // dual may be better
	    if (dualPass==1) {
	      // but we need to make infeasible
	      CoinWarmStartBasis * basis =
		dynamic_cast<CoinWarmStartBasis *>(solver->getWarmStart()) ;
	      if (basis) {
		// modify
		const double * lower = solver->getColLower();
		const double * upper = solver->getColUpper();
		double * solution = CoinCopyOfArray(solver->getColSolution(),
						    numberColumns);
		const double * objective = solver->getObjCoefficients();
		int nChanged=0;
		for (i=0;i<numberIntegersOrig;i++) {
		  int iColumn=integerVariableOrig[i];
		  if (objective[iColumn]>0.0) {
		    if (basis->getStructStatus(iColumn)==
			CoinWarmStartBasis::atUpperBound) {
		      solution[iColumn]=lower[iColumn];
		      basis->setStructStatus(iColumn,CoinWarmStartBasis::atLowerBound);
		      nChanged++;
		    }
		  } else if (objective[iColumn]<0.0) {
		    if (basis->getStructStatus(iColumn)==
			CoinWarmStartBasis::atUpperBound) {
		      solution[iColumn]=upper[iColumn];
		      basis->setStructStatus(iColumn,CoinWarmStartBasis::atUpperBound);
		      nChanged++;
		    }
		  }
		}
		if (!nChanged) {
		  for (i=0;i<numberIntegersOrig;i++) {
		    int iColumn=integerVariableOrig[i];
		    if (objective[iColumn]>0.0) {
		      if (basis->getStructStatus(iColumn)==
			  CoinWarmStartBasis::basic) {
			solution[iColumn]=lower[iColumn];
			basis->setStructStatus(iColumn,CoinWarmStartBasis::atLowerBound);
			break;
		      }
		    } else if (objective[iColumn]<0.0) {
		      if (basis->getStructStatus(iColumn)==
			  CoinWarmStartBasis::basic) {
			solution[iColumn]=upper[iColumn];
			basis->setStructStatus(iColumn,CoinWarmStartBasis::atUpperBound);
			break;
		      }
		    }
		  }
		}
		solver->setColSolution(solution);
		delete [] solution;
		solver->setWarmStart(basis);
		delete basis;
	      }
	    } else {
	      // faster to do from all slack!!!! ???
	      CoinWarmStartBasis dummy;
	      solver->setWarmStart(&dummy);
	    }
	  }
	  if (numberTries>1&&numberPasses==1&&firstPerturbedObjective) {
	    // Modify to use convex combination
	    // use basis from first time
	    solver->setWarmStart(&saveBasis);
	    // and objective
	    if (secondPassOpt<3||(secondPassOpt>=4&&secondPassOpt<6))
	      solver->setObjective(firstPerturbedObjective);
	    // and solution
	    solver->setColSolution(firstPerturbedSolution);
	    if (secondPassOpt==2||secondPassOpt==5)
	      solver->setHintParam(OsiDoDualInResolve,true); // dual may be better
	  }
	  solver->resolve();
	  if (!solver->isProvenOptimal()) {
	    // presumably max time or some such
	    exitAll=true;
	    break;
	  }
	  if (numberPasses==1&&secondPassOpt) {
	    if (numberTries==1||secondPassOpt>3) {
	      // save basis
	      CoinWarmStartBasis * basis =
		dynamic_cast<CoinWarmStartBasis *>(solver->getWarmStart()) ;
	      if (basis) {
		saveBasis = * basis;
		delete basis;
	      }
	      firstPerturbedObjective = CoinCopyOfArray(solver->getObjCoefficients(),numberColumns);
	      firstPerturbedSolution = CoinCopyOfArray(solver->getColSolution(),numberColumns);
	    }
	  }
	  solver->setHintParam(OsiDoDualInResolve,takeHint);
	  newTrueSolutionValue = -saveOffset;
	  newSumInfeas=0.0;
	  newNumberInfeas=0;
	  {
	    const double * newSolution = solver->getColSolution();
	    for (  i=0 ; i<numberColumns ; i++ ) {
	      if (solver->isInteger(i)) {
		double value = newSolution[i];
		double nearest = floor(value+0.5);
		newSumInfeas += fabs(value-nearest);
		if (fabs(value-nearest)>1.0e-6)
		  newNumberInfeas++;
	      }
	      newTrueSolutionValue += saveObjective[i]*newSolution[i];
	    }
	    newTrueSolutionValue *= direction;
	    if (newNumberInfeas&&newNumberInfeas<-20) {
#if 0
	      roundingObjective=solutionValue;
	      OsiSolverInterface * saveSolver = model_->swapSolver(solver);
	      int ifSol = roundingHeuristic.solution(roundingObjective,roundingSolution);
	      model_->swapSolver(saveSolver);
	      if (ifSol>0) 
		abort();
#endif
	      int numberRows=solver->getNumRows();
	      double * rowActivity = new double[numberRows];
	      memset(rowActivity,0,numberRows*sizeof(double));
	      int * which = new int[newNumberInfeas];
	      int * stack = new int[newNumberInfeas+1];
	      double * baseValue = new double[newNumberInfeas];
	      int * whichRow = new int[numberRows];
	      double * rowValue = new double[numberRows];
	      memset(rowValue,0,numberRows*sizeof(double));
	      int nRow=0;
	      // Column copy
	      const double * element = solver->getMatrixByCol()->getElements();
	      const int * row = solver->getMatrixByCol()->getIndices();
	      const CoinBigIndex * columnStart = solver->getMatrixByCol()->getVectorStarts();
	      const int * columnLength = solver->getMatrixByCol()->getVectorLengths();
	      int n=0;
	      double contrib = 0.0;
	      for (  i=0 ; i<numberColumns ; i++ ) {
		double value = newSolution[i];
		if (solver->isInteger(i)) {
		  double nearest = floor(value+0.5);
		  if (fabs(value-nearest)>1.0e-6) {
		    //printf("Column %d value %g\n",i,value);
		    for (CoinBigIndex j=columnStart[i];
			 j<columnStart[i]+columnLength[i];j++) {
		      int iRow=row[j];
		      //printf("row %d element %g\n",iRow,element[j]);
		      if (!rowValue[iRow]) {
			rowValue[iRow]=1.0;
			whichRow[nRow++]=iRow;
		      }
		    }
		    baseValue[n]=floor(value);
		    contrib += saveObjective[i]*value;
		    value=0.0;
		    stack[n]=0;
		    which[n++]=i;
		  }
		}
		for (CoinBigIndex j=columnStart[i];
		     j<columnStart[i]+columnLength[i];j++) {
		  int iRow=row[j];
		  rowActivity[iRow] += value*element[j];
		}
	      }
	      if (newNumberInfeas<15) {
		stack[n]=newNumberInfeas+100;
		int iStack=n;
		memset(rowValue,0,numberRows*sizeof(double));
		const double * rowLower = solver->getRowLower();
		const double * rowUpper = solver->getRowUpper();
		while (iStack>=0) {
		  double contrib2=0.0;
		  // Could do faster
		  for (int k=0 ; k<n ; k++ ) {
		    i=which[k];
		    double value = baseValue[k]+stack[k];
		    contrib2 += saveObjective[i]*value;
		    for (CoinBigIndex j=columnStart[i];
			 j<columnStart[i]+columnLength[i];j++) {
		      int iRow=row[j];
		      rowValue[iRow] += value*element[j];
		    }
		  }
		  // check if feasible
		  bool feasible=true;
		  for (int k=0;k<nRow;k++) {
		    i=whichRow[k];
		    double value = rowValue[i]+rowActivity[i];
		    rowValue[i]=0.0;
		    if(value<rowLower[i]-1.0e-7||
		       value>rowUpper[i]+1.0e-7)
		      feasible=false;
		  }
		  if (feasible) {
		    double newObj = newTrueSolutionValue * direction;
		    newObj += contrib2-contrib;
		    newObj *= direction;
		    printf("FFFeasible! - obj %g\n",newObj);
		    if (newObj<roundingObjective) {
		      printf("FBetter\n");
		      roundingObjective = newObj;
		      memcpy(roundingSolution,newSolution,numberColumns*sizeof(double));
		      for (int k=0 ; k<n ; k++ ) {
			i=which[k];
			double value = baseValue[k]+stack[k];
			roundingSolution[i]=value;
		      }
		    }
		  }
		  while (iStack>=0&&stack[iStack]) {
		    stack[iStack]--;
		    iStack--;
		  }
		  if (iStack>=0) {
		    stack[iStack]=1;
		    iStack=n;
		    stack[n]=1;
		  }
		}
	      }
	      delete [] rowActivity;
	      delete [] which;
	      delete [] stack;
	      delete [] baseValue;
	      delete [] whichRow;
	      delete [] rowValue;
	    }
	  }
	  if (false) {
	    OsiSolverInterface * saveSolver = model_->swapSolver(solver);
	    CbcRounding heuristic1(*model_);
	    heuristic1.setHeuristicName("rounding in feaspump!");
	    heuristic1.setWhen(1);
	    roundingObjective = newTrueSolutionValue;
	    double testObjectiveValue = CoinMin(solutionValue,roundingObjective);
	    int returnCode = heuristic1.solution(testObjectiveValue,roundingSolution,newTrueSolutionValue) ;
	    if (returnCode==1) {
	      assert(testObjectiveValue < CoinMin(solutionValue,roundingObjective));
	      roundingObjective = testObjectiveValue;
	    } else {
	      roundingObjective = COIN_DBL_MAX;
	    }
	    model_->swapSolver(saveSolver);
	  }
	  if (!solver->isProvenOptimal()) {
	    // presumably max time or some such
	    exitAll=true;
	    break;
	  }
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
	      assert (upper[iColumn]-lower[iColumn]>1.00001);
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
	  if (!solver2->isProvenOptimal()) {
	    // presumably max time or some such
	    exitAll=true;
	    break;
	  }
	  //assert (solver2->isProvenOptimal());
	  if (nAdd) {
	    solver->setColSolution(solver2->getColSolution());
	    delete solver2;
	  }
	  lower = solver->getColLower();
	  upper = solver->getColUpper();
	  solution = solver->getColSolution();
	  newTrueSolutionValue = -saveOffset;
	  newSumInfeas=0.0;
	  newNumberInfeas=0;
	  {
	    const double * newSolution = solver->getColSolution();
	    for (  i=0 ; i<numberColumns ; i++ ) {
	      if (solver->isInteger(i)) {
		double value = newSolution[i];
		double nearest = floor(value+0.5);
		newSumInfeas += fabs(value-nearest);
		if (fabs(value-nearest)>1.0e-6)
		  newNumberInfeas++;
	      }
	      newTrueSolutionValue += saveObjective[i]*newSolution[i];
	    }
	    newTrueSolutionValue *= direction;
	  }
	}
	if (lastMove!=1000000) {
	  if (newSumInfeas<lastSumInfeas) {
	    lastMove=numberPasses;
	    lastSumInfeas=newSumInfeas;
	  } else if (newSumInfeas>lastSumInfeas+1.0e-5) {
	    lastMove=1000000; // going up
	  }
	}
	totalNumberIterations += solver->getIterationCount();
	if (solver->getNumRows()<3000)
	  sprintf(pumpPrint,"Pass %3d: suminf. %10.5f (%d) obj. %g iterations %d", 
		  numberPasses+totalNumberPasses,
		  newSumInfeas,newNumberInfeas,
		  newTrueSolutionValue,solver->getIterationCount());
	else
	  sprintf(pumpPrint,"Pass %3d: (%.2f seconds) suminf. %10.5f (%d) obj. %g iterations %d", numberPasses+totalNumberPasses,
		  model_->getCurrentSeconds(),newSumInfeas,newNumberInfeas,
		  newTrueSolutionValue,solver->getIterationCount());
	model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	  << pumpPrint
	  <<CoinMessageEol;
	if (closestSolution&&solver->getObjValue()<closestObjectiveValue) {
	  int i;
	  const double * objective = solver->getObjCoefficients();
	  for (i=0;i<numberIntegersOrig;i++) {
	    int iColumn=integerVariableOrig[i];
	    if (objective[iColumn]>0.0)
	      closestSolution[i]=0;
	    else
	      closestSolution[i]=1;
	  }
	  closestObjectiveValue = solver->getObjValue();
	}
	//newLineNeeded=true;
	
      }
      // reduce scale factor
      scaleFactor *= weightFactor_;
    } // END WHILE
    // see if rounding worked!
    if (roundingObjective<solutionValue) {
      sprintf(pumpPrint,"Rounding solution of %g is better than previous of %g !\n",
	      roundingObjective,solutionValue);
      model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	<< pumpPrint
	<<CoinMessageEol;
      solutionValue=roundingObjective;
      memcpy(betterSolution,roundingSolution,numberColumns*sizeof(double));
      solutionFound=true;
    }
    if (!solutionFound) { 
      sprintf(pumpPrint,"No solution found this major pass");
      model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	<< pumpPrint
	<<CoinMessageEol;
    }
    //}
    delete solver;
    solver=NULL;
    for ( j=0;j<NUMBER_OLD;j++) 
      delete [] oldSolution[j];
    delete [] oldSolution;
    delete [] saveObjective;
    if (usedColumn&&!exitAll) {
      OsiSolverInterface * newSolver = model_->continuousSolver()->clone();
      const double * colLower = newSolver->getColLower();
      const double * colUpper = newSolver->getColUpper();
      bool stopBAB=false;
      int allowedPass=-1;
      while (!stopBAB) {
	stopBAB=true;
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
	  if (usedColumn[iColumn]<=allowedPass) {
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
	    if (!newSolver->isInteger(iColumn)&&usedColumn[iColumn]<=allowedPass) {
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
	  //newSolver->writeMps("bad.mps");
	  //assert (newSolver->isProvenOptimal());
	  exitAll=true;
	  break;
	}
	sprintf(pumpPrint,"Before mini branch and bound, %d integers at bound fixed and %d continuous",
		nFix,nFixC);
	if (nFixC2+nFixI!=0)
	  sprintf(pumpPrint+strlen(pumpPrint)," of which %d were internal integer and %d internal continuous",
		  nFixI,nFixC2);
	model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	  << pumpPrint
	  <<CoinMessageEol;
	double saveValue = newSolutionValue;
	if (newSolutionValue-model_->getCutoffIncrement()
	    >continuousObjectiveValue-1.0e-7) {
	  double saveFraction = fractionSmall_;
	  if (numberTries>1&&!numberBandBsolutions)
	    fractionSmall_ *= 0.5;
	  // Give branch and bound a bit more freedom
	  double cutoff2=newSolutionValue+
	    CoinMax(model_->getCutoffIncrement(),1.0e-3);
	  int returnCode2 = smallBranchAndBound(newSolver,numberNodes_,newSolution,newSolutionValue,
						cutoff2,"CbcHeuristicLocalAfterFPump");
	  fractionSmall_ = saveFraction;
	  if (returnCode2<0) {
	    if (returnCode2==-2) {
	      exitAll=true;
	      returnCode=0;
	    } else {
	      returnCode2=0; // returned on size - try changing
	      //#define ROUND_AGAIN
#ifdef ROUND_AGAIN
	      if (numberTries==1&&numberPasses>20&&allowedPass<numberPasses-1) {
		allowedPass=(numberPasses+allowedPass)>>1;
		sprintf(pumpPrint,
			"Fixing all variables which were last changed on pass %d and trying again",
		       allowedPass);
		model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
		  << pumpPrint
		  <<CoinMessageEol;
		stopBAB=false;
		continue;
	      }
#endif
	    }
	  }
	  if ((returnCode2&2)!=0) {
	    // could add cut
	    returnCode2 &= ~2;
	  }
	  if (returnCode2)
	    numberBandBsolutions++;
	} else {
	  // no need
	  exitAll=true;
	  //returnCode=0;
	}
	// recompute solution value
	if (returnCode&&true) {
	  delete newSolver;
	  newSolver = model_->continuousSolver()->clone();
	  newSolutionValue = -saveOffset;
	  double newSumInfeas=0.0;
	  const double * obj = newSolver->getObjCoefficients();
	  for (int i=0 ; i<numberColumns ; i++ ) {
	    if (newSolver->isInteger(i)) {
	      double value = newSolution[i];
	      double nearest = floor(value+0.5);
	      newSumInfeas += fabs(value-nearest);
	    }
	    newSolutionValue += obj[i]*newSolution[i];
	  }
	  newSolutionValue *= direction;
	}
	bool gotSolution = false;
	if (returnCode&&newSolutionValue<saveValue) {
	  sprintf(pumpPrint,"Mini branch and bound improved solution from %g to %g (%.2f seconds)",
		  saveValue,newSolutionValue,model_->getCurrentSeconds());
	  model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	    << pumpPrint
	    <<CoinMessageEol;
	  memcpy(betterSolution,newSolution,numberColumns*sizeof(double));
	  gotSolution=true;
	  if (fixContinuous&&nFixC+nFixC2>0) {
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
		//newSolver->writeMps("query","mps");
#if 0
		{
		  double saveOffset;
		  newSolver->getDblParam(OsiObjOffset,saveOffset);
		  const double * obj = newSolver->getObjCoefficients();
		  double newTrueSolutionValue = -saveOffset;
		  double newSumInfeas=0.0;
		  int numberColumns = newSolver->getNumCols();
		  const double * solution = newSolver->getColSolution();
		  for (int  i=0 ; i<numberColumns ; i++ ) {
		    if (newSolver->isInteger(i)) {
		      double value = solution[i];
		      double nearest = floor(value+0.5);
		      newSumInfeas += fabs(value-nearest);
		    }
		    if (solution[i])
		      printf("%d obj %g val %g - total %g\n",i,obj[i],solution[i],
			     newTrueSolutionValue);
		    newTrueSolutionValue += obj[i]*solution[i];
		  }
		  printf("obj %g - inf %g\n",newTrueSolutionValue,
			 newSumInfeas);
		}
#endif
		sprintf(pumpPrint,"Freeing continuous variables gives a solution of %g", value);
		model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
		  << pumpPrint
		  <<CoinMessageEol;
		newSolutionValue=value;
		memcpy(betterSolution,newSolver->getColSolution(),numberColumns*sizeof(double));
	      }
	    } else {
	      //newSolver->writeMps("bad3.mps");
	      exitAll=true;
	      break;
	    }
	  } 
	} else {
	  sprintf(pumpPrint,"Mini branch and bound did not improve solution (%.2f seconds)",
		  model_->getCurrentSeconds());
	  model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	    << pumpPrint
	    <<CoinMessageEol;
	  if (returnCode&&newSolutionValue<saveValue+1.0e-3&&nFixC+nFixC2) {
	    // may be able to do better
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
	      if (value<saveValue) {
		sprintf(pumpPrint,"Freeing continuous variables gives a solution of %g", value);
		model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
		  << pumpPrint
		  <<CoinMessageEol;
		newSolutionValue=value;
		memcpy(betterSolution,newSolver->getColSolution(),numberColumns*sizeof(double));
		gotSolution=true;
	      }
	    }
	  }
	}
	if (gotSolution) {
	  if ((accumulate_&1)!=0) {
	    model_->incrementUsed(betterSolution); // for local search
	    numberSolutions++;
	  }
	  solutionValue=newSolutionValue;
	  solutionFound=true;
	  CoinWarmStartBasis * basis =
	    dynamic_cast<CoinWarmStartBasis *>(newSolver->getWarmStart()) ;
	  if (basis) {
	    bestBasis = * basis;
	    delete basis;
	    CbcEventHandler * handler = model_->getEventHandler();
	    if (handler) {
	      double * saveOldSolution = CoinCopyOfArray(model_->bestSolution(),numberColumns);
	      double saveObjectiveValue = model_->getMinimizationObjValue();
	      model_->setBestSolution(betterSolution,numberColumns,newSolutionValue);
	      int action = handler->event(CbcEventHandler::heuristicSolution);
	      //printf("cutoff %g\n",model_->getCutoff());
	      if (saveOldSolution&&saveObjectiveValue<model_->getMinimizationObjValue())
		model_->setBestSolution(saveOldSolution,numberColumns,saveObjectiveValue);
	      delete [] saveOldSolution;
	      if (!action||model_->getCurrentSeconds()>model_->getMaximumSeconds()) {
		exitAll=true; // exit
		break;
	      }
	    }
	  }
	}
      } // end stopBAB while
      delete newSolver;
    }
    if (solutionFound) finalReturnCode=1;
    cutoff = CoinMin(cutoff,solutionValue-model_->getCutoffIncrement());
    if (numberTries>=maximumRetries_||!solutionFound||exitAll||cutoff<continuousObjectiveValue+1.0e-7) {
      break;
    } else {
      solutionFound=false;
      if (absoluteIncrement_>0.0||relativeIncrement_>0.0) {
	double gap = relativeIncrement_*fabs(solutionValue);
	cutoff -= CoinMax(CoinMax(gap,absoluteIncrement_),model_->getCutoffIncrement());
      } else {
	//double weights[10]={0.1,0.1,0.2,0.2,0.2,0.3,0.3,0.3,0.4,0.5};
	double weights[10]={0.1,0.2,0.3,0.3,0.4,0.4,0.4,0.5,0.5,0.6};
	cutoff -= weights[CoinMin(numberTries-1,9)]*(cutoff-continuousObjectiveValue);
      }
      // But round down
      if (model_->getCutoffIncrement()==0.999||
	  model_->getCutoffIncrement()==1.0)
	cutoff = floor(cutoff);
      if (cutoff<continuousObjectiveValue)
	break;
      sprintf(pumpPrint,"Round again with cutoff of %g",cutoff);
      model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
	<< pumpPrint
	<<CoinMessageEol;
      if ((accumulate_&3)<2&&usedColumn) {
	for (int i=0;i<numberColumns;i++)
	  usedColumn[i]=-1;
      }
      totalNumberPasses += numberPasses-1;
    }
  }
  delete solver; // probably NULL but do anyway
  if (!finalReturnCode&&closestSolution&&closestObjectiveValue <= 10.0&&usedColumn) {
    // try a bit of branch and bound
    OsiSolverInterface * newSolver = model_->continuousSolver()->clone();
    const double * colLower = newSolver->getColLower();
    const double * colUpper = newSolver->getColUpper();
    int i;
    double rhs = 0.0;
    for (i=0;i<numberIntegersOrig;i++) {
      int iColumn=integerVariableOrig[i];
      int direction = closestSolution[i];
      closestSolution[i]=iColumn;
      if (direction==0) {
	// keep close to LB
	rhs += colLower[iColumn];
	lastSolution[i]=1.0;
      } else {
	// keep close to UB
	rhs -= colUpper[iColumn];
	lastSolution[i]=-1.0;
      }
    }
    newSolver->addRow(numberIntegersOrig,closestSolution,
		      lastSolution,-COIN_DBL_MAX,rhs+10.0);
    //double saveValue = newSolutionValue;
    //newSolver->writeMps("sub");
    int returnCode = smallBranchAndBound(newSolver,numberNodes_,newSolution,newSolutionValue,
				     newSolutionValue,"CbcHeuristicLocalAfterFPump");
    if (returnCode<0)
      returnCode=0; // returned on size
    if ((returnCode&2)!=0) {
      // could add cut
      returnCode &= ~2;
    }
    if (returnCode) {
      //printf("old sol of %g new of %g\n",saveValue,newSolutionValue);
      memcpy(betterSolution,newSolution,numberColumns*sizeof(double));
      //abort();
      solutionValue=newSolutionValue;
      solutionFound=true;
    }
    delete newSolver;
  }
  delete [] roundingSolution;
  delete [] usedColumn;
  delete [] lastSolution;
  delete [] newSolution;
  delete [] closestSolution;
  delete [] integerVariable;
  delete [] firstPerturbedObjective;
  delete [] firstPerturbedSolution;
  if (solutionValue==incomingObjective) 
    sprintf(pumpPrint,"After %.2f seconds - Feasibility pump exiting - took %.2f seconds",
	    model_->getCurrentSeconds(),CoinCpuTime()-time1);
  else
    sprintf(pumpPrint,"After %.2f seconds - Feasibility pump exiting with objective of %g - took %.2f seconds",
	    model_->getCurrentSeconds(),solutionValue,CoinCpuTime()-time1);
  model_->messageHandler()->message(CBC_FPUMP1,model_->messages())
    << pumpPrint
    <<CoinMessageEol;
  if (bestBasis.getNumStructural())
    model_->setBestSolutionBasis(bestBasis);
  model_->setMinimizationObjValue(saveBestObjective);
  if ((accumulate_&1)!=0&&numberSolutions>1&&!model_->getSolutionCount()) {
    model_->setSolutionCount(1); // for local search
    model_->setNumberHeuristicSolutions(1); 
  }
#ifdef COIN_DEVELOP
  {
    double ncol = model_->solver()->getNumCols();
    double nrow = model_->solver()->getNumRows();
    printf("XXX total iterations %g ratios - %g %g %g\n",
	   totalNumberIterations,
	   totalNumberIterations/nrow,
	   totalNumberIterations/ncol,
	   totalNumberIterations/(2*nrow+2*ncol));
  }
#endif
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
CbcHeuristicFPump::rounds(OsiSolverInterface * solver,double * solution,
			  const double * objective,
			  int numberIntegers, const int * integerVariable,
			  char * pumpPrint, int iter,
			  bool roundExpensive, double downValue, int *flip)
{
  double integerTolerance = model_->getDblParam(CbcModel::CbcIntegerTolerance);
  double primalTolerance ;
  solver->getDblParam(OsiPrimalTolerance,primalTolerance) ;

  int i;

  const double * cost = solver->getObjCoefficients();
  int flip_up = 0;
  int flip_down  = 0;
  double  v = randomNumberGenerator_.randomDouble() * 20.0;
  int nn = 10 + (int) v;
  int nnv = 0;
  int * list = new int [nn];
  double * val = new double [nn];
  for (i = 0; i < nn; i++) val[i] = .001;

  const double * rowLower = solver->getRowLower();
  const double * rowUpper = solver->getRowUpper();
  int numberRows = solver->getNumRows();
  if ((iter&1)!=0) {
    // Do set covering variables
    const CoinPackedMatrix * matrixByRow = solver->getMatrixByRow();
    const double * elementByRow = matrixByRow->getElements();
    const int * column = matrixByRow->getIndices();
    const CoinBigIndex * rowStart = matrixByRow->getVectorStarts();
    const int * rowLength = matrixByRow->getVectorLengths();
    for (i=0;i<numberRows;i++) {
      if (rowLower[i]==1.0&&rowUpper[i]==1.0) {
	bool cover=true;
	double largest=0.0;
	int jColumn=-1;
	for (CoinBigIndex k=rowStart[i];k<rowStart[i]+rowLength[i];k++) {
	  int iColumn = column[k];
	  if (elementByRow[k]!=1.0||!solver->isInteger(iColumn)) {
	    cover=false;
	    break;
	  } else {
	    if (solution[iColumn]) {
	      double value = solution[iColumn]*
		(randomNumberGenerator_.randomDouble()+5.0);
	      if (value>largest) {
		largest=value;
		jColumn=iColumn;
	      }
	    }
	  }
	}
	if (cover) {
	  for (CoinBigIndex k=rowStart[i];k<rowStart[i]+rowLength[i];k++) {
	    int iColumn = column[k];
	    if (iColumn==jColumn) 
	      solution[iColumn]=1.0;
	    else 
	      solution[iColumn]=0.0;
	  }
	}
      }
    }
  }
#if 0
  // Do set covering variables
  const CoinPackedMatrix * matrixByRow = solver->getMatrixByRow();
  const double * elementByRow = matrixByRow->getElements();
  const int * column = matrixByRow->getIndices();
  const CoinBigIndex * rowStart = matrixByRow->getVectorStarts();
  const int * rowLength = matrixByRow->getVectorLengths();
  int numberColumns = solver->getNumCols();
  double * sortTemp = new double[numberColumns];
  int * whichTemp = new int [numberColumns];
  char * rowTemp = new char [numberRows];
  memset(rowTemp,0,numberRows);
  for (i=0;i<numberColumns;i++)
    whichTemp[i]=-1;
  int nSOS=0;
  for (i=0;i<numberRows;i++) {
    if (rowLower[i]==1.0&&rowUpper[i]==1.0) {
      bool cover=true;
      for (CoinBigIndex k=rowStart[i];k<rowStart[i]+rowLength[i];k++) {
	int iColumn = column[k];
	if (elementByRow[k]!=1.0||!solver->isInteger(iColumn)) {
	  cover=false;
	  break;
	}
      }
      if (cover) {
	rowTemp[i]=1;
	nSOS++;
	for (CoinBigIndex k=rowStart[i];k<rowStart[i]+rowLength[i];k++) {
	  int iColumn = column[k];
	  double value=solution[iColumn];
	  whichTemp[iColumn]=iColumn;
	}
      }
    }
  }
  if (nSOS) {
    // Column copy
    const CoinPackedMatrix * matrixByColumn = solver->getMatrixByCol();
    //const double * element = matrixByColumn->getElements();
    const int * row = matrixByColumn->getIndices();
    const CoinBigIndex * columnStart = matrixByColumn->getVectorStarts();
    const int * columnLength = matrixByColumn->getVectorLengths();
    int nLook = 0;
    for (i=0;i<numberColumns;i++) {
      if (whichTemp[i]>=0) {
	whichTemp[nLook]=i;
	double value=solution[i];
	if (value<0.5)
	  value *= (0.1*randomNumberGenerator_.randomDouble()+0.3);
	sortTemp[nLook++]=-value;
      }
    }
    CoinSort_2(sortTemp,sortTemp+nLook,whichTemp);
    double smallest=1.0;
    int nFix=0;
    int nOne=0;
    for (int j=0;j<nLook;j++) {
      int jColumn = whichTemp[j];
      double thisValue = solution[jColumn];
      if (!thisValue)
	continue;
      if (thisValue==1.0) 
	nOne++;
      smallest = CoinMin(smallest,thisValue);
      solution[jColumn]=1.0;
      double largest=0.0;
      for (CoinBigIndex jEl=columnStart[jColumn];
	   jEl<columnStart[jColumn]+columnLength[jColumn];jEl++) {
	int jRow = row[jEl];
	if (rowTemp[jRow]) {
	  for (CoinBigIndex k=rowStart[jRow];k<rowStart[jRow]+rowLength[jRow];k++) {
	    int iColumn = column[k];
	    if (solution[iColumn]) {
	      if (iColumn!=jColumn) {
		double value = solution[iColumn];
		if (value>largest) 
		  largest=value;
		solution[iColumn]=0.0;
	      }
	    }
	  }
	}
      }
      if (largest>thisValue)
	printf("%d was at %g - chosen over a value of %g\n",
	       jColumn,thisValue,largest);
      nFix++;
    }
    printf("%d fixed out of %d (%d at one already)\n",
	   nFix,nLook,nOne);
  }
  delete [] sortTemp;
  delete [] whichTemp;
  delete [] rowTemp;
#endif
  const double * columnLower = solver->getColLower();
  const double * columnUpper = solver->getColUpper();
  // Check if valid with current solution (allow for 0.99999999s)
  for (i=0;i<numberIntegers;i++) {
    int iColumn = integerVariable[i];
    double value=solution[iColumn];
    double round = floor(value+0.5);
    if (fabs(value-round)>primalTolerance) 
      break;
  }
  if (i==numberIntegers) {
    // may be able to use solution even if 0.99999's
    // get row activities
    double * rowActivity = new double[numberRows];
    memset(rowActivity,0,numberRows*sizeof(double));
    solver->getMatrixByCol()->times(solution,rowActivity) ;
    double largestInfeasibility =0.0;
    for (i=0 ; i < numberRows ; i++) {
      largestInfeasibility = CoinMax(largestInfeasibility,
				 rowLower[i]-rowActivity[i]);
      largestInfeasibility = CoinMax(largestInfeasibility,
				 rowActivity[i]-rowUpper[i]);
    }
    delete [] rowActivity;
    if (largestInfeasibility<=primalTolerance) {
      // feasible
      delete [] list; 
      delete [] val;
      return 1;
    }
  }
      
  // return rounded solution
  for (i=0;i<numberIntegers;i++) {
    int iColumn = integerVariable[i];
    double value=solution[iColumn];
    double round = floor(value+primalTolerance);
    if (value-round > downValue) round += 1.;
#if 1
    if (round < integerTolerance && cost[iColumn] < -1. + integerTolerance) flip_down++;
    if (round > 1. - integerTolerance && cost[iColumn] > 1. - integerTolerance) flip_up++;
#else
    if (round < columnLower[iColumn]+integerTolerance && cost[iColumn] < -1. + integerTolerance) flip_down++;
    if (round > columnUpper[iColumn] - integerTolerance && cost[iColumn] > 1. - integerTolerance) flip_up++;
#endif
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
  //if (iter != 0)
  //sprintf(pumpPrint+strlen(pumpPrint),"up = %5d , down = %5d", flip_up, flip_down);
  *flip = flip_up + flip_down;

  if (*flip == 0 && iter != 0) {
    //sprintf(pumpPrint+strlen(pumpPrint)," -- rand = %4d (%4d) ", nnv, nn);
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
  } else {
    //sprintf(pumpPrint+strlen(pumpPrint)," ");
  }
  delete [] list; delete [] val;
  //iter++;
    
  // get row activities
  double * rowActivity = new double[numberRows];
  memset(rowActivity,0,numberRows*sizeof(double));
  solver->getMatrixByCol()->times(solution,rowActivity) ;
  double largestInfeasibility =0.0;
  for (i=0 ; i < numberRows ; i++) {
    largestInfeasibility = CoinMax(largestInfeasibility,
			       rowLower[i]-rowActivity[i]);
    largestInfeasibility = CoinMax(largestInfeasibility,
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

  
