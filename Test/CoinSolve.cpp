// Copyright (C) 2004, International Business Machines
// Corporation and others.  All Rights Reserved.
   
#include "CoinPragma.hpp"

#include <cassert>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <string>
#include <iostream>


#include "CoinPragma.hpp"
#include "CoinHelperFunctions.hpp"
// Same version as CBC
#define CBCVERSION "0.70"

#include "CoinMpsIO.hpp"

#include "ClpFactorization.hpp"
#include "CoinTime.hpp"
#include "ClpSimplex.hpp"
#include "ClpSolve.hpp"
#include "ClpPackedMatrix.hpp"
#include "ClpPlusMinusOneMatrix.hpp"
#include "ClpNetworkMatrix.hpp"
#include "ClpDualRowSteepest.hpp"
#include "ClpDualRowDantzig.hpp"
#include "ClpLinearObjective.hpp"
#include "ClpPrimalColumnSteepest.hpp"
#include "ClpPrimalColumnDantzig.hpp"
#include "ClpPresolve.hpp"
#include "CbcOrClpParam.hpp"
#ifdef DMALLOC
#include "dmalloc.h"
#endif
#ifdef WSSMP_BARRIER
#define FOREIGN_BARRIER
#endif
#ifdef UFL_BARRIER
#define FOREIGN_BARRIER
#endif
#ifdef TAUCS_BARRIER
#define FOREIGN_BARRIER
#endif
#include "CoinWarmStartBasis.hpp"

#include "OsiSolverInterface.hpp"
#include "OsiCuts.hpp"
#include "OsiRowCut.hpp"
#include "OsiColCut.hpp"

#include "CglCutGenerator.hpp"
#include "CglGomory.hpp"
#include "CglProbing.hpp"
#include "CglKnapsackCover.hpp"
#include "CglOddHole.hpp"
#include "CglClique.hpp"
#include "CglFlowCover.hpp"
#include "CglMixedIntegerRounding.hpp"
#include "CglTwomir.hpp"

#include "CbcModel.hpp"
#include "CbcHeuristic.hpp"
#include "CbcCompareActual.hpp"
#include  "CbcOrClpParam.hpp"

#ifdef COIN_USE_CLP
#include "OsiClpSolverInterface.hpp"
#endif
#ifdef COIN_USE_DYLP
#include "OsiDylpSolverInterface.hpp"
#endif

static double totalTime=0.0;

//#############################################################################

#ifdef NDEBUG
#undef NDEBUG
#endif
// Allow for interrupts
// But is this threadsafe ? (so switched off by option)

#include "CoinSignal.hpp"
static CbcModel * currentBranchModel = NULL;

extern "C" {
   static void signal_handler(int whichSignal)
   {
      if (currentBranchModel!=NULL) 
	 currentBranchModel->setMaximumNodes(0); // stop at next node
      return;
   }
}

int mainTest (int argc, const char *argv[],int algorithm,
	      ClpSimplex empty, bool doPresolve,int doIdiot);
// Returns next valid field
int CbcOrClpRead_mode=1;
FILE * CbcOrClpReadCommand=stdin;
int main (int argc, const char *argv[])
{
  // next {} is just to make sure all memory should be freed - for debug
  /* Note
     This is meant as a stand-alone executable to do as much of coin as possible. 
     It should only have one solver known to it.
  */
  {
    double time1 = CoinCpuTime(),time2;
    // Set up all non-standard stuff
#ifdef COIN_USE_CLP
    OsiClpSolverInterface solver1;
    CbcModel model(solver1);
    OsiSolverInterface * solver = model.solver();
    OsiClpSolverInterface * clpSolver = dynamic_cast< OsiClpSolverInterface*> (solver);
    ClpSimplex * lpSolver = clpSolver->getModelPtr();
#endif
    
    
    // default action on import
    int allowImportErrors=0;
    int keepImportNames=1;
    int doIdiot=-1;
    int outputFormat=2;
    int slpValue=-1;
    int printOptions=0;
    int presolveOptions=0;
    int doCrash=0;
    int doSprint=-1;
    // set reasonable defaults
    int preSolve=5;
    bool preSolveFile=false;
// Set up likely cut generators and defaults

    CglGomory gomoryGen;
    // try larger limit
    gomoryGen.setLimit(3000);
    // set default action (0=off,1=on,2=root)
    int gomoryAction=1;

    CglProbing probingGen;
    probingGen.setUsingObjective(true);
    probingGen.setMaxPass(3);
    probingGen.setMaxProbe(100);
    probingGen.setMaxLook(50);
    probingGen.setRowCuts(3);
    // set default action (0=off,1=on,2=root)
    int probingAction=1;

    CglKnapsackCover knapsackGen;
    // set default action (0=off,1=on,2=root)
    int knapsackAction=1;

    CglOddHole oddholeGen;
    oddholeGen.setMinimumViolation(0.005);
    oddholeGen.setMinimumViolationPer(0.0002);
    oddholeGen.setMaximumEntries(100);
    // set default action (0=off,1=on,2=root)
    int oddholeAction=1;

    CglClique cliqueGen;
    cliqueGen.setStarCliqueReport(false);
    cliqueGen.setRowCliqueReport(false);
    // set default action (0=off,1=on,2=root)
    int cliqueAction=1;

    CglMixedIntegerRounding mixedGen;
    // set default action (0=off,1=on,2=root)
    int mixedAction=1;

    CglFlowCover flowGen;
    // set default action (0=off,1=on,2=root)
    int flowAction=1;

    CglTwomir twomirGen;
    // set default action (0=off,1=on,2=root)
    int twomirAction=0;

    bool useRounding=true;
   
    double djFix=1.0e100;
    double gapRatio=1.0e100;
    double tightenFactor=0.0;
    lpSolver->setPerturbation(50);
    lpSolver->messageHandler()->setPrefix(false);
    std::string directory ="./";
    std::string importFile ="";
    std::string exportFile ="default.mps";
    std::string importBasisFile ="";
    int basisHasValues=0;
    std::string exportBasisFile ="default.bas";
    std::string saveFile ="default.prob";
    std::string restoreFile ="default.prob";
    std::string solutionFile ="stdout";
#define CBCMAXPARAMETERS 200
    CbcOrClpParam parameters[CBCMAXPARAMETERS];
    int numberParameters ;
    establishParams(numberParameters,parameters) ;
    parameters[whichParam(BASISIN,numberParameters,parameters)].setStringValue(importBasisFile);
    parameters[whichParam(BASISOUT,numberParameters,parameters)].setStringValue(exportBasisFile);
    parameters[whichParam(DIRECTORY,numberParameters,parameters)].setStringValue(directory);
    parameters[whichParam(DUALBOUND,numberParameters,parameters)].setDoubleValue(lpSolver->dualBound());
    parameters[whichParam(DUALTOLERANCE,numberParameters,parameters)].setDoubleValue(lpSolver->dualTolerance());
    parameters[whichParam(EXPORT,numberParameters,parameters)].setStringValue(exportFile);
    parameters[whichParam(IDIOT,numberParameters,parameters)].setIntValue(doIdiot);
    parameters[whichParam(IMPORT,numberParameters,parameters)].setStringValue(importFile);
    int slog = whichParam(SOLVERLOGLEVEL,numberParameters,parameters);
    int log = whichParam(LOGLEVEL,numberParameters,parameters);
    parameters[slog].setIntValue(lpSolver->logLevel());
    parameters[whichParam(MAXFACTOR,numberParameters,parameters)].setIntValue(lpSolver->factorizationFrequency());
    parameters[whichParam(MAXITERATION,numberParameters,parameters)].setIntValue(lpSolver->maximumIterations());
    parameters[whichParam(OUTPUTFORMAT,numberParameters,parameters)].setIntValue(outputFormat);
    parameters[whichParam(PRESOLVEPASS,numberParameters,parameters)].setIntValue(preSolve);
    parameters[whichParam(PERTVALUE,numberParameters,parameters)].setIntValue(lpSolver->perturbation());
    parameters[whichParam(PRIMALTOLERANCE,numberParameters,parameters)].setDoubleValue(lpSolver->primalTolerance());
    parameters[whichParam(PRIMALWEIGHT,numberParameters,parameters)].setDoubleValue(lpSolver->infeasibilityCost());
    parameters[whichParam(RESTORE,numberParameters,parameters)].setStringValue(restoreFile);
    parameters[whichParam(SAVE,numberParameters,parameters)].setStringValue(saveFile);
    parameters[whichParam(TIMELIMIT,numberParameters,parameters)].setDoubleValue(lpSolver->maximumSeconds());
    parameters[whichParam(SOLUTION,numberParameters,parameters)].setStringValue(solutionFile);
    parameters[whichParam(SPRINT,numberParameters,parameters)].setIntValue(doSprint);
    
    // total number of commands read
    int numberGoodCommands=0;
    bool goodModel=false;

    // Hidden stuff for barrier
    int choleskyType = 0;
    int gamma=0;
    int scaleBarrier=0;
    int doKKT=0;
    int crossover=2; // do crossover unless quadratic
    
    std::string field;
    std::cout<<"Coin Solver version "<<CBCVERSION
	     <<", build "<<__DATE__<<std::endl;
    
    while (1) {
      // next command
      field=CoinReadGetCommand(argc,argv);
      
      // exit if null or similar
      if (!field.length()) {
	if (numberGoodCommands==1&&goodModel) {
	  // we just had file name - do branch and bound
	  field="branch";
	} else if (!numberGoodCommands) {
	  // let's give the sucker a hint
	  std::cout
	    <<"Clp takes input from arguments ( - switches to stdin)"
	    <<std::endl
	    <<"Enter ? for list of commands or help"<<std::endl;
	  field="-";
	} else {
	  break;
	}
      }
      
      // see if ? at end
      int numberQuery=0;
      if (field!="?"&&field!="???") {
	int length = field.length();
	int i;
	for (i=length-1;i>0;i--) {
	  if (field[i]=='?') 
	    numberQuery++;
	  else
	    break;
	}
	field=field.substr(0,length-numberQuery);
      }
      // find out if valid command
      int iParam;
      int numberMatches=0;
      int firstMatch=-1;
      for ( iParam=0; iParam<numberParameters; iParam++ ) {
	int match = parameters[iParam].matches(field);
	if (match==1) {
	  numberMatches = 1;
	  firstMatch=iParam;
	  break;
	} else {
	  if (match&&firstMatch<0)
	    firstMatch=iParam;
	  numberMatches += match>>1;
	}
      }
      if (iParam<numberParameters&&!numberQuery) {
	// found
	CbcOrClpParam found = parameters[iParam];
	CbcOrClpParameterType type = found.type();
	int valid;
	numberGoodCommands++;
#ifdef COIN_USE_CLP
	if (type==BAB&&goodModel) {
	  // check if any integers
	  if (!lpSolver->integerInformation())
	    type=DUALSIMPLEX;
	}
#endif
	if (type==GENERALQUERY) {
	  std::cout<<"In argument list keywords have leading - "
	    ", -stdin or just - switches to stdin"<<std::endl;
	  std::cout<<"One command per line (and no -)"<<std::endl;
	  std::cout<<"abcd? gives list of possibilities, if only one + explanation"<<std::endl;
	  std::cout<<"abcd?? adds explanation, if only one fuller help"<<std::endl;
	  std::cout<<"abcd without value (where expected) gives current value"<<std::endl;
	  std::cout<<"abcd value sets value"<<std::endl;
	  std::cout<<"Commands are:"<<std::endl;
	  int maxAcross=5;
	  int limits[]={1,51,101,151,201,251,301,351,401};
	  std::vector<std::string> types;
	  types.push_back("Double parameters:");
	  types.push_back("Branch and Cut double parameters:");
	  types.push_back("Integer parameters:");
	  types.push_back("Branch and Cut integer parameters:");
	  types.push_back("Keyword parameters and others:");
	  types.push_back("Branch and Cut keyword parameters and others:");
	  types.push_back("Actions:");
	  types.push_back("Branch and Cut actions:");
	  int iType;
	  for (iType=0;iType<8;iType++) {
	    int across=0;
	    std::cout<<types[iType]<<"   ";
	    for ( iParam=0; iParam<numberParameters; iParam++ ) {
	      int type = parameters[iParam].type();
	      if (parameters[iParam].displayThis()&&type>=limits[iType]
		  &&type<limits[iType+1]) {
		if (!across)
		  std::cout<<"  ";
		std::cout<<parameters[iParam].matchName()<<"  ";
		across++;
		if (across==maxAcross) {
		  std::cout<<std::endl;
		  across=0;
		}
	      }
	    }
	    if (across)
	      std::cout<<std::endl;
	  }
	} else if (type==FULLGENERALQUERY) {
	  std::cout<<"Full list of commands is:"<<std::endl;
	  int maxAcross=5;
	  int limits[]={1,51,101,151,201,251,301,351,401};
	  std::vector<std::string> types;
	  types.push_back("Double parameters:");
	  types.push_back("Branch and Cut double parameters:");
	  types.push_back("Integer parameters:");
	  types.push_back("Branch and Cut integer parameters:");
	  types.push_back("Keyword parameters and others:");
	  types.push_back("Branch and Cut keyword parameters and others:");
	  types.push_back("Actions:");
	  types.push_back("Branch and Cut actions:");
	  int iType;
	  for (iType=0;iType<8;iType++) {
	    int across=0;
	    std::cout<<types[iType]<<"  ";
	    for ( iParam=0; iParam<numberParameters; iParam++ ) {
	      int type = parameters[iParam].type();
	      if (type>=limits[iType]
		  &&type<limits[iType+1]) {
		if (!across)
		  std::cout<<"  ";
		std::cout<<parameters[iParam].matchName()<<"  ";
		across++;
		if (across==maxAcross) {
		  std::cout<<std::endl;
		  across=0;
		}
	      }
	    }
	    if (across)
	      std::cout<<std::endl;
	  }
	} else if (type<101) {
	  // get next field as double
	  double value = CoinReadGetDoubleField(argc,argv,&valid);
	  if (!valid) {
	    parameters[iParam].setDoubleValue(value);
	    if (type<51) {
	      parameters[iParam].setDoubleParameter(lpSolver,value);
	    } else if (type<81) {
	      parameters[iParam].setDoubleParameter(model,value);
	    } else {
	      switch(type) {
	      case DJFIX:
		djFix=value;
		preSolve=5;
		break;
	      case GAPRATIO:
		gapRatio=value;
		break;
	      case TIGHTENFACTOR:
		tightenFactor=value;
		break;
	      default:
		abort();
	      }
	    }
	  } else if (valid==1) {
	    abort();
	  } else {
	    std::cout<<parameters[iParam].name()<<" has value "<<
	      parameters[iParam].doubleValue()<<std::endl;
	  }
	} else if (type<201) {
	  // get next field as int
	  int value = CoinReadGetIntField(argc,argv,&valid);
	  if (!valid) {
	    parameters[iParam].setIntValue(value);
	    if (type<151) {
	      if (parameters[iParam].type()==PRESOLVEPASS)
		preSolve = value;
	      else if (parameters[iParam].type()==IDIOT)
		doIdiot = value;
	      else if (parameters[iParam].type()==SPRINT)
		doSprint = value;
	      else if (parameters[iParam].type()==OUTPUTFORMAT)
		outputFormat = value;
	      else if (parameters[iParam].type()==SLPVALUE)
		slpValue = value;
	      else if (parameters[iParam].type()==PRESOLVEOPTIONS)
		presolveOptions = value;
	      else if (parameters[iParam].type()==PRINTOPTIONS)
		printOptions = value;
	      else
		parameters[iParam].setIntParameter(lpSolver,value);
	    } else {
	      parameters[iParam].setIntParameter(model,value);
	    }
	  } else if (valid==1) {
	    abort();
	  } else {
	    std::cout<<parameters[iParam].name()<<" has value "<<
	      parameters[iParam].intValue()<<std::endl;
	  }
	} else if (type<301) {
	  // one of several strings
	  std::string value = CoinReadGetString(argc,argv);
	  int action = parameters[iParam].parameterOption(value);
	  if (action<0) {
	    if (value!="EOL") {
	      // no match
	      parameters[iParam].printOptions();
	    } else {
	      // print current value
	      std::cout<<parameters[iParam].name()<<" has value "<<
		parameters[iParam].currentOption()<<std::endl;
	    }
	  } else {
	    parameters[iParam].setCurrentOption(action);
	    // for now hard wired
	    switch (type) {
	    case DIRECTION:
	      if (action==0)
		lpSolver->setOptimizationDirection(1);
	      else if (action==1)
		lpSolver->setOptimizationDirection(-1);
	      else
		lpSolver->setOptimizationDirection(0);
	      break;
	    case DUALPIVOT:
	      if (action==0) {
		ClpDualRowSteepest steep(3);
		lpSolver->setDualRowPivotAlgorithm(steep);
	      } else if (action==1) {
		//ClpDualRowDantzig dantzig;
		ClpDualRowSteepest dantzig(5);
		lpSolver->setDualRowPivotAlgorithm(dantzig);
	      } else if (action==2) {
		// partial steep
		ClpDualRowSteepest steep(2);
		lpSolver->setDualRowPivotAlgorithm(steep);
	      } else {
		ClpDualRowSteepest steep;
		lpSolver->setDualRowPivotAlgorithm(steep);
	      }
	      break;
	    case PRIMALPIVOT:
	      if (action==0) {
		ClpPrimalColumnSteepest steep(3);
		lpSolver->setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==1) {
		ClpPrimalColumnSteepest steep(0);
		lpSolver->setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==2) {
		ClpPrimalColumnDantzig dantzig;
		lpSolver->setPrimalColumnPivotAlgorithm(dantzig);
	      } else if (action==3) {
		ClpPrimalColumnSteepest steep(2);
		lpSolver->setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==4) {
		ClpPrimalColumnSteepest steep(1);
		lpSolver->setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==5) {
		ClpPrimalColumnSteepest steep(4);
		lpSolver->setPrimalColumnPivotAlgorithm(steep);
	      } else if (action==6) {
		ClpPrimalColumnSteepest steep(10);
		lpSolver->setPrimalColumnPivotAlgorithm(steep);
	      }
	      break;
	    case SCALING:
	      lpSolver->scaling(action);
	      solver->setHintParam(OsiDoScale,action!=0,OsiHintTry);
	      break;
	    case AUTOSCALE:
	      lpSolver->setAutomaticScaling(action!=0);
	      break;
	    case SPARSEFACTOR:
	      lpSolver->setSparseFactorization((1-action)!=0);
	      break;
	    case BIASLU:
	      lpSolver->factorization()->setBiasLU(action);
	      break;
	    case PERTURBATION:
	      if (action==0)
		lpSolver->setPerturbation(50);
	      else
		lpSolver->setPerturbation(100);
	      break;
	    case ERRORSALLOWED:
	      allowImportErrors = action;
	      break;
	    case KEEPNAMES:
	      keepImportNames = 1-action;
	      break;
	    case PRESOLVE:
	      if (action==0)
		preSolve = 5;
	      else if (action==1)
		preSolve=0;
	      else if (action==2)
		preSolve=10;
	      else
		preSolveFile=true;
	      break;
	    case PFI:
	      lpSolver->factorization()->setForrestTomlin(action==0);
	      break;
	    case CRASH:
	      doCrash=action;
	      break;
	    case MESSAGES:
	      lpSolver->messageHandler()->setPrefix(action!=0);
	      break;
	    case CHOLESKY:
	      choleskyType = action;
	      break;
	    case GAMMA:
	      gamma=action;
	      break;
	    case BARRIERSCALE:
	      scaleBarrier=action;
	      break;
	    case KKT:
	      doKKT=action;
	      break;
	    case CROSSOVER:
	      crossover=action;
	      break;
	    case GOMORYCUTS:
	      gomoryAction = action;
	      break;
	    case PROBINGCUTS:
	      probingAction = action;
	      break;
	    case KNAPSACKCUTS:
	      knapsackAction = action;
	      break;
	    case ODDHOLECUTS:
	      oddholeAction = action;
	      break;
	    case CLIQUECUTS:
	      cliqueAction = action;
	      break;
	    case FLOWCUTS:
	      flowAction = action;
	      break;
	    case MIXEDCUTS:
	      mixedAction = action;
	      break;
	    case TWOMIRCUTS:
	      twomirAction = action;
	      break;
	    case ROUNDING:
	      useRounding = action;
	      break;
	    case COSTSTRATEGY:
	      if (action!=1) {
		printf("Pseudo costs not implemented yet\n");
	      } else {
		int numberColumns = model.getNumCols();
		int * sort = new int[numberColumns];
		double * dsort = new double[numberColumns];
		int * priority = new int [numberColumns];
		const double * objective = model.getObjCoefficients();
		int iColumn;
		int n=0;
		for (iColumn=0;iColumn<numberColumns;iColumn++) {
		  if (model.isInteger(iColumn)) {
		    sort[n]=n;
		    dsort[n++]=-objective[iColumn];
		  }
		}
		CoinSort_2(dsort,dsort+n,sort);
		int level=0;
		double last = -1.0e100;
		for (int i=0;i<n;i++) {
		  int iPut=sort[i];
		  if (dsort[i]!=last) {
		    level++;
		    last=dsort[i];
		  }
		  priority[iPut]=level;
		}
		model.passInPriorities( priority,false);
		delete [] priority;
		delete [] sort;
		delete [] dsort;
	      }
	      break;
	    default:
	      abort();
	    }
	  }
	} else {
	  // action
	  if (type==EXIT)
	    break; // stop all
	  switch (type) {
	  case DUALSIMPLEX:
	  case PRIMALSIMPLEX:
	  case SOLVECONTINUOUS:
	  case BARRIER:
	    if (goodModel) {
	      ClpSolve::SolveType method;
	      ClpSolve::PresolveType presolveType;
	      ClpSimplex * model2 = lpSolver;
	      ClpSolve solveOptions;
	      if (preSolve!=5&&preSolve)
		presolveType=ClpSolve::presolveNumber;
	      else if (preSolve)
		presolveType=ClpSolve::presolveOn;
	      else
		presolveType=ClpSolve::presolveOff;
	      solveOptions.setPresolveType(presolveType,preSolve);
	      if (type==DUALSIMPLEX||SOLVECONTINUOUS) {
		method=ClpSolve::useDual;
	      } else if (type==PRIMALSIMPLEX) {
		method=ClpSolve::usePrimalorSprint;
	      } else {
		method = ClpSolve::useBarrier;
		if (crossover==1) {
		  method=ClpSolve::useBarrierNoCross;
		} else if (crossover==2) {
		  ClpObjective * obj = lpSolver->objectiveAsObject();
		  if (obj->type()>1) {
		    method=ClpSolve::useBarrierNoCross;
		    presolveType=ClpSolve::presolveOff;
		    solveOptions.setPresolveType(presolveType,preSolve);
		  } 
		}
	      }
	      solveOptions.setSolveType(method);
	      if(preSolveFile)
		presolveOptions |= 0x40000000;
	      solveOptions.setSpecialOption(4,presolveOptions);
	      solveOptions.setSpecialOption(5,printOptions);
	      if (method==ClpSolve::useDual) {
		// dual
		if (doCrash)
		  solveOptions.setSpecialOption(0,1,doCrash); // crash
		else if (doIdiot)
		  solveOptions.setSpecialOption(0,2,doIdiot); // possible idiot
	      } else if (method==ClpSolve::usePrimalorSprint) {
		// primal
		// if slp turn everything off
		if (slpValue>0) {
		  doCrash=false;
		  doSprint=0;
		  doIdiot=-1;
		  solveOptions.setSpecialOption(1,10,slpValue); // slp
		  method=ClpSolve::usePrimal;
		}
		if (doCrash) {
		  solveOptions.setSpecialOption(1,1,doCrash); // crash
		} else if (doSprint>0) {
		  // sprint overrides idiot
		  solveOptions.setSpecialOption(1,3,doSprint); // sprint
		} else if (doIdiot>0) {
		  solveOptions.setSpecialOption(1,2,doIdiot); // idiot
		} else if (slpValue<=0) {
		  if (doIdiot==0) {
		    if (doSprint==0)
		      solveOptions.setSpecialOption(1,4); // all slack
		    else
		      solveOptions.setSpecialOption(1,9); // all slack or sprint
		  } else {
		    if (doSprint==0)
		      solveOptions.setSpecialOption(1,8); // all slack or idiot
		    else
		      solveOptions.setSpecialOption(1,7); // initiative
		  }
		}
		if (basisHasValues==-1)
		  solveOptions.setSpecialOption(1,11); // switch off values
	      } else if (method==ClpSolve::useBarrier||method==ClpSolve::useBarrierNoCross) {
		int barrierOptions = choleskyType;
		if (scaleBarrier)
		  barrierOptions |= 8;
		if (gamma)
		  barrierOptions |= 16;
		if (doKKT)
		  barrierOptions |= 32;
		solveOptions.setSpecialOption(4,barrierOptions);
	      }
	      model2->initialSolve(solveOptions);
	      basisHasValues=1;
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
	  case TIGHTEN:
	    if (goodModel) {
     	      int numberInfeasibilities = lpSolver->tightenPrimalBounds();
	      if (numberInfeasibilities)
		std::cout<<"** Analysis indicates model infeasible"<<std::endl;
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
	  case PLUSMINUS:
	    if (goodModel) {
	      ClpMatrixBase * saveMatrix = lpSolver->clpMatrix();
	      ClpPackedMatrix* clpMatrix =
		dynamic_cast< ClpPackedMatrix*>(saveMatrix);
	      if (clpMatrix) {
		ClpPlusMinusOneMatrix * newMatrix = new ClpPlusMinusOneMatrix(*(clpMatrix->matrix()));
		if (newMatrix->getIndices()) {
		  lpSolver->replaceMatrix(newMatrix);
		  delete saveMatrix;
		  std::cout<<"Matrix converted to +- one matrix"<<std::endl;
		} else {
		  std::cout<<"Matrix can not be converted to +- 1 matrix"<<std::endl;
		}
	      } else {
		std::cout<<"Matrix not a ClpPackedMatrix"<<std::endl;
	      }
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
	  case NETWORK:
	    if (goodModel) {
	      ClpMatrixBase * saveMatrix = lpSolver->clpMatrix();
	      ClpPackedMatrix* clpMatrix =
		dynamic_cast< ClpPackedMatrix*>(saveMatrix);
	      if (clpMatrix) {
		ClpNetworkMatrix * newMatrix = new ClpNetworkMatrix(*(clpMatrix->matrix()));
		if (newMatrix->getIndices()) {
		  lpSolver->replaceMatrix(newMatrix);
		  delete saveMatrix;
		  std::cout<<"Matrix converted to network matrix"<<std::endl;
		} else {
		  std::cout<<"Matrix can not be converted to network matrix"<<std::endl;
		}
	      } else {
		std::cout<<"Matrix not a ClpPackedMatrix"<<std::endl;
	      }
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
/*
  Run branch-and-cut. First set a few options -- node comparison, scaling. If
  the solver is Clp, consider running some presolve code (not yet converted
  this to generic OSI) with branch-and-cut. If presolve is disabled, or the
  solver is not Clp, simply run branch-and-cut. Print elapsed time at the end.
*/
	  case BAB: // branchAndBound
	  { if (goodModel)
	    { CbcCompareDefault compare; // Definition of node choice
	      model.setNodeComparison(compare);
	      model.messageHandler()->setLogLevel(parameters[log].intValue());
		int depthCriterion= parameters[whichParam(CUTDEPTH,
							  numberParameters,
							  parameters)].intValue();
		// add cut generators if wanted
		if (probingAction==1)
		  model.addCutGenerator(&probingGen,-1,"Probing",
					true,false,false,-100,depthCriterion);
		else if (probingAction==2)
		  model.addCutGenerator(&probingGen,-99,"Probing");
		if (gomoryAction==1)
		  model.addCutGenerator(&gomoryGen,-1,"Gomory",
					true,false,false,-100,depthCriterion);
		else if (gomoryAction==2)
		  model.addCutGenerator(&gomoryGen,-99,"Gomory");
		if (knapsackAction==1)
		  model.addCutGenerator(&knapsackGen,-1,"Knapsack",
					true,false,false,-100,depthCriterion);
		else if (knapsackAction==2)
		  model.addCutGenerator(&knapsackGen,-99,"Knapsack");
		if (oddholeAction==1)
		  model.addCutGenerator(&oddholeGen,-1,"OddHole",
					true,false,false,-100,depthCriterion);
		else if (oddholeAction==2)
		  model.addCutGenerator(&oddholeGen,-99,"OddHole");
		if (cliqueAction==1)
		  model.addCutGenerator(&cliqueGen,-1,"Clique",
					true,false,false,-100,depthCriterion);
		else if (cliqueAction==2)
		  model.addCutGenerator(&cliqueGen,-99,"Clique");
		if (mixedAction==1)
		  model.addCutGenerator(&mixedGen,-1,"MixedintegerRounding",
					true,false,false,-100,depthCriterion);
		else if (mixedAction==2)
		  model.addCutGenerator(&mixedGen,-99,"MixedintegerRounding");
		if (flowAction==1)
		  model.addCutGenerator(&flowGen,-1,"FlowCover",
					true,false,false,-100,depthCriterion);
		else if (flowAction==2)
		  model.addCutGenerator(&flowGen,-99,"FlowCover");
		if (twomirAction==1)
		  model.addCutGenerator(&twomirGen,-1,"TwoMirCuts",
					true,false,false,-100,depthCriterion);
		else if (twomirAction==2)
		  model.addCutGenerator(&twomirGen,-99,"TwoMirCuts");
	      OsiSolverInterface * solver = model.solver();
#ifdef COIN_USE_CLP
	      OsiClpSolverInterface * si =
		dynamic_cast<OsiClpSolverInterface *>(solver) ;
	      if (preSolve&&si != NULL) {
		// get clp itself
		ClpSimplex * modelC = si->getModelPtr();
		if (si->messageHandler()->logLevel())
		  si->messageHandler()->setLogLevel(1);
		if (modelC->tightenPrimalBounds()!=0) {
		  std::cout<<"Problem is infeasible!"<<std::endl;
		  break;
		}
		model.initialSolve();
		// bounds based on continuous
		if (tightenFactor) {
		  if (modelC->tightenPrimalBounds(tightenFactor)!=0) {
		    std::cout<<"Problem is infeasible!"<<std::endl;
		    break;
		  }
		}
		if (gapRatio<1.0e100) {
		  double value = si->getObjValue();
		  double value2 = gapRatio*(1.0e-5+fabs(value));
		  model.setAllowableGap(value2);
		  model.setAllowableFractionGap(gapRatio);
		  std::cout<<"Continuous "<<value
			   <<", so allowable gap set to "<<value2<<std::endl;
		}
		if (djFix<1.0e20) {
		  // do some fixing
		  int numberColumns = modelC->numberColumns();
		  int i;
		  const char * type = modelC->integerInformation();
		  double * lower = modelC->columnLower();
		  double * upper = modelC->columnUpper();
		  double * solution = modelC->primalColumnSolution();
		  double * dj = modelC->dualColumnSolution();
		  for (i=0;i<numberColumns;i++) {
		    if (type[i]) {
		      double value = solution[i];
		      if (value<lower[i]+1.0e-5&&dj[i]>djFix) {
			solution[i]=lower[i];
			upper[i]=lower[i];
		      } else if (value>upper[i]-1.0e-5&&dj[i]<-djFix) {
			solution[i]=upper[i];
			lower[i]=upper[i];
		      }
		    }
		  }
		}
		{
		  // integer presolve
		  if (si->messageHandler()->logLevel())
		    si->messageHandler()->setLogLevel(si->messageHandler()->logLevel()-1);
		  CbcModel * model2 = model.integerPresolve();
		  if (model2) {
		    // Do complete search
		    
		    CbcRounding heuristic1(*model2);
		    if (useRounding)
		      model2->addHeuristic(&heuristic1);
		    OsiClpSolverInterface * si =
		      dynamic_cast<OsiClpSolverInterface *>(model2->solver()) ;
		    if (si->messageHandler()->logLevel())
		      si->messageHandler()->setLogLevel(si->messageHandler()->logLevel()-1);
		    CoinSighandler_t saveSignal=SIG_DFL;
		    currentBranchModel = model2;
		    // register signal handler
		    saveSignal = signal(SIGINT,signal_handler);
		    model2->branchAndBound() ;
		    signal(SIGINT,saveSignal);
		    currentBranchModel = NULL;
		    // get back solution
		    model.originalModel(model2,false);
		    delete model2;
		  } else {
		    // infeasible
		    exit(1);
		  }
		}
	      } else
#endif
	      { if (model.solver()->messageHandler()->logLevel())
		  model.solver()->messageHandler()->setLogLevel(1) ;
		model.initialSolve() ;
		if (gapRatio < 1.0e100)
		{ double value = model.solver()->getObjValue() ;
		  double value2 = gapRatio*(1.0e-5+fabs(value)) ;
		  model.setAllowableGap(value2) ;
		  std::cout << "Continuous " << value
			    << ", so allowable gap set to "
			    << value2 << std::endl ; }
		CbcRounding heuristic1(model) ;
		if (useRounding)
		  model.addHeuristic(&heuristic1) ;
		if (model.solver()->messageHandler()->logLevel())
		  model.solver()->messageHandler()->setLogLevel(model.solver()->messageHandler()->logLevel()-1);
		parameters[slog].setIntValue(parameters[slog].intValue()-1);
		CoinSighandler_t saveSignal=SIG_DFL;
		currentBranchModel = &model;
		// register signal handler
		saveSignal = signal(SIGINT,signal_handler);
		model.branchAndBound() ;
		signal(SIGINT,saveSignal);
		currentBranchModel = NULL;
		model.solver()->messageHandler()->setLogLevel(parameters[slog].intValue());
	      }
		if (model.bestSolution())
		  { std::cout << "Optimal solution "
			      << model.solver()->getObjValue() << std::endl ; }
		else
		  { std::cout << "No integer solution found." << std::endl ; }
		
		time2 = CoinCpuTime() ;
		totalTime += time2-time1 ;
		std::cout << "Result " << model.solver()->getObjValue()
			  << " took " << time2-time1 << " seconds - total "
			  << totalTime << std::endl ;
		time1 = time2 ; 
                // Get solver again
#ifdef COIN_USE_CLP
                OsiClpSolverInterface * clpSolver = dynamic_cast< OsiClpSolverInterface*> (model.solver());
                lpSolver = clpSolver->getModelPtr();
#endif
            }
	  else
	    { std::cout << "** Current model not valid" << std::endl ; }
	  break ; }
	  case IMPORT:
	    {
	      // get next field
	      field = CoinReadGetString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field=="-") {
		// stdin
		canOpen=true;
		fileName = "-";
	      } else {
		if (field[0]=='/'||field[0]=='\\') {
		  fileName = field;
		} else if (field[0]=='~') {
		  char * environ = getenv("HOME");
		  if (environ) {
		    std::string home(environ);
		    field=field.erase(0,1);
		    fileName = home+field;
		  } else {
		    fileName=field;
		  }
		} else {
		  fileName = directory+field;
		}
		FILE *fp=fopen(fileName.c_str(),"r");
		if (fp) {
		  // can open - lets go for it
		  fclose(fp);
		  canOpen=true;
		} else {
		  std::cout<<"Unable to open file "<<fileName<<std::endl;
		}
	      }
	      if (canOpen) {
		int status =lpSolver->readMps(fileName.c_str(),
						   keepImportNames!=0,
						   allowImportErrors!=0);
		if (!status||(status>0&&allowImportErrors)) {
		  goodModel=true;
		  //Set integers in clpsolver
		  const char * info = lpSolver->integerInformation();
		  if (info) {
		    int numberColumns = lpSolver->numberColumns();
		    int i;
		    for (i=0;i<numberColumns;i++) {
		      if (info[i]) 
			clpSolver->setInteger(i);
		    }
		  }
		  // sets to all slack (not necessary?)
		  lpSolver->createStatus();
		  time2 = CoinCpuTime();
		  totalTime += time2-time1;
		  time1=time2;
		  // Go to canned file if just input file
		  if (CbcOrClpRead_mode==2&&argc==2) {
		    // only if ends .mps
		    char * find = strstr(fileName.c_str(),".mps");
		    if (find&&find[4]=='\0') {
		      find[1]='p'; find[2]='a';find[3]='r';
		      FILE *fp=fopen(fileName.c_str(),"r");
		      if (fp) {
			CbcOrClpReadCommand=fp; // Read from that file
			CbcOrClpRead_mode=-1;
		      }
		    }
		  }
		} else {
		  // errors
		  std::cout<<"There were "<<status<<
		    " errors on input"<<std::endl;
		}
	      }
	    }
	    break;
	  case EXPORT:
	    if (goodModel) {
	      // get next field
	      field = CoinReadGetString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field[0]=='/'||field[0]=='\\') {
		fileName = field;
	      } else if (field[0]=='~') {
		char * environ = getenv("HOME");
		if (environ) {
		  std::string home(environ);
		  field=field.erase(0,1);
		  fileName = home+field;
		} else {
		  fileName=field;
		}
	      } else {
		fileName = directory+field;
	      }
	      FILE *fp=fopen(fileName.c_str(),"w");
	      if (fp) {
		// can open - lets go for it
		fclose(fp);
		canOpen=true;
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	      if (canOpen) {
		// If presolve on then save presolved
		bool deleteModel2=false;
		ClpSimplex * model2 = lpSolver;
		if (preSolve) {
		  ClpPresolve pinfo;
		  int presolveOptions2 = presolveOptions&~0x40000000;
		  if ((presolveOptions2&0xffff)!=0)
		    pinfo.setPresolveActions(presolveOptions2);
		  if ((printOptions&1)!=0)
		    pinfo.statistics();
		  model2 = 
		    pinfo.presolvedModel(*lpSolver,1.0e-8,
					 true,preSolve);
		  if (model2) {
		    printf("Saving presolved model on %s\n",
			   fileName.c_str());
		    deleteModel2=true;
		  } else {
		    printf("Presolved model looks infeasible - saving original on %s\n",
			   fileName.c_str());
		    deleteModel2=false;
		    model2 = lpSolver;

		  }
		} else {
		  printf("Saving model on %s\n",
			   fileName.c_str());
		}
#if 0
		// Convert names
		int iRow;
		int numberRows=model2->numberRows();
		int iColumn;
		int numberColumns=model2->numberColumns();

		char ** rowNames = NULL;
		char ** columnNames = NULL;
		if (model2->lengthNames()) {
		  rowNames = new char * [numberRows];
		  for (iRow=0;iRow<numberRows;iRow++) {
		    rowNames[iRow] = 
		      strdup(model2->rowName(iRow).c_str());
#ifdef STRIPBLANKS
		    char * xx = rowNames[iRow];
		    int i;
		    int length = strlen(xx);
		    int n=0;
		    for (i=0;i<length;i++) {
		      if (xx[i]!=' ')
			xx[n++]=xx[i];
		    }
		    xx[n]='\0';
#endif
		  }
		  
		  columnNames = new char * [numberColumns];
		  for (iColumn=0;iColumn<numberColumns;iColumn++) {
		    columnNames[iColumn] = 
		      strdup(model2->columnName(iColumn).c_str());
#ifdef STRIPBLANKS
		    char * xx = columnNames[iColumn];
		    int i;
		    int length = strlen(xx);
		    int n=0;
		    for (i=0;i<length;i++) {
		      if (xx[i]!=' ')
			xx[n++]=xx[i];
		    }
		    xx[n]='\0';
#endif
		  }
		}
		CoinMpsIO writer;
		writer.setMpsData(*model2->matrix(), COIN_DBL_MAX,
				  model2->getColLower(), model2->getColUpper(),
				  model2->getObjCoefficients(),
				  (const char*) 0 /*integrality*/,
				  model2->getRowLower(), model2->getRowUpper(),
				  columnNames, rowNames);
		// Pass in array saying if each variable integer
		writer.copyInIntegerInformation(model2->integerInformation());
		writer.setObjectiveOffset(model2->objectiveOffset());
		writer.writeMps(fileName.c_str(),0,1,1);
		if (rowNames) {
		  for (iRow=0;iRow<numberRows;iRow++) {
		    free(rowNames[iRow]);
		  }
		  delete [] rowNames;
		  for (iColumn=0;iColumn<numberColumns;iColumn++) {
		    free(columnNames[iColumn]);
		  }
		  delete [] columnNames;
		}
#else
		model2->writeMps(fileName.c_str(),(outputFormat-1)/2,1+((outputFormat-1)&1));
#endif
		if (deleteModel2)
		  delete model2;
		time2 = CoinCpuTime();
		totalTime += time2-time1;
		time1=time2;
	      }
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
	  case BASISIN:
	    if (goodModel) {
	      // get next field
	      field = CoinReadGetString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field=="-") {
		// stdin
		canOpen=true;
		fileName = "-";
	      } else {
		if (field[0]=='/'||field[0]=='\\') {
		  fileName = field;
		} else if (field[0]=='~') {
		  char * environ = getenv("HOME");
		  if (environ) {
		    std::string home(environ);
		    field=field.erase(0,1);
		    fileName = home+field;
		  } else {
		    fileName=field;
		  }
		} else {
		  fileName = directory+field;
		}
		FILE *fp=fopen(fileName.c_str(),"r");
		if (fp) {
		  // can open - lets go for it
		  fclose(fp);
		  canOpen=true;
		} else {
		  std::cout<<"Unable to open file "<<fileName<<std::endl;
		}
	      }
	      if (canOpen) {
		int values = lpSolver->readBasis(fileName.c_str());
		if (values==0)
		  basisHasValues=-1;
		else
		  basisHasValues=1;
	      }
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
	  case BASISOUT:
	    if (goodModel) {
	      // get next field
	      field = CoinReadGetString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field[0]=='/'||field[0]=='\\') {
		fileName = field;
	      } else if (field[0]=='~') {
		char * environ = getenv("HOME");
		if (environ) {
		  std::string home(environ);
		  field=field.erase(0,1);
		  fileName = home+field;
		} else {
		  fileName=field;
		}
	      } else {
		fileName = directory+field;
	      }
	      FILE *fp=fopen(fileName.c_str(),"w");
	      if (fp) {
		// can open - lets go for it
		fclose(fp);
		canOpen=true;
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	      if (canOpen) {
		ClpSimplex * model2 = lpSolver;
		model2->writeBasis(fileName.c_str(),outputFormat>1,outputFormat-2);
		time2 = CoinCpuTime();
		totalTime += time2-time1;
		time1=time2;
	      }
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	    }
	    break;
	  case SAVE:
	    {
	      // get next field
	      field = CoinReadGetString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field[0]=='/'||field[0]=='\\') {
		fileName = field;
	      } else if (field[0]=='~') {
		char * environ = getenv("HOME");
		if (environ) {
		  std::string home(environ);
		  field=field.erase(0,1);
		  fileName = home+field;
		} else {
		  fileName=field;
		}
	      } else {
		fileName = directory+field;
	      }
	      FILE *fp=fopen(fileName.c_str(),"wb");
	      if (fp) {
		// can open - lets go for it
		fclose(fp);
		canOpen=true;
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	      if (canOpen) {
		int status;
		// If presolve on then save presolved
		bool deleteModel2=false;
		ClpSimplex * model2 = lpSolver;
		if (preSolve) {
		  ClpPresolve pinfo;
		  model2 = 
		    pinfo.presolvedModel(*lpSolver,1.0e-8,
					 false,preSolve);
		  if (model2) {
		    printf("Saving presolved model on %s\n",
			   fileName.c_str());
		    deleteModel2=true;
		  } else {
		    printf("Presolved model looks infeasible - saving original on %s\n",
			   fileName.c_str());
		    deleteModel2=false;
		    model2 = lpSolver;

		  }
		} else {
		  printf("Saving model on %s\n",
			   fileName.c_str());
		}
		status =model2->saveModel(fileName.c_str());
		if (deleteModel2)
		  delete model2;
		if (!status) {
		  goodModel=true;
		  time2 = CoinCpuTime();
		  totalTime += time2-time1;
		  time1=time2;
		} else {
		  // errors
		  std::cout<<"There were errors on output"<<std::endl;
		}
	      }
	    }
	    break;
	  case RESTORE:
	    {
	      // get next field
	      field = CoinReadGetString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      bool canOpen=false;
	      if (field[0]=='/'||field[0]=='\\') {
		fileName = field;
	      } else if (field[0]=='~') {
		char * environ = getenv("HOME");
		if (environ) {
		  std::string home(environ);
		  field=field.erase(0,1);
		  fileName = home+field;
		} else {
		  fileName=field;
		}
	      } else {
		fileName = directory+field;
	      }
	      FILE *fp=fopen(fileName.c_str(),"rb");
	      if (fp) {
		// can open - lets go for it
		fclose(fp);
		canOpen=true;
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	      if (canOpen) {
		int status =lpSolver->restoreModel(fileName.c_str());
		if (!status) {
		  goodModel=true;
		  time2 = CoinCpuTime();
		  totalTime += time2-time1;
		  time1=time2;
		} else {
		  // errors
		  std::cout<<"There were errors on input"<<std::endl;
		}
	      }
	    }
	    break;
	  case MAXIMIZE:
	    lpSolver->setOptimizationDirection(-1);
	    break;
	  case MINIMIZE:
	    lpSolver->setOptimizationDirection(1);
	    break;
	  case ALLSLACK:
	    lpSolver->allSlackBasis(true);
	    break;
	  case REVERSE:
	    if (goodModel) {
	      int iColumn;
	      int numberColumns=lpSolver->numberColumns();
	      double * dualColumnSolution = 
		lpSolver->dualColumnSolution();
	      ClpObjective * obj = lpSolver->objectiveAsObject();
	      assert(dynamic_cast<ClpLinearObjective *> (obj));
	      double offset;
	      double * objective = obj->gradient(NULL,NULL,offset,true);
	      for (iColumn=0;iColumn<numberColumns;iColumn++) {
		dualColumnSolution[iColumn] = dualColumnSolution[iColumn];
		objective[iColumn] = -objective[iColumn];
	      }
	      int iRow;
	      int numberRows=lpSolver->numberRows();
	      double * dualRowSolution = 
		lpSolver->dualRowSolution();
	      for (iRow=0;iRow<numberRows;iRow++) 
		dualRowSolution[iRow] = dualRowSolution[iRow];
	    }
	    break;
	  case DIRECTORY:
	    {
	      std::string name = CoinReadGetString(argc,argv);
	      if (name!="EOL") {
		int length=name.length();
		if (name[length-1]=='/'||name[length-1]=='\\')
		  directory=name;
		else
		  directory = name+"/";
		parameters[iParam].setStringValue(directory);
	      } else {
		parameters[iParam].printString();
	      }
	    }
	    break;
	  case STDIN:
	    CbcOrClpRead_mode=-1;
	    break;
	  case NETLIB_DUAL:
	  case NETLIB_BARRIER:
	  case NETLIB_PRIMAL:
	    {
	      // create fields for unitTest
	      const char * fields[4];
	      int nFields=2;
	      fields[0]="fake main from unitTest";
	      fields[1]="-netlib";
	      if (directory!="./") {
		fields[2]="-netlibDir";
		fields[3]=directory.c_str();
		nFields=4;
	      }
	      int algorithm;
	      if (type==NETLIB_DUAL) {
		std::cerr<<"Doing netlib with dual agorithm"<<std::endl;
		algorithm =0;
	      } else if (type==NETLIB_BARRIER) {
		std::cerr<<"Doing netlib with barrier agorithm"<<std::endl;
		algorithm =2;
	      } else {
		std::cerr<<"Doing netlib with primal agorithm"<<std::endl;
		algorithm=1;
	      }
	      mainTest(nFields,fields,algorithm,*lpSolver,
		       (preSolve!=0),doIdiot);
	    }
	    break;
	  case UNITTEST:
	    {
	      // create fields for unitTest
	      const char * fields[3];
	      int nFields=1;
	      fields[0]="fake main from unitTest";
	      if (directory!="./") {
		fields[1]="-mpsDir";
		fields[2]=directory.c_str();
		nFields=3;
	      }
	      mainTest(nFields,fields,false,*lpSolver,(preSolve!=0),
		       false);
	    }
	    break;
	  case FAKEBOUND:
	    if (goodModel) {
	      // get bound
	      double value = CoinReadGetDoubleField(argc,argv,&valid);
	      if (!valid) {
		std::cout<<"Setting "<<parameters[iParam].name()<<
		  " to DEBUG "<<value<<std::endl;
		int iRow;
		int numberRows=lpSolver->numberRows();
		double * rowLower = lpSolver->rowLower();
		double * rowUpper = lpSolver->rowUpper();
		for (iRow=0;iRow<numberRows;iRow++) {
		  // leave free ones for now
		  if (rowLower[iRow]>-1.0e20||rowUpper[iRow]<1.0e20) {
		    rowLower[iRow]=CoinMax(rowLower[iRow],-value);
		    rowUpper[iRow]=CoinMin(rowUpper[iRow],value);
		  }
		}
		int iColumn;
		int numberColumns=lpSolver->numberColumns();
		double * columnLower = lpSolver->columnLower();
		double * columnUpper = lpSolver->columnUpper();
		for (iColumn=0;iColumn<numberColumns;iColumn++) {
		  // leave free ones for now
		  if (columnLower[iColumn]>-1.0e20||
		      columnUpper[iColumn]<1.0e20) {
		    columnLower[iColumn]=CoinMax(columnLower[iColumn],-value);
		    columnUpper[iColumn]=CoinMin(columnUpper[iColumn],value);
		  }
		}
	      } else if (valid==1) {
		abort();
	      } else {
		std::cout<<"enter value for "<<parameters[iParam].name()<<
		  std::endl;
	      }
	    }
	    break;
	  case REALLY_SCALE:
	    if (goodModel) {
	      ClpSimplex newModel(*lpSolver,
				  lpSolver->scalingFlag());
	      printf("model really really scaled\n");
	      *lpSolver=newModel;
	    }
	    break;
	  case HELP:
	    std::cout<<"Coin Solver version "<<CBCVERSION
		     <<", build "<<__DATE__<<std::endl;
	    std::cout<<"Non default values:-"<<std::endl;
	    std::cout<<"Perturbation "<<lpSolver->perturbation()<<" (default 100)"
		     <<std::endl;
	    CoinReadPrintit(
		    "Presolve being done with 5 passes\n\
Dual steepest edge steep/partial on matrix shape and factorization density\n\
Clpnnnn taken out of messages\n\
If Factorization frequency default then done on size of matrix\n\n\
(-)unitTest, (-)netlib or (-)netlibp will do standard tests\n\n\
You can switch to interactive mode at any time so\n\
clp watson.mps -scaling off -primalsimplex\nis the same as\n\
clp watson.mps -\nscaling off\nprimalsimplex"
		    );
  	    break;
	  case SOLUTION:
	    if (goodModel) {
	      // get next field
	      field = CoinReadGetString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
	      }
	      std::string fileName;
	      FILE *fp=NULL;
	      if (field=="-"||field=="EOL"||field=="stdout") {
		// stdout
		fp=stdout;
	      } else if (field=="stderr") {
		// stderr
		fp=stderr;
	      } else {
		if (field[0]=='/'||field[0]=='\\') {
		  fileName = field;
		} else if (field[0]=='~') {
		  char * environ = getenv("HOME");
		  if (environ) {
		    std::string home(environ);
		    field=field.erase(0,1);
		    fileName = home+field;
		  } else {
		    fileName=field;
		  }
		} else {
		  fileName = directory+field;
		}
		fp=fopen(fileName.c_str(),"w");
	      }
	      if (fp) {
		// make fancy later on
		int iRow;
		int numberRows=lpSolver->numberRows();
		int lengthName = lpSolver->lengthNames(); // 0 if no names
		// in general I don't want to pass around massive
		// amounts of data but seems simpler here
		std::vector<std::string> rowNames =
		  *(lpSolver->rowNames());
		std::vector<std::string> columnNames =
		  *(lpSolver->columnNames());

		double * dualRowSolution = lpSolver->dualRowSolution();
		double * primalRowSolution = 
		  lpSolver->primalRowSolution();
		double * rowLower = lpSolver->rowLower();
		double * rowUpper = lpSolver->rowUpper();
		double primalTolerance = lpSolver->primalTolerance();
		char format[6];
		sprintf(format,"%%-%ds",CoinMax(lengthName,8));
		for (iRow=0;iRow<numberRows;iRow++) {
		  int type=0;
		  if (primalRowSolution[iRow]>rowUpper[iRow]+primalTolerance||
		      primalRowSolution[iRow]<rowLower[iRow]-primalTolerance) {
		    fprintf(fp,"** ");
		    type=2;
		  } else if (fabs(primalRowSolution[iRow])>1.0e-8) {
		    type=1;
		  } else if (numberRows<50) {
		    type=3;
		  }
		  if (type) {
		    fprintf(fp,"%7d ",iRow);
		    if (lengthName)
		      fprintf(fp,format,rowNames[iRow].c_str());
		    fprintf(fp,"%15.8g        %15.8g\n",primalRowSolution[iRow],
			    dualRowSolution[iRow]);
		  }
		}
		int iColumn;
		int numberColumns=lpSolver->numberColumns();
		double * dualColumnSolution = 
		  lpSolver->dualColumnSolution();
		double * primalColumnSolution = 
		  lpSolver->primalColumnSolution();
		double * columnLower = lpSolver->columnLower();
		double * columnUpper = lpSolver->columnUpper();
		for (iColumn=0;iColumn<numberColumns;iColumn++) {
		  int type=0;
		  if (primalColumnSolution[iColumn]>columnUpper[iColumn]+primalTolerance||
		      primalColumnSolution[iColumn]<columnLower[iColumn]-primalTolerance) {
		    fprintf(fp,"** ");
		    type=2;
		  } else if (fabs(primalColumnSolution[iColumn])>1.0e-8) {
		    type=1;
		  } else if (numberColumns<50) {
		    type=3;
		  }
		  if (type) {
		    fprintf(fp,"%7d ",iColumn);
		    if (lengthName)
		      fprintf(fp,format,columnNames[iColumn].c_str());
		    fprintf(fp,"%15.8g        %15.8g\n",
			    primalColumnSolution[iColumn],
			    dualColumnSolution[iColumn]);
		  }
		}
		if (fp!=stdout)
		  fclose(fp);
	      } else {
		std::cout<<"Unable to open file "<<fileName<<std::endl;
	      }
	    } else {
	      std::cout<<"** Current model not valid"<<std::endl;
	      
	    }
	    break;
	  default:
	    abort();
	  }
	} 
      } else if (!numberMatches) {
	std::cout<<"No match for "<<field<<" - ? for list of commands"
		 <<std::endl;
      } else if (numberMatches==1) {
	if (!numberQuery) {
	  std::cout<<"Short match for "<<field<<" - completion: ";
	  std::cout<<parameters[firstMatch].matchName()<<std::endl;
	} else if (numberQuery) {
	  std::cout<<parameters[firstMatch].matchName()<<" : ";
	  std::cout<<parameters[firstMatch].shortHelp()<<std::endl;
	  if (numberQuery>=2) 
	    parameters[firstMatch].printLongHelp();
	}
      } else {
	if (!numberQuery) 
	  std::cout<<"Multiple matches for "<<field<<" - possible completions:"
		   <<std::endl;
	else
	  std::cout<<"Completions of "<<field<<":"<<std::endl;
	for ( iParam=0; iParam<numberParameters; iParam++ ) {
	  int match = parameters[iParam].matches(field);
	  if (match&&parameters[iParam].displayThis()) {
	    std::cout<<parameters[iParam].matchName();
	    if (numberQuery>=2) 
	      std::cout<<" : "<<parameters[iParam].shortHelp();
	    std::cout<<std::endl;
	  }
	}
      }
    }
  }
  // By now all memory should be freed
#ifdef DMALLOC
  dmalloc_log_unfreed();
  dmalloc_shutdown();
#endif
  return 0;
}    
