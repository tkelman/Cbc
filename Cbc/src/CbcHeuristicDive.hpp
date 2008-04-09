// Copyright (C) 2008, International Business Machines
// Corporation and others.  All Rights Reserved.
#ifndef CbcHeuristicDive_H
#define CbcHeuristicDive_H

#include "CbcHeuristic.hpp"

/** Dive class
 */

class CbcHeuristicDive : public CbcHeuristic {
public:

  // Default Constructor 
  CbcHeuristicDive ();

  // Constructor with model - assumed before cuts
  CbcHeuristicDive (CbcModel & model);
  
  // Copy constructor 
  CbcHeuristicDive ( const CbcHeuristicDive &);
   
  // Destructor 
  ~CbcHeuristicDive ();

  /// Clone
  virtual CbcHeuristicDive * clone() const = 0;
  
  /// Assignment operator 
  CbcHeuristicDive & operator=(const CbcHeuristicDive& rhs);

  /// Create C++ lines to get to current state
  virtual void generateCpp( FILE * fp) {}

  /// Create C++ lines to get to current state - does work for base class
  void generateCpp( FILE * fp,const char * heuristic);

  /// Resets stuff if model changes
  virtual void resetModel(CbcModel * model);

  /// update model (This is needed if cliques update matrix etc)
  virtual void setModel(CbcModel * model);
  
  //  REMLOVE using CbcHeuristic::solution ;
  /** returns 0 if no solution, 1 if valid solution
      with better objective value than one passed in
      Sets solution values if good, sets objective value (only if good)
      This is called after cuts have been added - so can not add cuts
      This does Fractional Diving
  */
  virtual int solution(double & objectiveValue,
		       double * newSolution);

  /// Validate model i.e. sets when_ to 0 if necessary (may be NULL)
  virtual void validate();

  /// Set percentage of integer variables to fix at bounds
  void setPercentageToFix(double value)
  { percentageToFix_ = value; }

  /// Set maximum number of iterations
  void setMaxIterations(int value)
  { maxIterations_ = value; }

  /// Set maximum time allowed
  void setMaxTime(double value)
  { maxTime_ = value; }

  /// Tests if the heuristic can run
  virtual bool canHeuristicRun();

  /// Selects the next variable to branch on
  virtual void selectVariableToBranch(OsiSolverInterface* solver,
				      const double* newSolution,
				      int& bestColumn,
				      int& bestRound) = 0;

protected:
  // Data

  // Original matrix by column
  CoinPackedMatrix matrix_;

  // Down locks
  unsigned short * downLocks_;

  // Up locks
  unsigned short * upLocks_;

  // Percentage of integer variables to fix at bounds
  double percentageToFix_;

  // Maximum number of iterations
  int maxIterations_;

  // Maximum time allowed
  double maxTime_;

};

#endif
