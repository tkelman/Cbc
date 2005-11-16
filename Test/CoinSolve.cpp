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
#define CBCVERSION "1.00.00"

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
#include "OsiRowCutDebugger.hpp"
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

#include "CglPreProcess.hpp"
#include "CglCutGenerator.hpp"
#include "CglGomory.hpp"
#include "CglProbing.hpp"
#include "CglKnapsackCover.hpp"
#include "CglRedSplit.hpp"
#include "CglClique.hpp"
#include "CglFlowCover.hpp"
#include "CglMixedIntegerRounding2.hpp"
#include "CglTwomir.hpp"

#include "CbcModel.hpp"
#include "CbcHeuristic.hpp"
#include "CbcHeuristicLocal.hpp"
#include "CbcHeuristicGreedy.hpp"
#include "CbcHeuristicFPump.hpp"
#include "CbcTreeLocal.hpp"
#include "CbcCompareActual.hpp"
#include  "CbcOrClpParam.hpp"
#include  "CbcCutGenerator.hpp"

#include "OsiClpSolverInterface.hpp"

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
	      ClpSimplex empty, bool doPresolve,int switchOff);
int CbcOrClpRead_mode=1;
FILE * CbcOrClpReadCommand=stdin;
static int * analyze(OsiClpSolverInterface * solverMod, int & numberChanged, double & increment,
                     bool changeInt)
{
  OsiSolverInterface * solver = solverMod->clone();
  if (0) {
    // just get increment
    CbcModel model(*solver);
    model.analyzeObjective();
    double increment2=model.getCutoffIncrement();
    printf("initial cutoff increment %g\n",increment2);
  }
  const double *objective = solver->getObjCoefficients() ;
  const double *lower = solver->getColLower() ;
  const double *upper = solver->getColUpper() ;
  int numberColumns = solver->getNumCols() ;
  int numberRows = solver->getNumRows();
  double direction = solver->getObjSense();
  int iRow,iColumn;

  // Row copy
  CoinPackedMatrix matrixByRow(*solver->getMatrixByRow());
  const double * elementByRow = matrixByRow.getElements();
  const int * column = matrixByRow.getIndices();
  const CoinBigIndex * rowStart = matrixByRow.getVectorStarts();
  const int * rowLength = matrixByRow.getVectorLengths();

  // Column copy
  CoinPackedMatrix  matrixByCol(*solver->getMatrixByCol());
  const double * element = matrixByCol.getElements();
  const int * row = matrixByCol.getIndices();
  const CoinBigIndex * columnStart = matrixByCol.getVectorStarts();
  const int * columnLength = matrixByCol.getVectorLengths();

  const double * rowLower = solver->getRowLower();
  const double * rowUpper = solver->getRowUpper();

  char * ignore = new char [numberRows];
  int * changed = new int[numberColumns];
  int * which = new int[numberRows];
  double * changeRhs = new double[numberRows];
  memset(changeRhs,0,numberRows*sizeof(double));
  memset(ignore,0,numberRows);
  numberChanged=0;
  int numberInteger=0;
  for (iColumn=0;iColumn<numberColumns;iColumn++) {
    if (upper[iColumn] > lower[iColumn]+1.0e-8&&solver->isInteger(iColumn)) 
      numberInteger++;
  }
  bool finished=false;
  while (!finished) {
    int saveNumberChanged = numberChanged;
    for (iRow=0;iRow<numberRows;iRow++) {
      int numberContinuous=0;
      double value1=0.0,value2=0.0;
      bool allIntegerCoeff=true;
      double sumFixed=0.0;
      int jColumn1=-1,jColumn2=-1;
      for (CoinBigIndex j=rowStart[iRow];j<rowStart[iRow]+rowLength[iRow];j++) {
        int jColumn = column[j];
        double value = elementByRow[j];
        if (upper[jColumn] > lower[jColumn]+1.0e-8) {
          if (!solver->isInteger(jColumn)) {
            if (numberContinuous==0) {
              jColumn1=jColumn;
              value1=value;
            } else {
              jColumn2=jColumn;
              value2=value;
            }
            numberContinuous++;
          } else {
            if (fabs(value-floor(value+0.5))>1.0e-12)
              allIntegerCoeff=false;
          }
        } else {
          sumFixed += lower[jColumn]*value;
        }
      }
      double low = rowLower[iRow];
      if (low>-1.0e20) {
        low -= sumFixed;
        if (fabs(low-floor(low+0.5))>1.0e-12)
          allIntegerCoeff=false;
      }
      double up = rowUpper[iRow];
      if (up<1.0e20) {
        up -= sumFixed;
        if (fabs(up-floor(up+0.5))>1.0e-12)
          allIntegerCoeff=false;
      }
      if (numberContinuous==1) {
        // see if really integer
        // This does not allow for complicated cases
        if (low==up) {
          if (fabs(value1)>1.0e-3) {
            value1 = 1.0/value1;
            if (fabs(value1-floor(value1+0.5))<1.0e-12) {
              // integer
              changed[numberChanged++]=jColumn1;
              solver->setInteger(jColumn1);
              if (upper[jColumn1]>1.0e20)
                solver->setColUpper(jColumn1,1.0e20);
              if (lower[jColumn1]<-1.0e20)
                solver->setColLower(jColumn1,-1.0e20);
            }
          }
        } else {
          if (fabs(value1)>1.0e-3) {
            value1 = 1.0/value1;
            if (fabs(value1-floor(value1+0.5))<1.0e-12) {
              // This constraint will not stop it being integer
              ignore[iRow]=1;
            }
          }
        }
      } else if (numberContinuous==2) {
        if (low==up) {
          /* need general theory - for now just look at 2 cases -
             1 - +- 1 one in column and just costs i.e. matching objective
             2 - +- 1 two in column but feeds into G/L row which will try and minimize
          */
          if (fabs(value1)==1.0&&value1*value2==-1.0&&!lower[jColumn1]
              &&!lower[jColumn2]) {
            int n=0;
            int i;
            double objChange=direction*(objective[jColumn1]+objective[jColumn2]);
            double bound = CoinMin(upper[jColumn1],upper[jColumn2]);
            bound = CoinMin(bound,1.0e20);
            for ( i=columnStart[jColumn1];i<columnStart[jColumn1]+columnLength[jColumn1];i++) {
              int jRow = row[i];
              double value = element[i];
              if (jRow!=iRow) {
                which[n++]=jRow;
                changeRhs[jRow]=value;
              }
            }
            for ( i=columnStart[jColumn1];i<columnStart[jColumn1]+columnLength[jColumn1];i++) {
              int jRow = row[i];
              double value = element[i];
              if (jRow!=iRow) {
                if (!changeRhs[jRow]) {
                  which[n++]=jRow;
                  changeRhs[jRow]=value;
                } else {
                  changeRhs[jRow]+=value;
                }
              }
            }
            if (objChange>=0.0) {
              // see if all rows OK
              bool good=true;
              for (i=0;i<n;i++) {
                int jRow = which[i];
                double value = changeRhs[jRow];
                if (value) {
                  value *= bound;
                  if (rowLength[jRow]==1) {
                    if (value>0.0) {
                      double rhs = rowLower[jRow];
                      if (rhs>0.0) {
                        double ratio =rhs/value;
                        if (fabs(ratio-floor(ratio+0.5))>1.0e-12)
                          good=false;
                      }
                    } else {
                      double rhs = rowUpper[jRow];
                      if (rhs<0.0) {
                        double ratio =rhs/value;
                        if (fabs(ratio-floor(ratio+0.5))>1.0e-12)
                          good=false;
                      }
                    }
                  } else if (rowLength[jRow]==2) {
                    if (value>0.0) {
                      if (rowLower[jRow]>-1.0e20)
                        good=false;
                    } else {
                      if (rowUpper[jRow]<1.0e20)
                        good=false;
                    }
                  } else {
                    good=false;
                  }
                }
              }
              if (good) {
                // both can be integer
                changed[numberChanged++]=jColumn1;
                solver->setInteger(jColumn1);
                if (upper[jColumn1]>1.0e20)
                  solver->setColUpper(jColumn1,1.0e20);
                if (lower[jColumn1]<-1.0e20)
                  solver->setColLower(jColumn1,-1.0e20);
                changed[numberChanged++]=jColumn2;
                solver->setInteger(jColumn2);
                if (upper[jColumn2]>1.0e20)
                  solver->setColUpper(jColumn2,1.0e20);
                if (lower[jColumn2]<-1.0e20)
                  solver->setColLower(jColumn2,-1.0e20);
              }
            }
            // clear
            for (i=0;i<n;i++) {
              changeRhs[which[i]]=0.0;
            }
          }
        }
      }
    }
    for (iColumn=0;iColumn<numberColumns;iColumn++) {
      if (upper[iColumn] > lower[iColumn]+1.0e-8&&!solver->isInteger(iColumn)) {
        double value;
        value = upper[iColumn];
        if (value<1.0e20&&fabs(value-floor(value+0.5))>1.0e-12) 
          continue;
        value = lower[iColumn];
        if (value>-1.0e20&&fabs(value-floor(value+0.5))>1.0e-12) 
          continue;
        bool integer=true;
        for (CoinBigIndex j=columnStart[iColumn];j<columnStart[iColumn]+columnLength[iColumn];j++) {
          int iRow = row[j];
          if (!ignore[iRow]) {
            integer=false;
            break;
          }
        }
        if (integer) {
          // integer
          changed[numberChanged++]=iColumn;
          solver->setInteger(iColumn);
          if (upper[iColumn]>1.0e20)
            solver->setColUpper(iColumn,1.0e20);
          if (lower[iColumn]<-1.0e20)
            solver->setColLower(iColumn,-1.0e20);
        }
      }
    }
    finished = numberChanged==saveNumberChanged;
  }
  delete [] which;
  delete [] changeRhs;
  delete [] ignore;
  if (numberInteger)
    printf("%d integer variables",numberInteger);
  if (changeInt) {
    if (numberChanged)
      printf(" and %d variables made integer\n",numberChanged);
    else
      printf("\n");
    delete [] ignore;
    //increment=0.0;
    if (!numberChanged) {
      delete [] changed;
      delete solver;
      return NULL;
    } else {
      for (iColumn=0;iColumn<numberColumns;iColumn++) {
        if (solver->isInteger(iColumn))
          solverMod->setInteger(iColumn);
      }
      delete solver;
      return changed;
    }
  } else {
    if (numberChanged)
      printf(" and %d variables could be made integer\n",numberChanged);
    else
      printf("\n");
    // just get increment
    CbcModel model(*solver);
    model.analyzeObjective();
    double increment2=model.getCutoffIncrement();
    if (increment2>increment) {
      printf("cutoff increment increased from %g to %g\n",increment,increment2);
      increment=increment2;
    }
    delete solver;
    numberChanged=0;
    return NULL;
  }
}
int main (int argc, const char *argv[])
{
  /* Note
     This is meant as a stand-alone executable to do as much of coin as possible. 
     It should only have one solver known to it.
  */
  {
    double time1 = CoinCpuTime(),time2;
    CoinSighandler_t saveSignal=SIG_DFL;
    // register signal handler
    saveSignal = signal(SIGINT,signal_handler);
    // Set up all non-standard stuff
    OsiClpSolverInterface solver1;
    CbcModel model(solver1);
    CbcModel * babModel = NULL;
    model.setNumberBeforeTrust(21);
    int cutPass=-1234567;
    OsiSolverInterface * solver = model.solver();
    OsiClpSolverInterface * clpSolver = dynamic_cast< OsiClpSolverInterface*> (solver);
    ClpSimplex * lpSolver = clpSolver->getModelPtr();
    clpSolver->messageHandler()->setLogLevel(0) ;
    model.messageHandler()->setLogLevel(1);
    
    
    // default action on import
    int allowImportErrors=0;
    int keepImportNames=1;
    int doIdiot=-1;
    int outputFormat=2;
    int slpValue=-1;
    int printOptions=0;
    int printMode=0;
    int presolveOptions=0;
    int doCrash=0;
    int doSprint=-1;
    int doScaling=1;
    // set reasonable defaults
    int preSolve=5;
    int preProcess=1;
    bool preSolveFile=false;
   
    double djFix=1.0e100;
    double gapRatio=1.0e100;
    double tightenFactor=0.0;
    lpSolver->setPerturbation(50);
    lpSolver->messageHandler()->setPrefix(false);
    const char dirsep =  CoinFindDirSeparator();
    std::string directory = (dirsep == '/' ? "./" : ".\\");
    std::string defaultDirectory = directory;
    std::string importFile ="";
    std::string exportFile ="default.mps";
    std::string importBasisFile ="";
    std::string debugFile="";
    double * debugValues = NULL;
    int numberDebugValues = -1;
    int basisHasValues=0;
    std::string exportBasisFile ="default.bas";
    std::string saveFile ="default.prob";
    std::string restoreFile ="default.prob";
    std::string solutionFile ="stdout";
    std::string solutionSaveFile ="solution.file";
#define CBCMAXPARAMETERS 200
    CbcOrClpParam parameters[CBCMAXPARAMETERS];
    int numberParameters ;
    establishParams(numberParameters,parameters) ;
    parameters[whichParam(BASISIN,numberParameters,parameters)].setStringValue(importBasisFile);
    parameters[whichParam(BASISOUT,numberParameters,parameters)].setStringValue(exportBasisFile);
    parameters[whichParam(DEBUG,numberParameters,parameters)].setStringValue(debugFile);
    parameters[whichParam(DIRECTORY,numberParameters,parameters)].setStringValue(directory);
    parameters[whichParam(DUALBOUND,numberParameters,parameters)].setDoubleValue(lpSolver->dualBound());
    parameters[whichParam(DUALTOLERANCE,numberParameters,parameters)].setDoubleValue(lpSolver->dualTolerance());
    parameters[whichParam(EXPORT,numberParameters,parameters)].setStringValue(exportFile);
    parameters[whichParam(IDIOT,numberParameters,parameters)].setIntValue(doIdiot);
    parameters[whichParam(IMPORT,numberParameters,parameters)].setStringValue(importFile);
    int slog = whichParam(SOLVERLOGLEVEL,numberParameters,parameters);
    int log = whichParam(LOGLEVEL,numberParameters,parameters);
    parameters[slog].setIntValue(0);
    parameters[log].setIntValue(1);
    parameters[whichParam(MAXFACTOR,numberParameters,parameters)].setIntValue(lpSolver->factorizationFrequency());
    parameters[whichParam(MAXITERATION,numberParameters,parameters)].setIntValue(lpSolver->maximumIterations());
    parameters[whichParam(OUTPUTFORMAT,numberParameters,parameters)].setIntValue(outputFormat);
    parameters[whichParam(PRESOLVEPASS,numberParameters,parameters)].setIntValue(preSolve);
    parameters[whichParam(PERTVALUE,numberParameters,parameters)].setIntValue(lpSolver->perturbation());
    parameters[whichParam(PRIMALTOLERANCE,numberParameters,parameters)].setDoubleValue(lpSolver->primalTolerance());
    parameters[whichParam(PRIMALWEIGHT,numberParameters,parameters)].setDoubleValue(lpSolver->infeasibilityCost());
    parameters[whichParam(RESTORE,numberParameters,parameters)].setStringValue(restoreFile);
    parameters[whichParam(SAVE,numberParameters,parameters)].setStringValue(saveFile);
    //parameters[whichParam(TIMELIMIT,numberParameters,parameters)].setDoubleValue(1.0e8);
    parameters[whichParam(TIMELIMIT_BAB,numberParameters,parameters)].setDoubleValue(1.0e8);
    parameters[whichParam(SOLUTION,numberParameters,parameters)].setStringValue(solutionFile);
    parameters[whichParam(SAVESOL,numberParameters,parameters)].setStringValue(solutionSaveFile);
    parameters[whichParam(SPRINT,numberParameters,parameters)].setIntValue(doSprint);
    model.setNumberBeforeTrust(5);
    parameters[whichParam(NUMBERBEFORE,numberParameters,parameters)].setIntValue(5);
    parameters[whichParam(MAXNODES,numberParameters,parameters)].setIntValue(model.getMaximumNodes());
    model.setNumberStrong(5);
    parameters[whichParam(STRONGBRANCHING,numberParameters,parameters)].setIntValue(model.numberStrong());
    parameters[whichParam(INFEASIBILITYWEIGHT,numberParameters,parameters)].setDoubleValue(model.getDblParam(CbcModel::CbcInfeasibilityWeight));
    parameters[whichParam(INTEGERTOLERANCE,numberParameters,parameters)].setDoubleValue(model.getDblParam(CbcModel::CbcIntegerTolerance));
    parameters[whichParam(INCREMENT,numberParameters,parameters)].setDoubleValue(model.getDblParam(CbcModel::CbcCutoffIncrement));
    // Set up likely cut generators and defaults
    parameters[whichParam(PREPROCESS,numberParameters,parameters)].setCurrentOption("on");

    CglGomory gomoryGen;
    // try larger limit
    gomoryGen.setLimitAtRoot(512);
    gomoryGen.setLimit(50);
    // set default action (0=off,1=on,2=root)
    int gomoryAction=3;
    parameters[whichParam(GOMORYCUTS,numberParameters,parameters)].setCurrentOption("ifmove");

    CglProbing probingGen;
    probingGen.setUsingObjective(true);
    probingGen.setMaxPass(3);
    probingGen.setMaxPassRoot(3);
    // Number of unsatisfied variables to look at
    probingGen.setMaxProbe(10);
    probingGen.setMaxProbeRoot(50);
    // How far to follow the consequences
    probingGen.setMaxLook(10);
    probingGen.setMaxLookRoot(50);
    probingGen.setMaxLookRoot(10);
    // Only look at rows with fewer than this number of elements
    probingGen.setMaxElements(200);
    probingGen.setRowCuts(3);
    // set default action (0=off,1=on,2=root)
    int probingAction=3;
    parameters[whichParam(PROBINGCUTS,numberParameters,parameters)].setCurrentOption("ifmove");

    CglKnapsackCover knapsackGen;
    // set default action (0=off,1=on,2=root)
    int knapsackAction=3;
    parameters[whichParam(KNAPSACKCUTS,numberParameters,parameters)].setCurrentOption("ifmove");

    CglRedSplit redsplitGen;
    //redsplitGen.setLimit(100);
    // set default action (0=off,1=on,2=root)
    // Off as seems to give some bad cuts
    int redsplitAction=2;
    parameters[whichParam(REDSPLITCUTS,numberParameters,parameters)].setCurrentOption("root");

    CglClique cliqueGen(false,true);
    cliqueGen.setStarCliqueReport(false);
    cliqueGen.setRowCliqueReport(false);
    cliqueGen.setMinViolation(0.1);
    // set default action (0=off,1=on,2=root)
    int cliqueAction=3;
    parameters[whichParam(CLIQUECUTS,numberParameters,parameters)].setCurrentOption("ifmove");

    CglMixedIntegerRounding2 mixedGen;
    // set default action (0=off,1=on,2=root)
    int mixedAction=3;
    parameters[whichParam(MIXEDCUTS,numberParameters,parameters)].setCurrentOption("ifmove");

    CglFlowCover flowGen;
    // set default action (0=off,1=on,2=root)
    int flowAction=3;
    parameters[whichParam(FLOWCUTS,numberParameters,parameters)].setCurrentOption("ifmove");

    CglTwomir twomirGen;
    twomirGen.setMaxElements(250);
    // set default action (0=off,1=on,2=root)
    int twomirAction=2;
    parameters[whichParam(TWOMIRCUTS,numberParameters,parameters)].setCurrentOption("root");

    bool useRounding=true;
    parameters[whichParam(ROUNDING,numberParameters,parameters)].setCurrentOption("on");
    bool useFpump=true;
    bool useGreedy=true;
    parameters[whichParam(GREEDY,numberParameters,parameters)].setCurrentOption("on");
    bool useCombine=true;
    parameters[whichParam(COMBINE,numberParameters,parameters)].setCurrentOption("on");
    bool useLocalTree=false;
    parameters[whichParam(COSTSTRATEGY,numberParameters,parameters)].setCurrentOption("off");
    bool useCosts=false;
    
    // total number of commands read
    int numberGoodCommands=0;
    bool goodModel=false;
    // Set false if user does anything advanced
    bool defaultSettings=true;

    // Hidden stuff for barrier
    int choleskyType = 0;
    int gamma=0;
    int scaleBarrier=0;
    int doKKT=0;
    int crossover=2; // do crossover unless quadratic
    // For names
    int lengthName = 0;
    std::vector<std::string> rowNames;
    std::vector<std::string> columnNames;
    
    std::string field;
    std::cout<<"Coin Cbc and Clp Solver version "<<CBCVERSION
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
	    <<"CoinSolver takes input from arguments ( - switches to stdin)"
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
	if (type==BAB&&goodModel) {
	  // check if any integers
	  if (!lpSolver->integerInformation())
	    type=DUALSIMPLEX;
	}
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
                defaultSettings=false; // user knows what she is doing
		break;
	      case GAPRATIO:
		gapRatio=value;
		break;
	      case TIGHTENFACTOR:
		tightenFactor=value;
                defaultSettings=false; // user knows what she is doing
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
	      else if (parameters[iParam].type()==CUTPASS)
		cutPass = value;
	      else if (parameters[iParam].type()==FPUMPITS)
		{ useFpump = true;parameters[iParam].setIntValue(value);}
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
	      doScaling = 1-action;
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
            case INTPRINT:
              printMode=action;
              break;
              //case ALGORITHM:
	      //algorithm  = action;
              //defaultSettings=false; // user knows what she is doing
              //abort();
	      //break;
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
              defaultSettings=false; // user knows what she is doing
	      gomoryAction = action;
	      break;
	    case PROBINGCUTS:
              defaultSettings=false; // user knows what she is doing
	      probingAction = action;
	      break;
	    case KNAPSACKCUTS:
              defaultSettings=false; // user knows what she is doing
	      knapsackAction = action;
	      break;
	    case REDSPLITCUTS:
              defaultSettings=false; // user knows what she is doing
	      redsplitAction = action;
	      break;
	    case CLIQUECUTS:
              defaultSettings=false; // user knows what she is doing
	      cliqueAction = action;
	      break;
	    case FLOWCUTS:
              defaultSettings=false; // user knows what she is doing
	      flowAction = action;
	      break;
	    case MIXEDCUTS:
              defaultSettings=false; // user knows what she is doing
	      mixedAction = action;
	      break;
	    case TWOMIRCUTS:
              defaultSettings=false; // user knows what she is doing
	      twomirAction = action;
	      break;
	    case ROUNDING:
              defaultSettings=false; // user knows what she is doing
	      useRounding = action;
	      break;
	    case FPUMP:
              defaultSettings=false; // user knows what she is doing
              useFpump=action;
	      break;
            case CUTSSTRATEGY:
	      gomoryAction = action;
	      probingAction = action;
	      knapsackAction = action;
	      cliqueAction = action;
	      flowAction = action;
	      mixedAction = action;
	      twomirAction = action;
              parameters[whichParam(GOMORYCUTS,numberParameters,parameters)].setCurrentOption(action);
              parameters[whichParam(PROBINGCUTS,numberParameters,parameters)].setCurrentOption(action);
              parameters[whichParam(KNAPSACKCUTS,numberParameters,parameters)].setCurrentOption(action);
              if (!action) {
                redsplitAction = action;
                parameters[whichParam(REDSPLITCUTS,numberParameters,parameters)].setCurrentOption(action);
              }
              parameters[whichParam(CLIQUECUTS,numberParameters,parameters)].setCurrentOption(action);
              parameters[whichParam(FLOWCUTS,numberParameters,parameters)].setCurrentOption(action);
              parameters[whichParam(MIXEDCUTS,numberParameters,parameters)].setCurrentOption(action);
              parameters[whichParam(TWOMIRCUTS,numberParameters,parameters)].setCurrentOption(action);
              break;
            case HEURISTICSTRATEGY:
              useRounding = action;
              useGreedy = action;
              useCombine = action;
              //useLocalTree = action;
              useFpump=action;
              parameters[whichParam(ROUNDING,numberParameters,parameters)].setCurrentOption(action);
              parameters[whichParam(GREEDY,numberParameters,parameters)].setCurrentOption(action);
              parameters[whichParam(COMBINE,numberParameters,parameters)].setCurrentOption(action);
              //parameters[whichParam(LOCALTREE,numberParameters,parameters)].setCurrentOption(action);
              parameters[whichParam(FPUMP,numberParameters,parameters)].setCurrentOption(action);
              break;
	    case GREEDY:
              defaultSettings=false; // user knows what she is doing
	      useGreedy = action;
	      break;
	    case COMBINE:
              defaultSettings=false; // user knows what she is doing
	      useCombine = action;
	      break;
	    case LOCALTREE:
              defaultSettings=false; // user knows what she is doing
	      useLocalTree = action;
	      break;
	    case COSTSTRATEGY:
	      if (action!=1&&action!=0) {
		printf("Pseudo costs not implemented yet\n");
	      } else {
                useCosts=action;
	      }
	      break;
	    case PREPROCESS:
	      preProcess = action;
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
          case STRENGTHEN:
            if (goodModel) {
              model.initialSolve();
              // If user made settings then use them
              if (!defaultSettings) {
                OsiSolverInterface * solver = model.solver();
                if (!doScaling)
                  solver->setHintParam(OsiDoScale,false,OsiHintTry);
                OsiClpSolverInterface * si =
                  dynamic_cast<OsiClpSolverInterface *>(solver) ;
                assert (si != NULL);
                // get clp itself
                ClpSimplex * modelC = si->getModelPtr();
                //if (modelC->tightenPrimalBounds()!=0) {
                //std::cout<<"Problem is infeasible!"<<std::endl;
                //break;
                //}
                // bounds based on continuous
                if (tightenFactor) {
                  if (modelC->tightenPrimalBounds(tightenFactor)!=0) {
                    std::cout<<"Problem is infeasible!"<<std::endl;
                    break;
                  }
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
                  int numberFixed=0;
                  for (i=0;i<numberColumns;i++) {
                    if (type[i]) {
                      double value = solution[i];
                      if (value<lower[i]+1.0e-5&&dj[i]>djFix) {
                        solution[i]=lower[i];
                        upper[i]=lower[i];
                        numberFixed++;
                      } else if (value>upper[i]-1.0e-5&&dj[i]<-djFix) {
                        solution[i]=upper[i];
                        lower[i]=upper[i];
                        numberFixed++;
                      }
                    }
                  }
                  printf("%d columns fixed\n",numberFixed);
                }
              }
              // See if we want preprocessing
              OsiSolverInterface * saveSolver=NULL;
              CglPreProcess process;
              delete babModel;
              babModel = new CbcModel(model);
              OsiSolverInterface * solver3 = clpSolver->clone();
              babModel->assignSolver(solver3);
              OsiClpSolverInterface * clpSolver2 = dynamic_cast< OsiClpSolverInterface*> (babModel->solver());
              int numberChanged=0;
              if (clpSolver2->messageHandler()->logLevel())
                clpSolver2->messageHandler()->setLogLevel(1);
              int logLevel = parameters[slog].intValue();
              if (logLevel)
                clpSolver2->messageHandler()->setLogLevel(logLevel);
              lpSolver = clpSolver2->getModelPtr();
              if (lpSolver->factorizationFrequency()==200) {
                // User did not touch preset
                int numberRows = lpSolver->numberRows();
                const int cutoff1=10000;
                const int cutoff2=100000;
                const int base=75;
                const int freq0 = 50;
                const int freq1=200;
                const int freq2=400;
                const int maximum=1000;
                int frequency;
                if (numberRows<cutoff1)
                  frequency=base+numberRows/freq0;
                else if (numberRows<cutoff2)
                  frequency=base+cutoff1/freq0 + (numberRows-cutoff1)/freq1;
                else
                  frequency=base+cutoff1/freq0 + (cutoff2-cutoff1)/freq1 + (numberRows-cutoff2)/freq2;
                lpSolver->setFactorizationFrequency(CoinMin(maximum,frequency));
              }
              time2 = CoinCpuTime();
              totalTime += time2-time1;
              time1 = time2;
              double timeLeft = babModel->getMaximumSeconds();
              if (preProcess&&type==BAB) {
                saveSolver=babModel->solver()->clone();
                /* Do not try and produce equality cliques and
                   do up to 10 passes */
                OsiSolverInterface * solver2 = process.preProcess(*saveSolver,preProcess==3,10);
                if (!solver2) {
                  printf("Pre-processing says infeasible\n");
                  break;
                } else {
                  printf("processed model has %d rows, %d columns and %d elements\n",
                         solver2->getNumRows(),solver2->getNumCols(),solver2->getNumElements());
                }
                //solver2->resolve();
                if (preProcess==2) {
                  OsiClpSolverInterface * clpSolver2 = dynamic_cast< OsiClpSolverInterface*> (solver2);
                  ClpSimplex * lpSolver = clpSolver2->getModelPtr();
                  lpSolver->writeMps("presolved.mps",0,1,lpSolver->optimizationDirection());
                  printf("Preprocessed model (minimization) on presolved.mps\n");
                }
                // we have to keep solver2 so pass clone
                solver2 = solver2->clone();
                babModel->assignSolver(solver2);
                babModel->initialSolve();
                babModel->setMaximumSeconds(timeLeft-(CoinCpuTime()-time1));
              }
              // now tighten bounds
              {
                OsiClpSolverInterface * si =
                  dynamic_cast<OsiClpSolverInterface *>(babModel->solver()) ;
                assert (si != NULL);
                // get clp itself
                ClpSimplex * modelC = si->getModelPtr();
                if (modelC->tightenPrimalBounds()!=0) {
                  std::cout<<"Problem is infeasible!"<<std::endl;
                  break;
                }
                modelC->dual();
              }
              if (debugValues) {
                // for debug
                std::string problemName ;
                babModel->solver()->getStrParam(OsiProbName,problemName) ;
                //babModel->solver()->activateRowCutDebugger(problemName.c_str()) ;
                twomirGen.probname_=strdup(problemName.c_str());
                // checking seems odd
                //redsplitGen.set_given_optsol(babModel->solver()->getRowCutDebuggerAlways()->optimalSolution(),
                //                         babModel->getNumCols());
              }
	      if (useCosts) {
		int numberColumns = babModel->getNumCols();
		int * sort = new int[numberColumns];
		double * dsort = new double[numberColumns];
		int * priority = new int [numberColumns];
		const double * objective = babModel->getObjCoefficients();
		int iColumn;
		int n=0;
		for (iColumn=0;iColumn<numberColumns;iColumn++) {
		  if (babModel->isInteger(iColumn)) {
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
		babModel->passInPriorities( priority,false);
		delete [] priority;
		delete [] sort;
		delete [] dsort;
	      }
              // FPump done first as it only works if no solution
              CbcHeuristicFPump heuristic4(*babModel);
              if (useFpump) {
                heuristic4.setMaximumPasses(parameters[whichParam(FPUMPITS,numberParameters,parameters)].intValue());
                babModel->addHeuristic(&heuristic4);
              }
              CbcRounding heuristic1(*babModel);
              if (useRounding)
                babModel->addHeuristic(&heuristic1) ;
              CbcHeuristicLocal heuristic2(*babModel);
              heuristic2.setSearchType(1);
              if (useCombine)
                babModel->addHeuristic(&heuristic2);
              CbcHeuristicGreedyCover heuristic3(*babModel);
              CbcHeuristicGreedyEquality heuristic3a(*babModel);
              if (useGreedy) {
                babModel->addHeuristic(&heuristic3);
                babModel->addHeuristic(&heuristic3a);
              }
              if (useLocalTree) {
                CbcTreeLocal localTree(babModel,NULL,10,0,0,10000,2000);
                babModel->passInTreeHandler(localTree);
              }
              // add cut generators if wanted
              int switches[20];
              int numberGenerators=0;
              if (probingAction==1) {
                babModel->addCutGenerator(&probingGen,-1,"Probing");
                switches[numberGenerators++]=0;
              } else if (probingAction>=2) {
                babModel->addCutGenerator(&probingGen,-101+probingAction,"Probing");
                switches[numberGenerators++]=0;
              }
              if (gomoryAction==1) {
                babModel->addCutGenerator(&gomoryGen,-1,"Gomory");
                switches[numberGenerators++]=1;
              } else if (gomoryAction>=2) {
                babModel->addCutGenerator(&gomoryGen,-101+gomoryAction,"Gomory");
                switches[numberGenerators++]=-1;
              }
              if (knapsackAction==1) {
                babModel->addCutGenerator(&knapsackGen,-1,"Knapsack");
                switches[numberGenerators++]=0;
              } else if (knapsackAction>=2) {
                babModel->addCutGenerator(&knapsackGen,-101+knapsackAction,"Knapsack");
                switches[numberGenerators++]=0;
              }
              if (redsplitAction==1) {
                babModel->addCutGenerator(&redsplitGen,-1,"Reduce-and-split");
                switches[numberGenerators++]=1;
              } else if (redsplitAction>=2) {
                babModel->addCutGenerator(&redsplitGen,-101+redsplitAction,"Reduce-and-split");
                switches[numberGenerators++]=1;
              }
              if (cliqueAction==1) {
                babModel->addCutGenerator(&cliqueGen,-1,"Clique");
                switches[numberGenerators++]=1;
              } else if (cliqueAction>=2) {
                babModel->addCutGenerator(&cliqueGen,-101+cliqueAction,"Clique");
                switches[numberGenerators++]=-1;
              }
              if (mixedAction==1) {
                babModel->addCutGenerator(&mixedGen,-1,"MixedIntegerRounding2");
                switches[numberGenerators++]=1;
              } else if (mixedAction>=2) {
                babModel->addCutGenerator(&mixedGen,-101+mixedAction,"MixedIntegerRounding2");
                switches[numberGenerators++]=-1;
              }
              if (flowAction==1) {
                babModel->addCutGenerator(&flowGen,-1,"FlowCover");
                switches[numberGenerators++]=1;
              } else if (flowAction>=2) {
                babModel->addCutGenerator(&flowGen,-101+flowAction,"FlowCover");
                switches[numberGenerators++]=1;
              }
              if (twomirAction==1) {
                babModel->addCutGenerator(&twomirGen,-1,"TwoMirCuts");
                switches[numberGenerators++]=1;
              } else if (twomirAction>=2) {
                babModel->addCutGenerator(&twomirGen,-101+twomirAction,"TwoMirCuts");
                switches[numberGenerators++]=1;
              }
              // Say we want timings
              numberGenerators = babModel->numberCutGenerators();
              int iGenerator;
              int cutDepth=
                parameters[whichParam(CUTDEPTH,numberParameters,parameters)].intValue();
              for (iGenerator=0;iGenerator<numberGenerators;iGenerator++) {
                CbcCutGenerator * generator = babModel->cutGenerator(iGenerator);
                int howOften = generator->howOften();
                if (howOften==-98||howOften==-99) 
                  generator->setSwitchOffIfLessThan(switches[iGenerator]);
                generator->setTiming(true);
                if (cutDepth>=0)
                  generator->setWhatDepth(cutDepth) ;
              }
              // Could tune more
              babModel->setMinimumDrop(min(5.0e-2,
                                        fabs(babModel->getMinimizationObjValue())*1.0e-3+1.0e-4));
              if (cutPass==-1234567) {
                if (babModel->getNumCols()<500)
                  babModel->setMaximumCutPassesAtRoot(-100); // always do 100 if possible
                else if (babModel->getNumCols()<5000)
                  babModel->setMaximumCutPassesAtRoot(100); // use minimum drop
                else
                  babModel->setMaximumCutPassesAtRoot(20);
              } else {
                babModel->setMaximumCutPassesAtRoot(cutPass);
              }
              babModel->setMaximumCutPasses(1);
              
              // Do more strong branching if small
              //if (babModel->getNumCols()<5000)
              //babModel->setNumberStrong(20);
              // Switch off strong branching if wanted
              //if (babModel->getNumCols()>10*babModel->getNumRows())
              //babModel->setNumberStrong(0);
              babModel->messageHandler()->setLogLevel(parameters[log].intValue());
              if (babModel->getNumCols()>2000||babModel->getNumRows()>1500||
                  babModel->messageHandler()->logLevel()>1)
                babModel->setPrintFrequency(100);
              
              babModel->solver()->setIntParam(OsiMaxNumIterationHotStart,100);
              OsiClpSolverInterface * osiclp = dynamic_cast< OsiClpSolverInterface*> (babModel->solver());
              // go faster stripes
              if (osiclp->getNumRows()<300&&osiclp->getNumCols()<500) {
                osiclp->setupForRepeatedUse(2,0);
              } else {
                osiclp->setupForRepeatedUse(0,0);
              }
              double increment=babModel->getCutoffIncrement();;
              int * changed = analyze( osiclp,numberChanged,increment,false);
              if (debugValues) {
                if (numberDebugValues==babModel->getNumCols()) {
                  // for debug
                  babModel->solver()->activateRowCutDebugger(debugValues) ;
                } else {
                  printf("debug file has incorrect number of columns\n");
                }
              }
              babModel->setCutoffIncrement(CoinMax(babModel->getCutoffIncrement(),increment));
              // Turn this off if you get problems
              // Used to be automatically set
              osiclp->setSpecialOptions(osiclp->specialOptions()|(128+64)|1);
              if (gapRatio < 1.0e100) {
                double value = babModel->solver()->getObjValue() ;
                double value2 = gapRatio*(1.0e-5+fabs(value)) ;
                babModel->setAllowableGap(value2) ;
                std::cout << "Continuous " << value
                          << ", so allowable gap set to "
                          << value2 << std::endl ;
              }
              // probably faster to use a basis to get integer solutions
              babModel->setSpecialOptions(2);
              currentBranchModel = babModel;
              OsiSolverInterface * strengthenedModel=NULL;
              if (type==BAB) {
                babModel->branchAndBound();
              } else {
                strengthenedModel = babModel->strengthenedModel();
              }
              currentBranchModel = NULL;
              osiclp = dynamic_cast< OsiClpSolverInterface*> (babModel->solver());
              if (debugFile=="createAfterPre"&&babModel->bestSolution()) {
                lpSolver = osiclp->getModelPtr();
                //move best solution (should be there -- but ..)
                int n = lpSolver->getNumCols();
                memcpy(lpSolver->primalColumnSolution(),babModel->bestSolution(),n*sizeof(double));
                saveSolution(osiclp->getModelPtr(),"debug.file");
              }
              // Print more statistics
              std::cout<<"Cuts at root node changed objective from "<<babModel->getContinuousObjective()
                       <<" to "<<babModel->rootObjectiveAfterCuts()<<std::endl;
              
              for (iGenerator=0;iGenerator<numberGenerators;iGenerator++) {
                CbcCutGenerator * generator = babModel->cutGenerator(iGenerator);
                std::cout<<generator->cutGeneratorName()<<" was tried "
                         <<generator->numberTimesEntered()<<" times and created "
                         <<generator->numberCutsInTotal()<<" cuts of which "
                         <<generator->numberCutsActive()<<" were active after adding rounds of cuts";
                if (generator->timing())
                  std::cout<<" ( "<<generator->timeInCutGenerator()<<" seconds)"<<std::endl;
                else
                  std::cout<<std::endl;
              }
              time2 = CoinCpuTime();
              totalTime += time2-time1;
              // For best solution
              double * bestSolution = NULL;
              if (babModel->getMinimizationObjValue()<1.0e50&&type==BAB) {
                // post process
                if (preProcess) {
                  int n = saveSolver->getNumCols();
                  bestSolution = new double [n];
                  process.postProcess(*babModel->solver());
                  // Solution now back in saveSolver
                  babModel->assignSolver(saveSolver);
                  memcpy(bestSolution,babModel->solver()->getColSolution(),n*sizeof(double));
                } else {
                  int n = babModel->solver()->getNumCols();
                  bestSolution = new double [n];
                  memcpy(bestSolution,babModel->solver()->getColSolution(),n*sizeof(double));
                }
              }
              if (type==STRENGTHEN&&strengthenedModel)
                clpSolver = dynamic_cast< OsiClpSolverInterface*> (strengthenedModel);
              lpSolver = clpSolver->getModelPtr();
              if (debugFile=="create"&&bestSolution) {
                saveSolution(lpSolver,"debug.file");
              }
              if (numberChanged) {
                for (int i=0;i<numberChanged;i++) {
                  int iColumn=changed[i];
                  clpSolver->setContinuous(iColumn);
                }
                delete [] changed;
              }
              if (type==BAB) {
                //move best solution (should be there -- but ..)
                int n = lpSolver->getNumCols();
                if (bestSolution)
                  memcpy(lpSolver->primalColumnSolution(),bestSolution,n*sizeof(double));
                delete [] bestSolution;
                std::string statusName[]={"Finished","Stopped on ","Difficulties",
                                          "","","User ctrl-c"};
                std::string minor[]={"","","gap","nodes","time","","solutions"};
                int iStat = babModel->status();
                int iStat2 = babModel->secondaryStatus();
                std::cout<<"Result - "<<statusName[iStat]<<minor[iStat2]
                         <<" objective "<<babModel->getObjValue()<<
                  " after "<<babModel->getNodeCount()<<" nodes and "
                         <<babModel->getIterationCount()<<
                  " iterations - took "<<time2-time1<<" seconds"<<std::endl;
              } else {
                std::cout<<"Model strengthend - now has "<<clpSolver->getNumRows()
                         <<" rows"<<std::endl;
              }
              time1 = time2;
              delete babModel;
              babModel=NULL;
            } else {
              std::cout << "** Current model not valid" << std::endl ; 
            }
            break ;
	  case IMPORT:
	    {
              delete babModel;
              babModel=NULL;
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
                bool absolutePath;
                if (dirsep=='/') {
                  // non Windows (or cygwin)
                  absolutePath=(field[0]=='/');
                } else {
                  //Windows (non cycgwin)
                  absolutePath=(field[0]=='\\');
                  // but allow for :
                  if (strchr(field.c_str(),':'))
                    absolutePath=true;
                }
		if (absolutePath) {
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
                  if (keepImportNames) {
                    lengthName = lpSolver->lengthNames();
                    rowNames = *(lpSolver->rowNames());
                    columnNames = *(lpSolver->columnNames());
                  } else {
                    lengthName=0;
                  }
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
	  case DEBUG:
	    if (goodModel) {
              delete [] debugValues;
              debugValues=NULL;
	      // get next field
	      field = CoinReadGetString(argc,argv);
	      if (field=="$") {
		field = parameters[iParam].stringValue();
	      } else if (field=="EOL") {
		parameters[iParam].printString();
		break;
	      } else {
		parameters[iParam].setStringValue(field);
                debugFile=field;
                if (debugFile=="create"||
                    debugFile=="createAfterPre") {
                  printf("Will create a debug file so this run should be a good one\n");
                  break;
                }
	      }
	      std::string fileName;
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
                int numRows;
                double obj;
                fread(&numRows,sizeof(int),1,fp);
                fread(&numberDebugValues,sizeof(int),1,fp);
                fread(&obj,sizeof(double),1,fp);
                debugValues = new double[numberDebugValues+numRows];
                fread(debugValues,sizeof(double),numRows,fp);
                fread(debugValues,sizeof(double),numRows,fp);
                fread(debugValues,sizeof(double),numberDebugValues,fp);
                printf("%d doubles read into debugValues\n",numberDebugValues);
                fclose(fp);
              } else {
                std::cout<<"Unable to open file "<<fileName<<std::endl;
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
	  case NETLIB_EITHER:
	  case NETLIB_BARRIER:
	  case NETLIB_PRIMAL:
	  case NETLIB_TUNE:
	    {
	      // create fields for unitTest
	      const char * fields[4];
	      int nFields=2;
	      fields[0]="fake main from unitTest";
	      fields[1]="-netlib";
	      if (directory!=defaultDirectory) {
		fields[2]="-netlibDir";
		fields[3]=directory.c_str();
		nFields=4;
	      }
	      int algorithm;
	      if (type==NETLIB_DUAL) {
		std::cerr<<"Doing netlib with dual algorithm"<<std::endl;
		algorithm =0;
	      } else if (type==NETLIB_BARRIER) {
		std::cerr<<"Doing netlib with barrier algorithm"<<std::endl;
		algorithm =2;
	      } else if (type==NETLIB_EITHER) {
		std::cerr<<"Doing netlib with dual or primal algorithm"<<std::endl;
		algorithm =3;
	      } else if (type==NETLIB_TUNE) {
		std::cerr<<"Doing netlib with best algorithm!"<<std::endl;
		algorithm =5;
                // uncomment next to get active tuning
                // algorithm=6;
	      } else {
		std::cerr<<"Doing netlib with primal agorithm"<<std::endl;
		algorithm=1;
	      }
              int specialOptions = lpSolver->specialOptions();
              lpSolver->setSpecialOptions(0);
	      mainTest(nFields,fields,algorithm,*lpSolver,
		       (preSolve!=0),specialOptions);
	    }
	    break;
	  case UNITTEST:
	    {
	      // create fields for unitTest
	      const char * fields[3];
	      int nFields=1;
	      fields[0]="fake main from unitTest";
	      if (directory!=defaultDirectory) {
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
		double * dualRowSolution = lpSolver->dualRowSolution();
		double * primalRowSolution = 
		  lpSolver->primalRowSolution();
		double * rowLower = lpSolver->rowLower();
		double * rowUpper = lpSolver->rowUpper();
		double primalTolerance = lpSolver->primalTolerance();
		char format[6];
		sprintf(format,"%%-%ds",CoinMax(lengthName,8));
                if (printMode>2) {
                  for (iRow=0;iRow<numberRows;iRow++) {
                    int type=printMode-3;
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
                }
		int iColumn;
		int numberColumns=lpSolver->numberColumns();
		double * dualColumnSolution = 
		  lpSolver->dualColumnSolution();
		double * primalColumnSolution = 
		  lpSolver->primalColumnSolution();
		double * columnLower = lpSolver->columnLower();
		double * columnUpper = lpSolver->columnUpper();
                if (printMode!=2) {
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
                    // see if integer
                    if ((!lpSolver->isInteger(iColumn)||fabs(primalColumnSolution[iColumn])<1.0e-8)
                         &&printMode==1)
                      type=0;
                    if (type) {
                      fprintf(fp,"%7d ",iColumn);
                      if (lengthName)
                        fprintf(fp,format,columnNames[iColumn].c_str());
                      fprintf(fp,"%15.8g        %15.8g\n",
                              primalColumnSolution[iColumn],
                              dualColumnSolution[iColumn]);
                    }
                  }
                } else {
                  // special format suitable for OsiRowCutDebugger
                  int n=0;
                  bool comma=false;
                  bool newLine=false;
                  fprintf(fp,"\tint intIndicesV[]={\n");
                  for (iColumn=0;iColumn<numberColumns;iColumn++) {
                    if(primalColumnSolution[iColumn]>0.5&&model.solver()->isInteger(iColumn)) {
                      if (comma)
                        fprintf(fp,",");
                      if (newLine)
                        fprintf(fp,"\n");
                      fprintf(fp,"%d ",iColumn);
                      comma=true;
                      newLine=false;
                      n++;
                      if (n==10) {
                        n=0;
                        newLine=true;
                      }
                    }
                  }
                  fprintf(fp,"};\n");
                  n=0;
                  comma=false;
                  newLine=false;
                  fprintf(fp,"\tdouble intSolnV[]={\n");
                  for ( iColumn=0;iColumn<numberColumns;iColumn++) {
                    if(primalColumnSolution[iColumn]>0.5&&model.solver()->isInteger(iColumn)) {
                      if (comma)
                        fprintf(fp,",");
                      if (newLine)
                        fprintf(fp,"\n");
                      int value = (int) (primalColumnSolution[iColumn]+0.5);
                      fprintf(fp,"%d. ",value);
                      comma=true;
                      newLine=false;
                      n++;
                      if (n==10) {
                        n=0;
                        newLine=true;
                      }
                    }
                  }
                  fprintf(fp,"};\n");
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
	  case SAVESOL:
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
              saveSolution(lpSolver,fileName);
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
/*
  Version 1.00.00 November 16 2005.
  This is to stop me (JJF) messing about too much.
  Tuning changes should be noted here.
  The testing next version may be activated by CBC_NEXT_VERSION
  This applies to OsiClp, Clp etc
*/
