// Copyright (C) 2006, International Business Machines
// Corporation and others.  All Rights Reserved.
#ifndef CglLinked_H
#define CglLinked_H
/* THIS CONTAINS STUFF THAT SHOULD BE IN
   OsiSolverLink
   OsiBranchLink
   CglTemporary
*/
#ifdef COIN_HAS_ASL
#define COIN_HAS_LINK
#endif
#ifdef COIN_HAS_LINK
#include "OsiClpSolverInterface.hpp"
#include "CoinModel.hpp"
class CbcModel;
class CoinPackedMatrix;
class OsiLinkedBound;
class OsiObject;
//#############################################################################

/**
   
This is to allow the user to replace initialSolve and resolve
This version changes coefficients
*/

class OsiSolverLink : public OsiClpSolverInterface {
  
public:
  //---------------------------------------------------------------------------
  /**@name Solve methods */
  //@{
  /// Solve initial LP relaxation 
  virtual void initialSolve();
  
  /// Resolve an LP relaxation after problem modification
  virtual void resolve();

  /**
     Problem specific 
     Returns -1 if node fathomed and no solution
              0 if did nothing
	      1 if node fathomed and solution
     allFixed is true if all LinkedBound variables are fixed
  */
  virtual int fathom(bool allFixed) {return 0;};
  /** Solves nonlinear problem from CoinModel using SLP - may be used as crash
      for other algorithms when number of iterations small.
      Also exits if all problematical variables are changing
      less than deltaTolerance
      Returns solution array
  */
  double * nonlinearSLP(int numberPasses,double deltaTolerance);
  //@}
  
  
  /**@name Constructors and destructors */
  //@{
  /// Default Constructor
  OsiSolverLink ();
  
  /** This creates from a coinModel object 

      if errors.then number of sets is -1
      
      This creates linked ordered sets information.  It assumes -

      for product terms syntax is yy*f(zz)
      also just f(zz) is allowed 
      and even a constant

      modelObject not const as may be changed as part of process.
  */
  OsiSolverLink(  CoinModel & modelObject);
  // Other way with existing object
  void load(  CoinModel & modelObject,bool tightenBounds=false);
  /// Clone
  virtual OsiSolverInterface * clone(bool copyData=true) const;
  
  /// Copy constructor 
  OsiSolverLink (const OsiSolverLink &);
  
  /// Assignment operator 
  OsiSolverLink & operator=(const OsiSolverLink& rhs);
  
  /// Destructor 
  virtual ~OsiSolverLink ();
  
  //@}
  
  
  /**@name Sets and Gets */
  //@{
  /// Add a bound modifier 
  void addBoundModifier(bool upperBoundAffected, bool useUpperBound, int whichVariable, int whichVariableAffected, 
			double multiplier=1.0);
  /// Update coefficients - returns number updated if in updating mode
  int updateCoefficients(ClpSimplex * solver, CoinPackedMatrix * matrix);
  /// Analyze constraints to see which are convex (quadratic)
  void analyzeObjects();
  /// Objective value of best solution found internally
  inline double bestObjectiveValue() const
  { return bestObjectiveValue_;};
  /// Set objective value of best solution found internally
  inline void setBestObjectiveValue(double value)
  { bestObjectiveValue_ = value;};
  /// Best solution found internally
  inline const double * bestSolution() const
  { return bestSolution_;};
  /// Set best solution found internally
  void setBestSolution(const double * solution, int numberColumns);
  /// Set special options
  inline void setSpecialOptions2(int value)
  { specialOptions2_=value;};
  /// Say convex (should work it out)
  inline void sayConvex()
  { specialOptions2_ |= 4;};
  /// Get special options
  inline int specialOptions2() const
  { return specialOptions2_;};
  /** Clean copy of matrix
      So we can add rows
  */
  CoinPackedMatrix * cleanMatrix() const
  { return matrix_;};
  /** Row copy of matrix 
      Just genuine columns and rows
      Linear part
  */
  CoinPackedMatrix * originalRowCopy() const
  { return originalRowCopy_;};
  /// Copy of quadratic model if one
  ClpSimplex * quadraticModel() const
  { return quadraticModel_;};
  /// Default meshSize 
  inline double defaultMeshSize() const
  { return defaultMeshSize_;};
  inline void setDefaultMeshSize(double value)
  { defaultMeshSize_=value;};
  /// Default maximumbound 
  inline double defaultBound() const
  { return defaultBound_;};
  inline void setDefaultBound(double value)
  { defaultBound_=value;};
  /// Set integer priority
  inline void setIntegerPriority(int value)
  { integerPriority_=value;};
  /// Get integer priority
  inline int integerPriority() const
  { return integerPriority_;};
  /// Set biLinear priority
  inline void setBiLinearPriority(int value)
  { biLinearPriority_=value;};
  /// Get biLinear priority
  inline int biLinearPriority() const
  { return biLinearPriority_;};
  /// Set Cbc Model
  inline void setCbcModel(CbcModel * model)
  { cbcModel_=model;};
  /// Return CoinModel
  inline const CoinModel * coinModel() const
  { return &coinModel_;};
  //@}
  
  //---------------------------------------------------------------------------
  
protected:
  
  
  /**@name functions */
  //@{
  /// Do real work of initialize
  //void initialize(ClpSimplex * & solver, OsiObject ** & object) const;
  /// Do real work of delete
  void gutsOfDestructor(bool justNullify=false);
  /// Do real work of copy
  void gutsOfCopy(const OsiSolverLink & rhs) ;
  //@}
  
  /**@name Private member data */
  //@{
  /** Clean copy of matrix
      Marked coefficients will be multiplied by L or U
  */
  CoinPackedMatrix * matrix_;
  /** Row copy of matrix 
      Just genuine columns and rows
  */
  CoinPackedMatrix * originalRowCopy_;
  /// Copy of quadratic model if one
  ClpSimplex * quadraticModel_;
  /// Pointer back to CbcModel
  CbcModel * cbcModel_;
  /// Number of rows with nonLinearities
  int numberNonLinearRows_;
  /// Starts of lists
  int * startNonLinear_;
  /// Row number for a list
  int * rowNonLinear_;
  /** Indicator whether is convex, concave or neither
      -1 concave, 0 neither, +1 convex
  */
  int * convex_;
  /// Indices in a list/row
  int * whichNonLinear_;
  /// Model in CoinModel format
  CoinModel coinModel_;
  /// Number of variables in tightening phase
  int numberVariables_;
  /// Information
  OsiLinkedBound * info_;
  /**
     0 bit (1) - don't do mini B&B
     1 bit (2) - quadratic only in objective (add OA cuts)
     2 bit (4) - convex
     4 bit (8) - try adding OA cuts
  */
  int specialOptions2_;
  /// Objective transfer row if one
  int objectiveRow_;
  /// Objective transfer variable if one
  int objectiveVariable_;
  /// Objective value of best solution found internally
  double bestObjectiveValue_;
  /// Default mesh
  double defaultMeshSize_;
  /// Default maximum bound
  double defaultBound_;
  /// Best solution found internally
  double * bestSolution_;
  /// Priority for integers
  int integerPriority_;
  /// Priority for bilinear
  int biLinearPriority_;
  //@}
};
/**
   List of bounds which depend on other bounds
*/

class OsiLinkedBound {
  
public:
  //---------------------------------------------------------------------------
  /**@name Action methods */
  //@{
  /// Update other bounds
  void updateBounds(ClpSimplex * solver);
  //@}
  
  
  /**@name Constructors and destructors */
  //@{
  /// Default Constructor
  OsiLinkedBound ();
  /// Useful Constructor
  OsiLinkedBound(OsiSolverInterface * model, int variable,
		 int numberAffected, const int * positionL, 
		 const int * positionU, const double * multiplier);
  
  /// Copy constructor 
  OsiLinkedBound (const OsiLinkedBound &);
  
  /// Assignment operator 
  OsiLinkedBound & operator=(const OsiLinkedBound& rhs);
  
  /// Destructor 
  ~OsiLinkedBound ();
  
  //@}
  
  /**@name Sets and Gets */
  //@{
  /// Get variable
  inline int variable() const
  { return variable_;};
  /// Add a bound modifier
  void addBoundModifier(bool upperBoundAffected, bool useUpperBound, int whichVariable, 
			double multiplier=1.0);
  //@}
  
private:
  typedef struct {
    /*
      0 - LB of variable affected
      1 - UB of variable affected
      2 - element in position (affected) affected
    */
    unsigned int affect:2;
    unsigned int ubUsed:1; // nonzero if UB of this variable is used
    /* 
       0 - use x*multiplier
       1 - use multiplier/x
       2 - if UB use min of current upper and x*multiplier, if LB use max of current lower and x*multiplier
    */
    unsigned int type:4; // type of computation
    unsigned int affected:25; // variable or element affected
    float multiplier; // to use in computation 
  } boundElementAction;
  
  /**@name Private member data */
  //@{
  /// Pointer back to model 
  OsiSolverInterface * model_;
  /// Variable
  int variable_;
  /// Number of variables/elements affected
  int numberAffected_;
  /// Maximum number of variables/elements affected
  int maximumAffected_;
  /// Actions
  boundElementAction * affected_;
  //@}
};
#include "CbcHeuristic.hpp"
/** heuristic - just picks up any good solution
 */

class CbcHeuristicDynamic3 : public CbcHeuristic {
public:
  
  // Default Constructor 
  CbcHeuristicDynamic3 ();
  
  /* Constructor with model
   */
  CbcHeuristicDynamic3 (CbcModel & model);
  
  // Copy constructor 
  CbcHeuristicDynamic3 ( const CbcHeuristicDynamic3 &);
  
  // Destructor 
  ~CbcHeuristicDynamic3 ();
  
  /// Clone
  virtual CbcHeuristic * clone() const;
  
  /// update model
  virtual void setModel(CbcModel * model);
  
  /** returns 0 if no solution, 1 if valid solution.
      Sets solution values if good, sets objective value (only if good)
      We leave all variables which are at one at this node of the
      tree to that value and will
      initially set all others to zero.  We then sort all variables in order of their cost
      divided by the number of entries in rows which are not yet covered.  We randomize that
      value a bit so that ties will be broken in different ways on different runs of the heuristic.
      We then choose the best one and set it to one and repeat the exercise.  
      
  */
  virtual int solution(double & objectiveValue,
		       double * newSolution);
  /// Resets stuff if model changes
  virtual void resetModel(CbcModel * model);
  /// Returns true if can deal with "odd" problems e.g. sos type 2
  virtual bool canDealWithOdd() const
  { return true;};
  
protected:
private:
  /// Illegal Assignment operator 
  CbcHeuristicDynamic3 & operator=(const CbcHeuristicDynamic3& rhs);
};

#include "OsiBranchingObject.hpp"

/** Define Special Linked Ordered Sets.

*/
class CoinWarmStartBasis;

class OsiOldLink : public OsiSOS {

public:

  // Default Constructor 
  OsiOldLink ();

  /** Useful constructor - A valid solution is if all variables are zero
      apart from k*numberLink to (k+1)*numberLink-1 where k is 0 through
      numberInSet-1.  The length of weights array is numberInSet.
      For this constructor the variables in matrix are the numberInSet*numberLink
      starting at first. If weights null then 0,1,2..
  */
  OsiOldLink (const OsiSolverInterface * solver, int numberMembers,
           int numberLinks, int first,
           const double * weights, int setNumber);
  /** Useful constructor - A valid solution is if all variables are zero
      apart from k*numberLink to (k+1)*numberLink-1 where k is 0 through
      numberInSet-1.  The length of weights array is numberInSet.
      For this constructor the variables are given by list - grouped.
      If weights null then 0,1,2..
  */
  OsiOldLink (const OsiSolverInterface * solver, int numberMembers,
           int numberLinks, int typeSOS, const int * which,
           const double * weights, int setNumber);
  
  // Copy constructor 
  OsiOldLink ( const OsiOldLink &);
   
  /// Clone
  virtual OsiObject * clone() const;

  // Assignment operator 
  OsiOldLink & operator=( const OsiOldLink& rhs);

  // Destructor 
  virtual ~OsiOldLink ();
  
  /// Infeasibility - large is 0.5
  virtual double infeasibility(const OsiBranchingInformation * info,int & whichWay) const;

  /** Set bounds to fix the variable at the current (integer) value.

    Given an integer value, set the lower and upper bounds to fix the
    variable. Returns amount it had to move variable.
  */
  virtual double feasibleRegion(OsiSolverInterface * solver, const OsiBranchingInformation * info) const;

  /** Creates a branching object

    The preferred direction is set by \p way, 0 for down, 1 for up.
  */
  virtual OsiBranchingObject * createBranch(OsiSolverInterface * solver, const OsiBranchingInformation * info, int way) const;

  /// Redoes data when sequence numbers change
  virtual void resetSequenceEtc(int numberColumns, const int * originalColumns);

  /// Number of links for each member
  inline int numberLinks() const
  {return numberLinks_;};

  /** \brief Return true if object can take part in normal heuristics
  */
  virtual bool canDoHeuristics() const 
  {return false;};
  /** \brief Return true if branch should only bound variables
  */
  virtual bool boundBranch() const 
  {return false;};

private:
  /// data

  /// Number of links
  int numberLinks_;
};
/** Branching object for Linked ordered sets

 */
class OsiOldLinkBranchingObject : public OsiSOSBranchingObject {

public:

  // Default Constructor 
  OsiOldLinkBranchingObject ();

  // Useful constructor
  OsiOldLinkBranchingObject (OsiSolverInterface * solver,  const OsiOldLink * originalObject,
                          int way,
                          double separator);
  
  // Copy constructor 
  OsiOldLinkBranchingObject ( const OsiOldLinkBranchingObject &);
   
  // Assignment operator 
  OsiOldLinkBranchingObject & operator=( const OsiOldLinkBranchingObject& rhs);

  /// Clone
  virtual OsiBranchingObject * clone() const;

  // Destructor 
  virtual ~OsiOldLinkBranchingObject ();
  
  /// Does next branch and updates state
  virtual double branch(OsiSolverInterface * solver);

  /** \brief Print something about branch - only if log level high
  */
  virtual void print(const OsiSolverInterface * solver=NULL);
private:
  /// data
};
/** Define data for one link
    
*/


class OsiOneLink {

public:

  // Default Constructor 
  OsiOneLink ();

  /** Useful constructor - 
      
  */
  OsiOneLink (const OsiSolverInterface * solver, int xRow, int xColumn, int xyRow,
	      const char * functionString);
  
  // Copy constructor 
  OsiOneLink ( const OsiOneLink &);
   
  // Assignment operator 
  OsiOneLink & operator=( const OsiOneLink& rhs);

  // Destructor 
  virtual ~OsiOneLink ();
  
  /// data

  /// Row which defines x (if -1 then no x)
  int xRow_;
  /// Column which defines x
  int xColumn_;
  /// Output row
  int xyRow;
  /// Function
  std::string function_;
};
/** Define Special Linked Ordered Sets. New style

    members and weights may be stored in SOS object

    This is for y and x*f(y) and z*g(y) etc

*/


class OsiLink : public OsiSOS {

public:

  // Default Constructor 
  OsiLink ();

  /** Useful constructor -
      
  */
  OsiLink (const OsiSolverInterface * solver, int yRow,
	   int yColumn, double meshSize);
  
  // Copy constructor 
  OsiLink ( const OsiLink &);
   
  /// Clone
  virtual OsiObject * clone() const;

  // Assignment operator 
  OsiLink & operator=( const OsiLink& rhs);

  // Destructor 
  virtual ~OsiLink ();
  
  /// Infeasibility - large is 0.5
  virtual double infeasibility(const OsiBranchingInformation * info,int & whichWay) const;

  /** Set bounds to fix the variable at the current (integer) value.

    Given an integer value, set the lower and upper bounds to fix the
    variable. Returns amount it had to move variable.
  */
  virtual double feasibleRegion(OsiSolverInterface * solver, const OsiBranchingInformation * info) const;

  /** Creates a branching object

    The preferred direction is set by \p way, 0 for down, 1 for up.
  */
  virtual OsiBranchingObject * createBranch(OsiSolverInterface * solver, const OsiBranchingInformation * info, int way) const;

  /// Redoes data when sequence numbers change
  virtual void resetSequenceEtc(int numberColumns, const int * originalColumns);

  /// Number of links for each member
  inline int numberLinks() const
  {return numberLinks_;};

  /** \brief Return true if object can take part in normal heuristics
  */
  virtual bool canDoHeuristics() const 
  {return false;};
  /** \brief Return true if branch should only bound variables
  */
  virtual bool boundBranch() const 
  {return false;};

private:
  /// data
  /// Current increment for y points
  double meshSize_;
  /// Links
  OsiOneLink * data_;
  /// Number of links
  int numberLinks_;
  /// Row which defines y
  int yRow_;
  /// Column which defines y
  int yColumn_;
};
/** Branching object for Linked ordered sets

 */
class OsiLinkBranchingObject : public OsiTwoWayBranchingObject {

public:

  // Default Constructor 
  OsiLinkBranchingObject ();

  // Useful constructor
  OsiLinkBranchingObject (OsiSolverInterface * solver,  const OsiLink * originalObject,
                          int way,
                          double separator);
  
  // Copy constructor 
  OsiLinkBranchingObject ( const OsiLinkBranchingObject &);
   
  // Assignment operator 
  OsiLinkBranchingObject & operator=( const OsiLinkBranchingObject& rhs);

  /// Clone
  virtual OsiBranchingObject * clone() const;

  // Destructor 
  virtual ~OsiLinkBranchingObject ();
  
  /// Does next branch and updates state
  virtual double branch(OsiSolverInterface * solver);

  /** \brief Print something about branch - only if log level high
  */
  virtual void print(const OsiSolverInterface * solver=NULL);
private:
  /// data
};
/** Define BiLinear objects

    This models x*y where one or both are integer

*/


class OsiBiLinear : public OsiObject2 {

public:

  // Default Constructor 
  OsiBiLinear ();

  /** Useful constructor - 
      This Adds in rows and variables to construct valid Linked Ordered Set
      Adds extra constraints to match other x/y
      So note not const solver
  */
  OsiBiLinear (OsiSolverInterface * solver, int xColumn,
	       int yColumn, int xyRow, double coefficient,
	       double xMesh, double yMesh,
	       int numberExistingObjects=0,const OsiObject ** objects=NULL );
  
  // Copy constructor 
  OsiBiLinear ( const OsiBiLinear &);
   
  /// Clone
  virtual OsiObject * clone() const;

  // Assignment operator 
  OsiBiLinear & operator=( const OsiBiLinear& rhs);

  // Destructor 
  virtual ~OsiBiLinear ();
  
  /// Infeasibility - large is 0.5
  virtual double infeasibility(const OsiBranchingInformation * info,int & whichWay) const;

  /** Set bounds to fix the variable at the current (integer) value.

    Given an integer value, set the lower and upper bounds to fix the
    variable. Returns amount it had to move variable.
  */
  virtual double feasibleRegion(OsiSolverInterface * solver, const OsiBranchingInformation * info) const;

  /** Creates a branching object

    The preferred direction is set by \p way, 0 for down, 1 for up.
  */
  virtual OsiBranchingObject * createBranch(OsiSolverInterface * solver, const OsiBranchingInformation * info, int way) const;

  /// Redoes data when sequence numbers change
  virtual void resetSequenceEtc(int numberColumns, const int * originalColumns);

  // This does NOT set mutable stuff
  virtual double checkInfeasibility(const OsiBranchingInformation * info) const;
  
  /** \brief Return true if object can take part in normal heuristics
  */
  virtual bool canDoHeuristics() const 
  {return false;};
  /** \brief Return true if branch should only bound variables
  */
  virtual bool boundBranch() const 
  { return (branchingStrategy_&4)!=0;};
  /// X column
  inline int xColumn() const
  { return xColumn_;};
  /// Y column
  inline int yColumn() const
  { return yColumn_;};
  /// X row
  inline int xRow() const
  { return xRow_;};
  /// Y row
  inline int yRow() const
  { return yRow_;};
  /// XY row
  inline int xyRow() const
  { return xyRow_;};
  /// Coefficient
  inline double coefficient() const
  { return coefficient_;};
  /// Set coefficient
  inline void setCoefficient(double value)
  { coefficient_ = value;};
  /// First lambda (of 4)
  inline int firstLambda() const
  { return firstLambda_;};
  /// X satisfied if less than this away from mesh
  inline double xSatisfied() const
  { return xSatisfied_;};
  inline void setXSatisfied(double value)
  { xSatisfied_=value;};
  /// Y satisfied if less than this away from mesh
  inline double ySatisfied() const
  { return ySatisfied_;};
  inline void setYSatisfied(double value)
  { ySatisfied_=value;};
  /// X meshSize 
  inline double xMeshSize() const
  { return xMeshSize_;};
  inline void setXMeshSize(double value)
  { xMeshSize_=value;};
  /// Y meshSize 
  inline double yMeshSize() const
  { return yMeshSize_;};
  inline void setYMeshSize(double value)
  { yMeshSize_=value;};
  /// XY satisfied if two version differ by less than this
  inline double xySatisfied() const
  { return xySatisfied_;};
  inline void setXYSatisfied(double value)
  { xySatisfied_=value;};
  /** branching strategy etc
      bottom 2 bits
      0 branch on either, 1 branch on x, 2 branch on y
      next bit
      4 set to say don't update coefficients
      next bit
      8 set to say don't use in feasible region
  */
  inline int branchingStrategy() const
  { return branchingStrategy_;};
  inline void setBranchingStrategy(int value)
  { branchingStrategy_=value;};
  /** Simple quadratic bound marker.
      0 no
      1 L if coefficient pos, G if negative i.e. value is ub on xy
      2 G if coefficient pos, L if negative i.e. value is lb on xy
      3 E
      If bound then real coefficient is 1.0 and coefficient_ is bound
  */
  inline int boundType() const
  { return boundType_;};
  inline void setBoundType(int value)
  { boundType_ = value;};
  /// Does work of branching
  void newBounds(OsiSolverInterface * solver, int way, short xOrY, double separator) const;
  /// Updates coefficients - returns number updated
  int updateCoefficients(const double * lower, const double * upper, double * objective,
			  CoinPackedMatrix * matrix, CoinWarmStartBasis * basis) const;
  /// Returns true value of single xyRow coefficient
  double xyCoefficient(const double * solution) const;
  /// Get LU coefficients from matrix
  void getCoefficients(const OsiSolverInterface * solver,double xB[2], double yB[2], double xybar[4]) const;
  /// Compute lambdas (third entry in each .B is current value) (nonzero if bad)
  double computeLambdas(const double xB[3], const double yB[3],const double xybar[4],double lambda[4]) const;

protected:
  /// Compute lambdas if coefficients not changing
  void computeLambdas(const OsiSolverInterface * solver,double lambda[4]) const;
  /// data
  
  /// Coefficient
  double coefficient_;
  /// x mesh
  double xMeshSize_;
  /// y mesh
  double yMeshSize_;
  /// x satisfied if less than this away from mesh
  double xSatisfied_;
  /// y satisfied if less than this away from mesh
  double ySatisfied_;
  /// xy satisfied if less than this away from true
  double xySatisfied_;
  /// value of x or y to branch about
  mutable double xyBranchValue_;
  /// x column
  int xColumn_;
  /// y column 
  int yColumn_;
  /// First lambda (of 4)
  int firstLambda_;
  /** branching strategy etc
      bottom 2 bits
      0 branch on either, 1 branch on x, 2 branch on y
      next bit
      4 set to say don't update coefficients
      next bit
      8 set to say don't use in feasible region
  */
  int branchingStrategy_;
  /** Simple quadratic bound marker.
      0 no
      1 L if coefficient pos, G if negative i.e. value is ub on xy
      2 G if coefficient pos, L if negative i.e. value is lb on xy
      3 E
      If bound then real coefficient is 1.0 and coefficient_ is bound
  */
  int boundType_;
  /// x row
  int xRow_;
  /// y row (-1 if x*x)
  int yRow_;
  /// Output row
  int xyRow_;
  /// Convexity row
  int convexity_;
  /// Which chosen -1 none, 0 x, 1 y
  mutable short chosen_;
};
/** Branching object for BiLinear objects

 */
class OsiBiLinearBranchingObject : public OsiTwoWayBranchingObject {

public:

  // Default Constructor 
  OsiBiLinearBranchingObject ();

  // Useful constructor
  OsiBiLinearBranchingObject (OsiSolverInterface * solver,  const OsiBiLinear * originalObject,
                          int way,
                          double separator, int chosen);
  
  // Copy constructor 
  OsiBiLinearBranchingObject ( const OsiBiLinearBranchingObject &);
   
  // Assignment operator 
  OsiBiLinearBranchingObject & operator=( const OsiBiLinearBranchingObject& rhs);

  /// Clone
  virtual OsiBranchingObject * clone() const;

  // Destructor 
  virtual ~OsiBiLinearBranchingObject ();
  
  /// Does next branch and updates state
  virtual double branch(OsiSolverInterface * solver);

  /** \brief Print something about branch - only if log level high
  */
  virtual void print(const OsiSolverInterface * solver=NULL);
  /** \brief Return true if branch should only bound variables
  */
  virtual bool boundBranch() const; 
private:
  /// data
  /// 1 means branch on x, 2 branch on y
  short chosen_;
};
/** Define Continuous BiLinear objects for an == bound

    This models x*y = b where both are continuous

*/


class OsiBiLinearEquality : public OsiBiLinear {

public:

  // Default Constructor 
  OsiBiLinearEquality ();

  /** Useful constructor - 
      This Adds in rows and variables to construct Ordered Set
      for x*y = b
      So note not const solver
  */
  OsiBiLinearEquality (OsiSolverInterface * solver, int xColumn,
	       int yColumn, int xyRow, double rhs,
		       double xMesh);
  
  // Copy constructor 
  OsiBiLinearEquality ( const OsiBiLinearEquality &);
   
  /// Clone
  virtual OsiObject * clone() const;

  // Assignment operator 
  OsiBiLinearEquality & operator=( const OsiBiLinearEquality& rhs);

  // Destructor 
  virtual ~OsiBiLinearEquality ();
  
  /// Possible improvement
  virtual double improvement(const OsiSolverInterface * solver) const;
  /** change grid
      if type 0 then use solution and make finer
      if 1 then back to original
      returns mesh size
  */
  double newGrid(OsiSolverInterface * solver, int type) const;
  /// Number of points
  inline int numberPoints() const
  { return numberPoints_;};
  inline void setNumberPoints(int value)
  { numberPoints_ = value;};

private:
  /// Number of points
  int numberPoints_;
};
/// Define a single integer class - but one where you kep branching until fixed even if satsified


class OsiSimpleFixedInteger : public OsiSimpleInteger {

public:

  /// Default Constructor 
  OsiSimpleFixedInteger ();

  /// Useful constructor - passed solver index
  OsiSimpleFixedInteger (const OsiSolverInterface * solver, int iColumn);
  
  /// Useful constructor - passed solver index and original bounds
  OsiSimpleFixedInteger (int iColumn, double lower, double upper);
  
  /// Useful constructor - passed simple integer
  OsiSimpleFixedInteger (const OsiSimpleInteger &);
  
  /// Copy constructor 
  OsiSimpleFixedInteger ( const OsiSimpleFixedInteger &);
   
  /// Clone
  virtual OsiObject * clone() const;

  /// Assignment operator 
  OsiSimpleFixedInteger & operator=( const OsiSimpleFixedInteger& rhs);

  /// Destructor 
  virtual ~OsiSimpleFixedInteger ();
  
  /// Infeasibility - large is 0.5
  virtual double infeasibility(const OsiBranchingInformation * info, int & whichWay) const;
  /** Creates a branching object

    The preferred direction is set by \p way, 0 for down, 1 for up.
  */
  virtual OsiBranchingObject * createBranch(OsiSolverInterface * solver, const OsiBranchingInformation * info, int way) const;
protected:
  /// data
  
};

#include <string>

#include "CglStored.hpp"

class CoinWarmStartBasis;
/** Stored Temporary Cut Generator Class - destroyed after first use */
class CglTemporary : public CglStored {
 
public:
    
  
  /**@name Generate Cuts */
  //@{
  /** Generate Mixed Integer Stored cuts for the model of the 
      solver interface, si.

      Insert the generated cuts into OsiCut, cs.

      This generator just looks at previously stored cuts
      and inserts any that are violated by enough
  */
  virtual void generateCuts( const OsiSolverInterface & si, OsiCuts & cs,
			     const CglTreeInfo info = CglTreeInfo()) const;
  //@}

  /**@name Constructors and destructors */
  //@{
  /// Default constructor 
  CglTemporary ();
 
  /// Copy constructor 
  CglTemporary (const CglTemporary & rhs);

  /// Clone
  virtual CglCutGenerator * clone() const;

  /// Assignment operator 
  CglTemporary &
    operator=(const CglTemporary& rhs);
  
  /// Destructor 
  virtual
    ~CglTemporary ();
  //@}
      
private:
  
 // Private member methods

  // Private member data
};
#endif
#endif
