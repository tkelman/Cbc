/**********************************************************************
TODO:

If we swap two nodes (parent and grandparent) then we should check if anything
has been explored under GP after the swap, and if not, just get rid of GP and
everything below.

If strong branching fixed anything in the grandparent we may still swap it
with the parent (we don't do it yet, see the test on
strong_branching_fixed_vars_), but those fixings must be moved to the parent as
well.

Same thing for locally valid cuts, if GP has them and GP is switched with P
then locally valid cuts must be treated as generated in P.

Same for reduced cost fixing :-(. If P has done any then P and GP can't be
switched.

Maybe the best solution is to create local information (strong branching, rc
fixing, local cuts, etc.) only every so often, say every 10th level. Then that
would block switches, but everywhere else we would be safe. Good question
what is worth more: the switches or the local info.

Alternative solution: Do not add local info to the tree, but keep it in a set
of excluded clauses ala Todias Achterberg: Conflict Analysis in Mixed Integer
Programming.

We may want to disable fixing by strong branching completely and if such
situation occurs then simply do the branch and one side will be fathomed
immediately and we can try to switch.

Bound modifications when nodes are swapped could be avoided if always start
from original bounds and go back to root to apply all branching decisions. On
the other hand, reduced cost fixing would be lost. And so would fixings by
strong branching. Although both could be stored in an array keeping track of
changes implied by the branching decisions.

**********************************************************************


**********************************************************************/
#include "CoinTime.hpp"
#include "OsiClpSolverInterface.hpp"

#define FUNNY_BRANCHING 1
#define DEBUG_DYNAMIC_BRANCHING
//#define DELETE_NODES

#ifdef DEBUG_DYNAMIC_BRANCHING
int dyn_debug = 10;
#endif

// below needed for pathetic branch and bound code
#include <vector>
#include <map>

using namespace std;

class DBVectorNode;

class LPresult {
private:
  LPresult(const LPresult& rhs);
  LPresult& operator=(const LPresult& rhs);
  void gutsOfConstructor(const OsiSolverInterface& model);
public:
  LPresult(const OsiSolverInterface& model);
  ~LPresult();
public:
  bool isAbandoned;
  bool isProvenDualInfeasible;
  bool isPrimalObjectiveLimitReached;
  bool isProvenOptimal;
  bool isDualObjectiveLimitReached;
  bool isIterationLimitReached;
  bool isProvenPrimalInfeasible;
  double getObjSense;
  double getObjValue;
  double* getReducedCost;
  double* getColLower;
  double* getColUpper;
  double* rowDualRay;
  double* colDualRay;
  double* getColSolution; // FIXME Not needed, just for debugging
  double yb_plus_rl_minus_su;
};

void
LPresult::gutsOfConstructor(const OsiSolverInterface& model)
{
  isAbandoned = model.isAbandoned();
  isProvenDualInfeasible = model.isProvenDualInfeasible();
  isPrimalObjectiveLimitReached = model.isPrimalObjectiveLimitReached();
  isProvenOptimal = model.isProvenOptimal();
  isDualObjectiveLimitReached = model.isDualObjectiveLimitReached();
  isIterationLimitReached = model.isIterationLimitReached();
  isProvenPrimalInfeasible = model.isProvenPrimalInfeasible();
  getObjSense = model.getObjSense();
  getObjValue = model.getObjValue();

  getReducedCost = new double[model.getNumCols()];
  CoinDisjointCopyN(model.getReducedCost(), model.getNumCols(), getReducedCost);
  getColLower = new double[model.getNumCols()];
  CoinDisjointCopyN(model.getColLower(), model.getNumCols(), getColLower);
  getColUpper = new double[model.getNumCols()];
  CoinDisjointCopyN(model.getColUpper(), model.getNumCols(), getColUpper);
  getColSolution = new double[model.getNumCols()];
  CoinDisjointCopyN(model.getColSolution(), model.getNumCols(), getColSolution);
  rowDualRay = NULL;
  colDualRay = NULL;

  yb_plus_rl_minus_su = 0;
  
  if (isIterationLimitReached ||
      isAbandoned ||
      isProvenDualInfeasible ||
      isPrimalObjectiveLimitReached) {
    return; // None of these should happen
  }
  // If isDualObjectiveLimitReached is true then isProvenOptimal and
  // isProvenPrimalInfeasible are correct (search for mustResolve).
  // If isDualObjectiveLimitReached false, then the others were correct even
  // without resolve. So... we need some data only if we are infeasible, but
  // that flag is correct, so we can test it.
  if (isProvenPrimalInfeasible) {
    const std::vector<double*> dual_rays = model.getDualRays(1);
    if (dual_rays.size() == 0) {
      printf("WARNING: LP is infeas, but no dual ray is returned!\n");
      return;
    }
    const double* rub = model.getRowUpper();
    const double* rlb = model.getRowLower();
    const double* cub = model.getColUpper();
    const double* clb = model.getColLower();
    const int numRows = model.getNumRows();
    const int numCols = model.getNumCols();
    rowDualRay = dual_rays[0];
    colDualRay = new double[model.getNumCols()];
#if 1
    // WARNING  ==== WARNING ==== WARNING ==== WARNING ==== WARNING
    // For some reason clp returns the negative of the dual ray...
    for (int i = 0; i < numRows; ++i) {
      rowDualRay[i] = -rowDualRay[i];
    }
#endif
    const CoinPackedMatrix* m = model.getMatrixByCol();
    m->transposeTimes(rowDualRay, colDualRay);
    
    for (int i = 0; i < numRows; ++i) {
      const double ray_i = rowDualRay[i]; 
      if (ray_i > 1e-8) {
	yb_plus_rl_minus_su += ray_i*rlb[i];
	assert(rlb[i] > -1e29);
      } else if (ray_i < 1e-8) {
	yb_plus_rl_minus_su += ray_i*rub[i];
	assert(rub[i] < 1e29);
      }
    }
    for (int j = 0; j < numCols; ++j) {
      if (colDualRay[j] > 1e-8) { //
	yb_plus_rl_minus_su -= colDualRay[j] * cub[j];
	assert(cub[j] < 1e29);
      } else if (colDualRay[j] < -1e-8) {
	yb_plus_rl_minus_su -= colDualRay[j] * clb[j];
	assert(clb[j] > -1e29);
      }
    }
    for (int i = dual_rays.size()-1; i > 0; --i) {
      delete[] dual_rays[i];
    }
    assert(yb_plus_rl_minus_su > 1e-8);
  }
}
  
LPresult::LPresult(const OsiSolverInterface& model)
{
  gutsOfConstructor(model);
}

LPresult::~LPresult()
{
  delete[] getReducedCost;
  delete[] getColLower;
  delete[] getColUpper;
  delete[] getColSolution;
  delete[] colDualRay;
  delete[] rowDualRay;
}

// Trivial class for Branch and Bound

class DBNodeSimple  {
public:
  enum DBNodeWay {
    WAY_UNSET=0x00,
    WAY_DOWN_UP__NOTHING_DONE=0x10,
    WAY_DOWN_UP__DOWN_DONE=0x11,
    WAY_DOWN_UP__BOTH_DONE=0x13,
    WAY_DOWN_UP__=0x10,
    WAY_UP_DOWN__NOTHING_DONE=0x20,
    WAY_UP_DOWN__UP_DONE=0x22,
    WAY_UP_DOWN__BOTH_DONE=0x23,
    WAY_UP_DOWN__=0x20,
    WAY_DOWN_CURRENT=0x01,
    WAY_UP_CURRENT=0x02,
    WAY_BOTH_DONE=0x03,
    
  };

private:
  void gutsOfCopy(const DBNodeSimple& rhs);
  void gutsOfConstructor (int index, OsiSolverInterface &model,
			  int numberIntegers, int * integer,
			  CoinWarmStart * basis);
  void gutsOfDestructor();  
  
public:
    
  // Default Constructor 
  DBNodeSimple ();

  // Constructor from current state (and list of integers)
  // Also chooses branching variable (if none set to -1)
  DBNodeSimple (int index, OsiSolverInterface &model,
		int numberIntegers, int * integer,
		CoinWarmStart * basis);
  // Copy constructor 
  DBNodeSimple ( const DBNodeSimple &);
   
  // Assignment operator 
  DBNodeSimple & operator=( const DBNodeSimple& rhs);

  // Destructor 
  ~DBNodeSimple ();

  // Extension - true if other extension of this
  bool extension(const DBNodeSimple & other,
		 const double * originalLower,
		 const double * originalUpper) const;
  // Tests if we can switch this node (this is the parent) with its parent
  bool canSwitchParentWithGrandparent(const int* which,
				      LPresult& lpres,
				      const int * original_lower,
				      const int * original_upper,
				      DBVectorNode & branchingTree);
  // returns the node_id of the node that can be deleted. If -1, no node can be deleted
  int nodeToSwitchWithParent(const int* which,
			     LPresult& lpres,
			     const int * original_lower,
			     const int * original_upper,
			     DBVectorNode & branchingTree);
  inline bool bothChildDone() const {
    return (way_ & WAY_BOTH_DONE) == WAY_BOTH_DONE;
  }
  inline bool workingOnDownChild() const {
    return (way_ == WAY_DOWN_UP__DOWN_DONE || way_ == WAY_UP_DOWN__BOTH_DONE);
  }
  /* Return whether the child that will be now processed is the down branch or
     not. */
  inline bool advanceWay() { 
    switch (way_) {
    case WAY_UNSET:
    case WAY_DOWN_UP__BOTH_DONE:
    case WAY_UP_DOWN__BOTH_DONE:
      abort();
    case WAY_DOWN_UP__NOTHING_DONE:
      way_ = WAY_DOWN_UP__DOWN_DONE;
      return true;
    case WAY_DOWN_UP__DOWN_DONE:
      way_ = WAY_DOWN_UP__BOTH_DONE;
      return false;
    case WAY_UP_DOWN__NOTHING_DONE:
      way_ = WAY_UP_DOWN__UP_DONE;
      return false;
    case WAY_UP_DOWN__UP_DONE:
      way_ = WAY_UP_DOWN__BOTH_DONE;
      return true;
    }
    return true; // to placate some compilers...
  }
    
  
  // Public data
  int index_;
  // The id of the node
  int node_id_;
  // Basis (should use tree, but not as wasteful as bounds!)
  CoinWarmStart * basis_;
  // Objective value (COIN_DBL_MAX) if spare node
  double objectiveValue_;
  // Branching variable (0 is first integer) 
  int variable_;
  // Way to branch: see enum DBNodeWay
  int way_;
  // Number of integers (for length of arrays)
  int numberIntegers_;
  // Current value
  double value_;
  // Parent 
  int parent_;
  // Left child
  int child_down_;
  // Right child
  int child_up_;
  // Whether strong branching fixed variables when we branched on this node
  bool strong_branching_fixed_vars_;
  // Whether reduced cost fixing fixed anything in this node
  bool reduced_cost_fixed_vars_;
  // Previous in chain
  int previous_;
  // Next in chain
  int next_;
  // Now I must use tree
  // Bounds stored in full (for integers)
  int * lower_;
  int * upper_;
};


DBNodeSimple::DBNodeSimple() :
  index_(-1),
  node_id_(-1),
  basis_(NULL),
  objectiveValue_(COIN_DBL_MAX),
  variable_(-100),
  way_(WAY_UNSET),
  numberIntegers_(0),
  value_(0.5),
  parent_(-1),
  child_down_(-1),
  child_up_(-1),
  strong_branching_fixed_vars_(false),
  reduced_cost_fixed_vars_(false),
  previous_(-1),
  next_(-1),
  lower_(NULL),
  upper_(NULL)
{
}
DBNodeSimple::DBNodeSimple(int index, OsiSolverInterface & model,
			   int numberIntegers, int * integer,CoinWarmStart * basis)
{
  gutsOfConstructor(index, model,numberIntegers,integer,basis);
}
void
DBNodeSimple::gutsOfConstructor(int index, OsiSolverInterface & model,
				int numberIntegers, int * integer,CoinWarmStart * basis)
{
  index_ = index;
  node_id_ = -1;
  basis_ = basis;
  variable_=-1;
  way_ = WAY_UNSET;
  numberIntegers_=numberIntegers;
  value_=0.0;
  parent_ = -1;
  child_down_ = -1;
  child_up_ = -1;
  strong_branching_fixed_vars_ = false;
  reduced_cost_fixed_vars_ = false;
  previous_ = -1;
  next_ = -1;
  if (model.isProvenOptimal()&&!model.isDualObjectiveLimitReached()) {
    objectiveValue_ = model.getObjSense()*model.getObjValue();
  } else {
    objectiveValue_ = 1.0e100;
    lower_ = NULL;
    upper_ = NULL;
    return; // node cutoff
  }
  lower_ = new int [numberIntegers_];
  upper_ = new int [numberIntegers_];
  assert (upper_!=NULL);
  const double * lower = model.getColLower();
  const double * upper = model.getColUpper();
  const double * solution = model.getColSolution();
  int i;
  // Hard coded integer tolerance
#define INTEGER_TOLERANCE 1.0e-6
  ///////// Start of Strong branching code - can be ignored
  // Number of strong branching candidates
#define STRONG_BRANCHING 5
#ifdef STRONG_BRANCHING
  double upMovement[STRONG_BRANCHING];
  double downMovement[STRONG_BRANCHING];
  double solutionValue[STRONG_BRANCHING];
  int chosen[STRONG_BRANCHING];
  int iSmallest=0;
  // initialize distance from integer
  for (i=0;i<STRONG_BRANCHING;i++) {
    upMovement[i]=0.0;
    chosen[i]=-1;
  }
  variable_=-1;
  // This has hard coded integer tolerance
  double mostAway=INTEGER_TOLERANCE;
  int numberAway=0;
  for (i=0;i<numberIntegers;i++) {
    int iColumn = integer[i];
    lower_[i]=(int)lower[iColumn];
    upper_[i]=(int)upper[iColumn];
    double value = solution[iColumn];
    value = max(value,(double) lower_[i]);
    value = min(value,(double) upper_[i]);
    double nearest = floor(value+0.5);
    if (fabs(value-nearest)>INTEGER_TOLERANCE)
      numberAway++;
    if (fabs(value-nearest)>mostAway) {
      double away = fabs(value-nearest);
      if (away>upMovement[iSmallest]) {
	//add to list
	upMovement[iSmallest]=away;
	solutionValue[iSmallest]=value;
	chosen[iSmallest]=i;
	int j;
	iSmallest=-1;
	double smallest = 1.0;
	for (j=0;j<STRONG_BRANCHING;j++) {
	  if (upMovement[j]<smallest) {
	    smallest=upMovement[j];
	    iSmallest=j;
	  }
	}
      }
    }
  }
  int numberStrong=0;
  for (i=0;i<STRONG_BRANCHING;i++) {
    if (chosen[i]>=0) { 
      numberStrong ++;
      variable_ = chosen[i];
    }
  }
  // out strong branching if bit set
  OsiClpSolverInterface* clp =
    dynamic_cast<OsiClpSolverInterface*>(&model);
  if (clp&&(clp->specialOptions()&16)!=0&&numberStrong>1) {
    int j;
    int iBest=-1;
    double best = 0.0;
    for (j=0;j<STRONG_BRANCHING;j++) {
      if (upMovement[j]>best) {
        best=upMovement[j];
        iBest=j;
      }
    }
    numberStrong=1;
    variable_=chosen[iBest];
  }
  if (numberStrong==1) {
    // just one - makes it easy
    int iColumn = integer[variable_];
    double value = solution[iColumn];
    value = max(value,(double) lower_[variable_]);
    value = min(value,(double) upper_[variable_]);
    double nearest = floor(value+0.5);
    value_=value;
    if (value<=nearest)
      way_ = WAY_UP_DOWN__NOTHING_DONE; // up
    else
      way_ = WAY_DOWN_UP__NOTHING_DONE; // down
  } else if (numberStrong) {
    // more than one - choose
    bool chooseOne=true;
    model.markHotStart();
    for (i=0;i<STRONG_BRANCHING;i++) {
      int iInt = chosen[i];
      if (iInt>=0) {
	int iColumn = integer[iInt];
	double value = solutionValue[i]; // value of variable in original
	double objectiveChange;
	value = max(value,(double) lower_[iInt]);
	value = min(value,(double) upper_[iInt]);

	// try down

	model.setColUpper(iColumn,floor(value));
	model.solveFromHotStart();
	model.setColUpper(iColumn,upper_[iInt]);
	if (model.isProvenOptimal()&&!model.isDualObjectiveLimitReached()) {
	  objectiveChange = model.getObjSense()*model.getObjValue()
	    - objectiveValue_;
	} else {
	  objectiveChange = 1.0e100;
	}
	assert (objectiveChange>-1.0e-5);
	objectiveChange = CoinMax(objectiveChange,0.0);
	downMovement[i]=objectiveChange;

	// try up

	model.setColLower(iColumn,ceil(value));
	model.solveFromHotStart();
	model.setColLower(iColumn,lower_[iInt]);
	if (model.isProvenOptimal()&&!model.isDualObjectiveLimitReached()) {
	  objectiveChange = model.getObjSense()*model.getObjValue()
	    - objectiveValue_;
	} else {
	  objectiveChange = 1.0e100;
	}
	assert (objectiveChange>-1.0e-5);
	objectiveChange = CoinMax(objectiveChange,0.0);
	upMovement[i]=objectiveChange;
	
	/* Possibilities are:
	   Both sides feasible - store
	   Neither side feasible - set objective high and exit
	   One side feasible - change bounds and resolve
	*/
	bool solveAgain=false;
	if (upMovement[i]<1.0e100) {
	  if(downMovement[i]<1.0e100) {
	    // feasible - no action
	  } else {
	    // up feasible, down infeasible
	    solveAgain = true;
	    model.setColLower(iColumn,ceil(value));
	  }
	} else {
	  if(downMovement[i]<1.0e100) {
	    // down feasible, up infeasible
	    solveAgain = true;
	    model.setColUpper(iColumn,floor(value));
	  } else {
	    // neither side feasible
	    objectiveValue_=1.0e100;
	    chooseOne=false;
	    break;
	  }
	}
	if (solveAgain) {
	  // need to solve problem again - signal this
	  variable_ = numberIntegers;
	  chooseOne=false;
	  break;
	}
      }
    }
    if (chooseOne) {
      // choose the one that makes most difference both ways
      double best = -1.0;
      double best2 = -1.0;
      for (i=0;i<STRONG_BRANCHING;i++) {
	int iInt = chosen[i];
	if (iInt>=0) {
	  //std::cout<<"Strong branching on "
          //   <<i<<""<<iInt<<" down "<<downMovement[i]
          //   <<" up "<<upMovement[i]
          //   <<" value "<<solutionValue[i]
          //   <<std::endl;
	  bool better = false;
	  if (min(upMovement[i],downMovement[i])>best) {
	    // smaller is better
	    better=true;
	  } else if (min(upMovement[i],downMovement[i])>best-1.0e-5) {
	    if (max(upMovement[i],downMovement[i])>best2+1.0e-5) {
	      // smaller is about same, but larger is better
	      better=true;
	    }
	  }
	  if (better) {
	    best = min(upMovement[i],downMovement[i]);
	    best2 = max(upMovement[i],downMovement[i]);
	    variable_ = iInt;
	    double value = solutionValue[i];
	    value = max(value,(double) lower_[variable_]);
	    value = min(value,(double) upper_[variable_]);
	    value_=value;
	    if (upMovement[i]<=downMovement[i])
	      way_ = WAY_UP_DOWN__NOTHING_DONE; // up
	    else
	      way_ = WAY_DOWN_UP__NOTHING_DONE; // down
	  }
	}
      }
    }
    // Delete the snapshot
    model.unmarkHotStart();
  }
  ////// End of Strong branching
#else
  variable_=-1;
  // This has hard coded integer tolerance
  double mostAway=INTEGER_TOLERANCE;
  int numberAway=0;
  for (i=0;i<numberIntegers;i++) {
    int iColumn = integer[i];
    lower_[i]=(int)lower[iColumn];
    upper_[i]=(int)upper[iColumn];
    double value = solution[iColumn];
    value = max(value,(double) lower_[i]);
    value = min(value,(double) upper_[i]);
    double nearest = floor(value+0.5);
    if (fabs(value-nearest)>INTEGER_TOLERANCE)
      numberAway++;
    if (fabs(value-nearest)>mostAway) {
      mostAway=fabs(value-nearest);
      variable_=i;
      value_=value;
      if (value<=nearest)
	way_ = WAY_UP_DOWN__NOTHING_DONE; // up
      else
	way_ = WAY_DOWN_UP__NOTHING_DONE; // down
    }
  }
#endif
}

void
DBNodeSimple::gutsOfCopy(const DBNodeSimple & rhs)
{
  index_=rhs.index_;
  node_id_=rhs.node_id_;
  basis_ = rhs.basis_ ? rhs.basis_->clone() : NULL;
  objectiveValue_=rhs.objectiveValue_;
  variable_=rhs.variable_;
  way_=rhs.way_;
  numberIntegers_=rhs.numberIntegers_;
  value_=rhs.value_;
  parent_ = rhs.parent_;
  child_down_ = rhs.child_down_;
  child_up_ = rhs.child_up_;
  strong_branching_fixed_vars_ = rhs.strong_branching_fixed_vars_;
  reduced_cost_fixed_vars_ = rhs.reduced_cost_fixed_vars_;
  previous_ = rhs.previous_;
  next_ = rhs.next_;
  lower_=NULL;
  upper_=NULL;
  if (rhs.lower_!=NULL) {
    lower_ = new int [numberIntegers_];
    upper_ = new int [numberIntegers_];
    assert (upper_!=NULL);
    memcpy(lower_,rhs.lower_,numberIntegers_*sizeof(int));
    memcpy(upper_,rhs.upper_,numberIntegers_*sizeof(int));
  }
}

DBNodeSimple::DBNodeSimple(const DBNodeSimple & rhs)
{
  gutsOfCopy(rhs);
}

DBNodeSimple &
DBNodeSimple::operator=(const DBNodeSimple & rhs)
{
  if (this != &rhs) {
    // LL: in original code basis/lower/upper was left alone if rhs did not
    // have them. Was that intentional?
    gutsOfDestructor();
    gutsOfCopy(rhs);
  }
  return *this;
}


DBNodeSimple::~DBNodeSimple ()
{
  gutsOfDestructor();
}
// Work of destructor
void 
DBNodeSimple::gutsOfDestructor()
{
  delete [] lower_;
  delete [] upper_;
  delete basis_;
  lower_ = NULL;
  upper_ = NULL;
  basis_ = NULL;
  objectiveValue_ = COIN_DBL_MAX;
}
// Extension - true if other extension of this
bool 
DBNodeSimple::extension(const DBNodeSimple & other,
			const double * originalLower,
			const double * originalUpper) const
{
  bool ok=true;
  for (int i=0;i<numberIntegers_;i++) {
    if (upper_[i]<originalUpper[i]||
	lower_[i]>originalLower[i]) {
      if (other.upper_[i]>upper_[i]||
	  other.lower_[i]<lower_[i]) {
	ok=false;
	break;
      }
    }
  }
  return ok;
}

#include <vector>

// Must code up by hand
class DBVectorNode  {
  
public:
    
  // Default Constructor 
  DBVectorNode ();

  // Copy constructor 
  DBVectorNode ( const DBVectorNode &);
   
  // Assignment operator 
  DBVectorNode & operator=( const DBVectorNode& rhs);

  // Destructor 
  ~DBVectorNode ();
  // Size
  inline int size() const
  { return size_-sizeDeferred_;}
  // Push
  void push_back(DBNodeSimple & node); // the node_id_ of node will change
  /* Remove a single node from the tree and adjust the previos_/next_/first_
     etc fields. Does NOT update child/parent relationship */
  void removeNode(int n);
  /* Remove the subtree rooted at node n. properly adjusts previos_/next_ etc
     fields. Does NOT update child/parent relationships. */
  void removeSubTree(int n);
  // Last one in (or other criterion)
  DBNodeSimple back() const;
  // Get rid of last one
  void pop_back();
  // Works out best one
  int best() const;
  // Rearranges the tree
  void moveNodeUp(const int* which,
		  OsiSolverInterface& model, DBNodeSimple & node);
  // Fix the bounds in the descendants of subroot
  void adjustBounds(int subroot, int brvar, int brlb, int brub);
  // Delete the node that was selected for deletion
  void deleteSelectedNode(int node_to_delete_id, const int* which,
			  OsiSolverInterface& model, DBNodeSimple & node);

  // Check that the bounds correspond to the branching decisions...
  void checkTree() const;
  void checkNode(int node) const;

  // Public data
  // Maximum size
  int maximumSize_;
  // Current size
  int size_;
  // Number still hanging around
  int sizeDeferred_;
  // First spare
  int firstSpare_;
  // First 
  int first_;
  // Last 
  int last_;
  // Chosen one
  mutable int chosen_;
  // Nodes
  DBNodeSimple * nodes_;
};


DBVectorNode::DBVectorNode() :
  maximumSize_(10),
  size_(0),
  sizeDeferred_(0),
  firstSpare_(0),
  first_(-1),
  last_(-1)
{
  nodes_ = new DBNodeSimple[maximumSize_];
  for (int i=0;i<maximumSize_;i++) {
    nodes_[i].previous_=i-1;
    nodes_[i].next_=i+1;
  }
}

DBVectorNode::DBVectorNode(const DBVectorNode & rhs) 
{  
  maximumSize_ = rhs.maximumSize_;
  size_ = rhs.size_;
  sizeDeferred_ = rhs.sizeDeferred_;
  firstSpare_ = rhs.firstSpare_;
  first_ = rhs.first_;
  last_ = rhs.last_;
  nodes_ = new DBNodeSimple[maximumSize_];
  for (int i=0;i<maximumSize_;i++) {
    nodes_[i] = rhs.nodes_[i];
  }
}

DBVectorNode &
DBVectorNode::operator=(const DBVectorNode & rhs)
{
  if (this != &rhs) {
    delete [] nodes_;
    maximumSize_ = rhs.maximumSize_;
    size_ = rhs.size_;
    sizeDeferred_ = rhs.sizeDeferred_;
    firstSpare_ = rhs.firstSpare_;
    first_ = rhs.first_;
    last_ = rhs.last_;
    nodes_ = new DBNodeSimple[maximumSize_];
    for (int i=0;i<maximumSize_;i++) {
      nodes_[i] = rhs.nodes_[i];
    }
  }
  return *this;
}


DBVectorNode::~DBVectorNode ()
{
  delete [] nodes_;
}
// Push
void 
DBVectorNode::push_back(DBNodeSimple & node)
{
  if (size_==maximumSize_) {
    assert (firstSpare_==size_);
    maximumSize_ = (maximumSize_*3)+10;
    DBNodeSimple * temp = new DBNodeSimple[maximumSize_];
    int i;
    for (i=0;i<size_;i++) {
      temp[i]=nodes_[i];
    }
    delete [] nodes_;
    nodes_ = temp;
    //firstSpare_=size_;
    int last = -1;
    for ( i=size_;i<maximumSize_;i++) {
      nodes_[i].previous_=last;
      nodes_[i].next_=i+1;
      last = i;
    }
  }
  assert (firstSpare_<maximumSize_);
  assert (nodes_[firstSpare_].previous_<0);
  int next = nodes_[firstSpare_].next_;
  nodes_[firstSpare_]=node;
  nodes_[firstSpare_].node_id_ = firstSpare_;
  if (last_>=0) {
    assert (nodes_[last_].next_==-1);
    nodes_[last_].next_=firstSpare_;
  }
  nodes_[firstSpare_].previous_=last_;
  nodes_[firstSpare_].next_=-1;
  if (last_==-1) {
    assert (first_==-1);
    first_ = firstSpare_;
  }
  last_=firstSpare_;
  if (next>=0&&next<maximumSize_) {
    firstSpare_ = next;
    nodes_[firstSpare_].previous_=-1;
  } else {
    firstSpare_=maximumSize_;
  }
  chosen_ = -1;
  //best();
  size_++;
  if (node.bothChildDone())
    sizeDeferred_++;
}
// Works out best one
int 
DBVectorNode::best() const
{
  // can modify
  chosen_=-1;
  if (chosen_<0) {
    chosen_=last_;
    while (nodes_[chosen_].bothChildDone()) {
      chosen_ = nodes_[chosen_].previous_;
      assert (chosen_>=0);
    }
  }
  return chosen_;
}
// Last one in (or other criterion)
DBNodeSimple 
DBVectorNode::back() const
{
  assert (last_>=0);
  return nodes_[best()];
}

void
DBVectorNode::removeNode(int n)
{
  DBNodeSimple& node = nodes_[n];
  if (node.bothChildDone())
    sizeDeferred_--;
  int previous = node.previous_;
  int next = node.next_;
  node.~DBNodeSimple();
  if (previous >= 0) {
    nodes_[previous].next_=next;
  } else {
    first_ = next;
  }
  if (next >= 0) {
    nodes_[next].previous_ = previous;
  } else {
    last_ = previous;
  }
  node.previous_ = -1;
  if (firstSpare_ >= 0) {
    node.next_ = firstSpare_;
  } else {
    node.next_ = -1;
  }
  firstSpare_ = n;
  assert (size_>0);
  size_--;
}

void
DBVectorNode::removeSubTree(int n)
{
  if (nodes_[n].child_down_ >= 0) {
    removeSubTree(nodes_[n].child_down_);
  }
  if (nodes_[n].child_up_ >= 0) {
    removeSubTree(nodes_[n].child_up_);
  }
  removeNode(n);
}

// Get rid of last one
void 
DBVectorNode::pop_back()
{
  // Temporary until more sophisticated
  //assert (last_==chosen_);
  removeNode(chosen_);
  chosen_ = -1;
}

int
DBNodeSimple::nodeToSwitchWithParent(const int* which,
				     LPresult& lpres,
				     const int * original_lower,
				     const int * original_upper,
				     DBVectorNode & branchingTree)
{
  /*
    The current node ('this') is really the parent (P) and the solution in the
    lpres represents the child. The grandparent (GP) is this.parent_. Let's have
    variable names respecting the truth.
  */
#if !defined(FUNNY_BRANCHING)
  return -1;
#endif

  const int parent_id = node_id_;
  const DBNodeSimple& parent = branchingTree.nodes_[parent_id];
  const int grandparent_id = parent.parent_;

  if (grandparent_id == -1) {
    // can't flip root higher up...
    return -1;
  }
  //  const DBNodeSimple& grandparent = branchingTree.nodes_[grandparent_id];
  
  // THINK: these are not going to happen (hopefully), but if they do, what
  // should we do???
  if (lpres.isAbandoned) {
    printf("WARNING: lpres.isAbandoned true!\n");
    return -1;
  }
  if (lpres.isProvenDualInfeasible) {
    printf("WARNING: lpres.isProvenDualInfeasible true!\n");
    return -1;
  }
  if (lpres.isPrimalObjectiveLimitReached) {
    printf("WARNING: lpres.isPrimalObjectiveLimitReached true!\n");
    return -1;
  }

  if (parent.strong_branching_fixed_vars_ || parent.reduced_cost_fixed_vars_)
    return -1;

  if (lpres.isIterationLimitReached) {
    // THINK: should we do anything?
    return -1;
  }

  // make sure that the parent has only one branch
  if(parent.way_ != DBNodeSimple::WAY_UP_DOWN__UP_DONE &&
     parent.way_ != DBNodeSimple::WAY_DOWN_UP__DOWN_DONE)
    return -1;
  
  const double direction = lpres.getObjSense;


  int node_to_switch_id = -1;
  int node_to_check_id = grandparent_id;
  int child_of_node_to_check_id = parent_id;

  while(node_to_check_id != -1) {

    const DBNodeSimple& node_to_check = branchingTree.nodes_[node_to_check_id];

    if (node_to_check.strong_branching_fixed_vars_ ||
	node_to_check.reduced_cost_fixed_vars_) {
      return node_to_switch_id;
    }

    // make sure that the node_to_check has only one branch
    if(node_to_check.way_ != DBNodeSimple::WAY_UP_DOWN__UP_DONE &&
       node_to_check.way_ != DBNodeSimple::WAY_DOWN_UP__DOWN_DONE)
      return node_to_switch_id;

    const int GP_brvar = node_to_check.variable_;
    const int GP_brvar_fullid = which[GP_brvar];
    const bool parent_is_down_child = child_of_node_to_check_id == node_to_check.child_down_;

    
    // At this point the only flags of interest are isProvenOptimal,
    // isDualObjectiveLimitReached and isProvenPrimalInfeasible.
    // Note that isDualObjectiveLimitReached can't be true alone (search for
    // mustResolve).

    
    if (lpres.isProvenOptimal) {
      // May or may not be over the dual obj limit. If the obj val of parent is
      // higher than that of the node_to_check then we allow switching.
      // NOTE: node_to_check's obj val can;t be 1e100 since then parent would not
      // exist...
      if (lpres.getObjValue > node_to_check.objectiveValue_ + 1e-8) {
	double djValue = lpres.getReducedCost[GP_brvar_fullid]*direction;
	bool should_switch = false;
	if (djValue > 1.0e-6) {
	  // wants to go down
	  if (parent_is_down_child) {
	    should_switch = true;
	  }
	  else if (lpres.getColLower[GP_brvar_fullid] > std::ceil(node_to_check.value_)) {
	    should_switch = true;
	  }
	} else if (djValue < -1.0e-6) {
	  // wants to go up
	  if (! parent_is_down_child) {
	    should_switch = true;
	  }
	  else if (lpres.getColUpper[GP_brvar_fullid] < std::floor(node_to_check.value_)) {
	    should_switch = true;
	  }
	}
	if(should_switch) {
	  node_to_switch_id = node_to_check_id;
	  child_of_node_to_check_id = node_to_check_id;
	  node_to_check_id = node_to_check.parent_;
	}
	else
	  node_to_check_id = -1;
      }
      else
	node_to_check_id = -1;
    }
    else {
      // Problem is really infeasible and has a dual ray.
      assert(lpres.isProvenPrimalInfeasible);

      return -1;
    }

  }

  return node_to_switch_id;
}

bool
DBNodeSimple::canSwitchParentWithGrandparent(const int* which,
					     LPresult& lpres,
					     const int * original_lower,
					     const int * original_upper,
					     DBVectorNode & branchingTree)
{
  /*
    The current node ('this') is really the parent (P) and the solution in the
    lpres represents the child. The grandparent (GP) is this.parent_. Let's have
    variable names respecting the truth.
  */
#if !defined(FUNNY_BRANCHING)
  return false;
#endif

  const int parent_id = node_id_;
  const DBNodeSimple& parent = branchingTree.nodes_[parent_id];
  const int grandparent_id = parent.parent_;

  if (grandparent_id == -1) {
    // can't flip root higher up...
    return false;
  }
  const DBNodeSimple& grandparent = branchingTree.nodes_[grandparent_id];
  
  // THINK: these are not going to happen (hopefully), but if they do, what
  // should we do???
  if (lpres.isAbandoned) {
    printf("WARNING: lpres.isAbandoned true!\n");
    return false;
  }
  if (lpres.isProvenDualInfeasible) {
    printf("WARNING: lpres.isProvenDualInfeasible true!\n");
    return false;
  }
  if (lpres.isPrimalObjectiveLimitReached) {
    printf("WARNING: lpres.isPrimalObjectiveLimitReached true!\n");
    return false;
  }

  if (parent.strong_branching_fixed_vars_ || parent.reduced_cost_fixed_vars_ ||
      grandparent.strong_branching_fixed_vars_ ||
      grandparent.reduced_cost_fixed_vars_) {
    return false;
  }
  
  const double direction = lpres.getObjSense;

  const int GP_brvar = grandparent.variable_;
  const int GP_brvar_fullid = which[GP_brvar];
  const bool parent_is_down_child = parent_id == grandparent.child_down_;

  if (lpres.isIterationLimitReached) {
    // THINK: should we do anything?
    return false;
  }

  // At this point the only flags of interest are isProvenOptimal,
  // isDualObjectiveLimitReached and isProvenPrimalInfeasible.
  // Note that isDualObjectiveLimitReached can't be true alone (search for
  // mustResolve).

  if (lpres.isProvenOptimal) {
    // May or may not be over the dual obj limit. If the obj val of parent is
    // higher than that of the grandparent then we allow switching.
    // NOTE: grandparent's obj val can;t be 1e100 since then parent would not
    // exist...
    if (lpres.getObjValue > grandparent.objectiveValue_ + 1e-8) {
      double djValue = lpres.getReducedCost[GP_brvar_fullid]*direction;
      if (djValue > 1.0e-6) {
	// wants to go down
	if (parent_is_down_child) {
	  return true;
	}
	if (lpres.getColLower[GP_brvar_fullid] > std::ceil(grandparent.value_)) {
	  return true;
	}
      } else if (djValue < -1.0e-6) {
	// wants to go up
	if (! parent_is_down_child) {
	  return true;
	}
	if (lpres.getColUpper[GP_brvar_fullid] < std::floor(grandparent.value_)) {
	  return true;
	}
      }
    }
    return false;
  }

  // Problem is really infeasible and has a dual ray.
  assert(lpres.isProvenPrimalInfeasible);
  const int greatgrandparent_id = grandparent.parent_;
  const int x = GP_brvar_fullid; // for easier reference... we'll use s_x

  /*
    Now we are going to check that if we relax the GP's branching decision
    then the child's LP relaxation is still infeasible. The test is done by
    checking whether the column (or its negative) of the GP's branching
    variable cuts off the dual ray proving the infeasibility.
  */
    
  const double* colDualRay = lpres.colDualRay;
  const double ub_without_GP = greatgrandparent_id >= 0 ?
    branchingTree.nodes_[greatgrandparent_id].upper_[GP_brvar] :
    original_upper[GP_brvar];
  const double lb_without_GP = greatgrandparent_id >= 0 ?
    branchingTree.nodes_[greatgrandparent_id].lower_[GP_brvar] :
    original_lower[GP_brvar];

  if (parent_is_down_child) { // upper bound on x is forced
    if (colDualRay[x] < 1e-8) { // if yA_x] is non-positive then the var is
				// at its lower bound (or has 0 red cost)
				// so same dual ray still proves infeas
      return true;
    }
    const double max_ub_increase = lpres.yb_plus_rl_minus_su / colDualRay[x];
    assert(max_ub_increase >= 1e-8);
    const bool willSwitch =
      max_ub_increase > ub_without_GP - lpres.getColUpper[x] + 1e-8;
    if (willSwitch) {
      // The switch will happen so we might as well update the "infeasibility"
      lpres.yb_plus_rl_minus_su -= colDualRay[x] * (ub_without_GP - lpres.getColUpper[x]);
    }
    return willSwitch;
  } else { // lower bound on x is forced
    if (colDualRay[x] > -1e-8) { // if yA_x is non-negative then the var is
				 // at its upper bound (or has 0 red cost)
				 // so same dual ray still proves infeas
      return true;
    }
    const double max_lb_decrease = - lpres.yb_plus_rl_minus_su / colDualRay[x];
    assert(max_lb_decrease >= 1e-8);
    const bool willSwitch =
      max_lb_decrease > lpres.getColLower[x] - lb_without_GP + 1e-8;
    if (willSwitch) {
      // The switch will happen so we might as well update the "infeasibility"
      lpres.yb_plus_rl_minus_su += colDualRay[x] * (lpres.getColLower[x] - lb_without_GP);
    }
    return willSwitch;
  }
  // THINK: the above is definitely good for minimization problems. Is it good
  // for max?

  return true; // to placate some compilers
}

void
DBVectorNode::adjustBounds(int subroot, int brvar, int brlb, int brub)
{
  assert(subroot != -1);
  DBNodeSimple& node = nodes_[subroot];
  // Take the intersection of brvar's bounds in node and those passed in
  brub = CoinMin(brub, node.upper_[brvar]);
  brlb = CoinMax(brlb, node.lower_[brvar]);
  if (brub < brlb) {
    // This node became infeasible. Get rid of it and of its descendants
    const int parent_id = node.parent_;
    removeSubTree(subroot);
    if (nodes_[parent_id].child_down_ == subroot) {
      nodes_[parent_id].child_down_ = -1;
    } else {
      nodes_[parent_id].child_up_ = -1;
    }
    return;
  }
  if (node.variable_ == brvar) {
    if (node.value_ < brlb) {
      // child_down_ just became infeasible. Just cut out the current node
      // together with its child_down_ from the tree and hang child_up_ on the
      // parent of this node.
      if (node.child_down_ >= 0) {
	removeSubTree(node.child_down_);
      }
      const int parent_id = node.parent_;
      const int child_remains = node.child_up_;
      if (nodes_[parent_id].child_down_ == subroot) {
	nodes_[parent_id].child_down_ = child_remains;
      } else {
	nodes_[parent_id].child_up_ = child_remains;
      }
      removeNode(subroot);
      if (child_remains >= 0) {
	nodes_[child_remains].parent_ = parent_id;
	adjustBounds(child_remains, brvar, brlb, brub);
      }
      return;
    }
    if (node.value_ > brub) {
      // child_up_ just became infeasible. Just cut out the current node
      // together with its child_down_ from the tree and hang child_down_ on
      // the parent of this node.
      if (node.child_up_ >= 0) {
	removeSubTree(node.child_up_);
      }
      const int parent_id = node.parent_;
      const int child_remains = node.child_down_;
      if (nodes_[parent_id].child_down_ == subroot) {
	nodes_[parent_id].child_down_ = child_remains;
      } else {
	nodes_[parent_id].child_up_ = child_remains;
      }
      removeNode(subroot);
      if (child_remains >= 0) {
	nodes_[child_remains].parent_ = parent_id;
	adjustBounds(child_remains, brvar, brlb, brub);
      }
      return;
    }
    // Now brlb < node.value_ < brub (value_ is fractional)
  }
  node.lower_[brvar] = brlb;
  node.upper_[brvar] = brub;
  if (node.child_down_ >= 0) {
    adjustBounds(node.child_down_, brvar, brlb, brub);
  }
  if (node.child_up_ >= 0) {
    adjustBounds(node.child_up_, brvar, brlb, brub);
  }
}

void
DBVectorNode::moveNodeUp(const int* which,
			 OsiSolverInterface& model, DBNodeSimple & node)
{
  /*
    The current node ('this') is really the parent (P). The grandparent (GP)
    is this.parent_. Let's have variable names respecting the truth.
  */
  const int parent_id = node.node_id_;
  DBNodeSimple& parent = nodes_[parent_id];
  const int grandparent_id = parent.parent_;
  assert(grandparent_id != -1);
  DBNodeSimple& grandparent = nodes_[grandparent_id];
  const int greatgrandparent_id = grandparent.parent_;
  
  const bool parent_is_down_child = parent_id == grandparent.child_down_;

#if defined(DEBUG_DYNAMIC_BRANCHING)
  if (dyn_debug >= 100) {
    printf("entered moveNodeUp\n");
    printf("parent_id %d grandparent_id %d greatgrandparent_id %d\n",
	   parent_id, grandparent_id, greatgrandparent_id);
    printf("parent.way_ %d\n", parent.way_);
  }
#endif


  // First hang the nodes where they belong.
  parent.parent_ = greatgrandparent_id;
  grandparent.parent_ = parent_id;
  const bool down_child_stays_with_parent = parent.workingOnDownChild();
  int& child_to_move =
    down_child_stays_with_parent ? parent.child_up_ : parent.child_down_;
  const bool child_to_move_is_processed = parent.bothChildDone();

#if defined(DEBUG_DYNAMIC_BRANCHING)
  if (dyn_debug >= 1000) {
    printf("parent_is_down_child %d down_child_stays_with_parent %d child_to_move %d\n", parent_is_down_child, down_child_stays_with_parent, child_to_move);
  }
#endif

  if (parent_is_down_child) {
    grandparent.child_down_ = child_to_move;
  } else {
    grandparent.child_up_ = child_to_move;
  }
  if (child_to_move >= 0) {
    nodes_[child_to_move].parent_ = grandparent_id;
  }
  child_to_move = grandparent_id;
  if (greatgrandparent_id >= 0) {
    DBNodeSimple& greatgrandparent = nodes_[greatgrandparent_id];
    if (grandparent_id == greatgrandparent.child_down_) {
      greatgrandparent.child_down_ = parent_id;
    } else {
      greatgrandparent.child_up_ = parent_id;
    }
  }

#if defined(DEBUG_DYNAMIC_BRANCHING)
  if (dyn_debug >= 1000) {
    printf("after exchange\n");
    printf("parent.parent_ %d parent.child_down_ %d parent.child_up_ %d\n",
	   parent.parent_, parent.child_down_, parent.child_up_);
    printf("grandparent.parent_ %d grandparent.child_down_ %d grandparent.child_up_ %d\n",
	   grandparent.parent_, grandparent.child_down_, grandparent.child_up_);
    if (greatgrandparent_id >= 0) {
      DBNodeSimple& greatgrandparent = nodes_[greatgrandparent_id];
      printf("greatgrandparent.parent_ %d greatgrandparent.child_down_ %d greatgrandparent.child_up_ %d\n",
	     greatgrandparent.parent_, greatgrandparent.child_down_, greatgrandparent.child_up_);
    }
    printf("exiting moveNodeUp\n");
  }
#endif


  // Now make sure way_ is set properly
  bool removeGrandparent = false;
  if (down_child_stays_with_parent) {
    if (!parent.bothChildDone()) {
      parent.way_ = DBNodeSimple::WAY_UP_DOWN__BOTH_DONE;
      sizeDeferred_++;
    }
    if (grandparent.bothChildDone()) {
      if (! child_to_move_is_processed) {
	grandparent.way_ = parent_is_down_child ?
	  DBNodeSimple::WAY_UP_DOWN__UP_DONE :
	  DBNodeSimple::WAY_DOWN_UP__DOWN_DONE;
	sizeDeferred_--;
      }
    } else { // only parent is processed from the two children of grandparent
      if (! child_to_move_is_processed) {
	// remove grandparent, none of its children is processed now, why
	// force its branching decision?
	removeGrandparent = true;
	// the node is removed further down after changing the parent's bounds
	parent.child_up_ = -1;
	parent.way_ = DBNodeSimple::WAY_DOWN_UP__DOWN_DONE;
	// No bound changes are needed on the GO side as GP is
	// deleted...
      } else {
	grandparent.way_ = parent_is_down_child ?
	  DBNodeSimple::WAY_DOWN_UP__DOWN_DONE :
	  DBNodeSimple::WAY_UP_DOWN__UP_DONE;
      }
    }
  } else {
    if (!parent.bothChildDone()) {
      parent.way_ = DBNodeSimple::WAY_DOWN_UP__BOTH_DONE;
      sizeDeferred_++;
    }
    if (grandparent.bothChildDone()) {
      if (! child_to_move_is_processed) {
	grandparent.way_ = parent_is_down_child ?
	  DBNodeSimple::WAY_UP_DOWN__UP_DONE :
	  DBNodeSimple::WAY_DOWN_UP__DOWN_DONE;
	sizeDeferred_--;
      }
    } else { // only parent is processed from the two children of grandparent
      if (! child_to_move_is_processed) {
	removeGrandparent = true;
	// the node is removed further down after changing the parent's bounds
	parent.child_down_ = -1;
	parent.way_ = DBNodeSimple::WAY_UP_DOWN__UP_DONE;
      } else {
	grandparent.way_ = parent_is_down_child ?
	  DBNodeSimple::WAY_DOWN_UP__DOWN_DONE :
	  DBNodeSimple::WAY_UP_DOWN__UP_DONE;
      }
    }
  }
  
  // Now modify bounds

  // First, get rid of GP's bound change of its branching variable in the
  // bound list of P. Note: If greatgrandparent_id is < 0 then GP is the root
  // so its bounds are the original bounds.
  const int GP_brvar = grandparent.variable_;
  if (parent_is_down_child) {
    const int old_upper = grandparent.upper_[GP_brvar];
    parent.upper_[GP_brvar] = old_upper;
    if ((GP_brvar != parent.variable_) ||
	(GP_brvar == parent.variable_ && !down_child_stays_with_parent)) {
      model.setColUpper(which[GP_brvar], old_upper);
    }
  } else {
    const int old_lower = grandparent.lower_[GP_brvar];
    parent.lower_[GP_brvar] = old_lower;
    if ((GP_brvar != parent.variable_) ||
	(GP_brvar == parent.variable_ && down_child_stays_with_parent)) {
      model.setColLower(which[GP_brvar], old_lower);
    }
  }

  if(removeGrandparent) {
    removeNode(grandparent_id);
    sizeDeferred_--;
  }
  else {
    // Now add the branching var bound change of P to GP and all of its
    // descendant
    if (down_child_stays_with_parent) {
      adjustBounds(grandparent_id, parent.variable_,
		   (int)ceil(parent.value_), parent.upper_[parent.variable_]);
    } else {
      adjustBounds(grandparent_id, parent.variable_,
		   parent.lower_[parent.variable_], (int)floor(parent.value_));
    }
  }
}

void
DBVectorNode::deleteSelectedNode(int node_to_delete_id, const int* which,
				 OsiSolverInterface& model, DBNodeSimple & node)
{
  // loop from the current node up until it finds the node_to_delete
  int parent_id = node.node_id_;
  DBNodeSimple& parent = nodes_[parent_id];
  int grandparent_id = parent.parent_;
  assert(grandparent_id != -1);

  while(grandparent_id != node_to_delete_id) {
    parent_id = grandparent_id;
    grandparent_id = nodes_[parent_id].parent_;
    assert(grandparent_id != -1);
  }

  DBNodeSimple& grandparent = nodes_[grandparent_id];
  const int greatgrandparent_id = grandparent.parent_;

  const bool parent_is_down_child = parent_id == grandparent.child_down_;

  // First hang the nodes where they belong.
  DBNodeSimple& parent_0 = nodes_[parent_id];
  parent_0.parent_ = greatgrandparent_id;
  if (greatgrandparent_id >= 0) {
    DBNodeSimple& greatgrandparent = nodes_[greatgrandparent_id];
    if (grandparent_id == greatgrandparent.child_down_) {
      greatgrandparent.child_down_ = parent_id;
    } else {
      greatgrandparent.child_up_ = parent_id;
    }
  }

  // Now modify bounds

  // First, get rid of GP's bound change of its branching variable in the
  // bound list of P. Note: If greatgrandparent_id is < 0 then GP is the root
  // so its bounds are the original bounds.
  const int GP_brvar = grandparent.variable_;
  parent_id = node.node_id_;
  DBNodeSimple& parent_1 = nodes_[parent_id];
  if (parent_is_down_child) {
    const int old_upper = grandparent.upper_[GP_brvar];
    parent_1.upper_[GP_brvar] = old_upper;
    if (GP_brvar != parent_1.variable_)
      model.setColUpper(which[GP_brvar], old_upper);
  } else {
    const int old_lower = grandparent.lower_[GP_brvar];
    parent_1.lower_[GP_brvar] = old_lower;
    if (GP_brvar != parent_1.variable_)
      model.setColLower(which[GP_brvar], old_lower);
  }
  parent_id = parent_1.parent_;
  while(parent_id != greatgrandparent_id) {
    DBNodeSimple& parent_2 = nodes_[parent_id];    
    if (parent_is_down_child) {
      const int old_upper = grandparent.upper_[GP_brvar];
      parent_2.upper_[GP_brvar] = old_upper;
    } else {
      const int old_lower = grandparent.lower_[GP_brvar];
      parent_2.lower_[GP_brvar] = old_lower;
    }
    parent_id = parent_2.parent_;
  }    

  // remove grandparent
  removeNode(grandparent_id);
  //  sizeDeferred_--; // this is not needed because the grandparent just had 1 branch

}

void
DBVectorNode::checkNode(int node) const
{
  if (node == -1) {
    return;
  }
  const DBNodeSimple* n = nodes_ + node;
  const DBNodeSimple* p = nodes_ + n->parent_;
  for (int i = n->numberIntegers_-1; i >= 0; --i) {
    if (i == p->variable_) {
      if (node == p->child_down_) {
	assert(p->lower_[i] <= n->lower_[i]);
	assert((int)(floor(p->value_)) == n->upper_[i]);
      } else {
	assert((int)(ceil(p->value_)) == n->lower_[i]);
	assert(p->upper_[i] >= n->upper_[i]);
      }
    } else {
      assert(p->lower_[i] <= n->lower_[i]);
      assert(p->upper_[i] >= n->upper_[i]);
    }
  }
  checkNode(n->child_down_);
  checkNode(n->child_up_);
}

void
DBVectorNode::checkTree() const
{
  // find the root
  int root = first_;
  while (true) {
    if (nodes_[root].parent_ == -1) {
      break;
    } else {
      root = nodes_[root].parent_;
    }
  }
  checkNode(nodes_[root].child_down_);
  checkNode(nodes_[root].child_up_);
}

std::string getTree(DBVectorNode& branchingTree)
{
  std::string tree;
  char line[1000];
  int k_max = branchingTree.size_;
  for(int k=0; k<k_max; k++) {
    DBNodeSimple& node = branchingTree.nodes_[k];
    if(node.lower_ != NULL) {
      sprintf(line, "%d %d %d %d %f %d %d 0x%x %d %d %f\n",
	      k, node.node_id_, node.parent_, node.variable_,
	      node.value_, node.lower_[node.variable_],
	      node.upper_[node.variable_], node.way_,
	      node.child_down_, node.child_up_, node.objectiveValue_);
      tree += line;
    }
    else
      k_max++;
  }
  return tree;
}

void printTree(const std::string& tree, int levels)
{
  size_t pos = tree.size();
  for (int i = levels-1; i >= 0; --i) {
    pos = tree.rfind('\n', pos-1);
    if (pos == std::string::npos) {
      pos = 0;
      break;
    }
  }
  printf("%s", tree.c_str() + (pos+1));
}

void printChain(DBVectorNode& branchingTree, int k)
{
  while (k != -1) {
    DBNodeSimple& node = branchingTree.nodes_[k];
    printf("   %d %d %d %d %f %d %d 0x%x %d %d %c %c\n",
	   k, node.node_id_, node.parent_, node.variable_,
	   node.value_, node.lower_[node.variable_],
	   node.upper_[node.variable_], node.way_,
	   node.child_down_, node.child_up_,
	   node.strong_branching_fixed_vars_ ? 'T' : 'F',
	   node.reduced_cost_fixed_vars_ ? 'T' : 'F');
    k = node.parent_;
  }
}

// Invoke solver's built-in enumeration algorithm
void 
branchAndBound(OsiSolverInterface & model) {
  double time1 = CoinCpuTime();

  OsiClpSolverInterface* clp =
    dynamic_cast<OsiClpSolverInterface*>(&model);
  if(clp) {
    ClpSimplex* modelPtr = clp->getModelPtr();
    modelPtr->setLogLevel(0);
  }

  // solve LP
  model.initialSolve();

  if (model.isProvenOptimal()&&!model.isDualObjectiveLimitReached()) {
    // Continuous is feasible - find integers
    int numberIntegers=0;
    int numberColumns = model.getNumCols();
    int iColumn;
    int i;
    for (iColumn=0;iColumn<numberColumns;iColumn++) {
      if( model.isInteger(iColumn))
        numberIntegers++;
    }
    if (!numberIntegers) {
      std::cout<<"No integer variables"
               <<std::endl;
      return;
    }
    int * which = new int[numberIntegers]; // which variables are integer
    // original bounds
    int * originalLower = new int[numberIntegers];
    int * originalUpper = new int[numberIntegers];
    int * relaxedLower = new int[numberIntegers];
    int * relaxedUpper = new int[numberIntegers];
    {
      const double * lower = model.getColLower();
      const double * upper = model.getColUpper();
      numberIntegers=0;
      for (iColumn=0;iColumn<numberColumns;iColumn++) {
	if( model.isInteger(iColumn)) {
	  originalLower[numberIntegers]=(int) lower[iColumn];
	  originalUpper[numberIntegers]=(int) upper[iColumn];
	  which[numberIntegers++]=iColumn;
	}
      }
    }
    double direction = model.getObjSense();
    // empty tree
    DBVectorNode branchingTree;
    
    // Add continuous to it;
    DBNodeSimple rootNode(0,model,numberIntegers,which,model.getWarmStart());
    // something extra may have been fixed by strong branching
    // if so go round again
    while (rootNode.variable_==numberIntegers) {
      model.resolve();
      rootNode = DBNodeSimple(0,model,numberIntegers,which,model.getWarmStart());
    }
    if (rootNode.objectiveValue_<1.0e100) {
      // push on stack
      branchingTree.push_back(rootNode);
    }
    
    // For printing totals
    int numberIterations=0;
    int numberNodes =0;
    DBNodeSimple bestNode;
    ////// Start main while of branch and bound
    // while until nothing on stack
    while (branchingTree.size()) {
#if defined(DEBUG_DYNAMIC_BRANCHING)
      if (dyn_debug >= 1000) {
	printf("branchingTree.size = %d %d\n",branchingTree.size(),branchingTree.size_);
	printf("i node_id parent child_down child_up\n");
	for(int k=0; k<branchingTree.size_; k++) {
	  DBNodeSimple& node = branchingTree.nodes_[k];
	  printf("%d %d %d %d %d\n",k, node.node_id_, node.parent_,
		 node.child_down_, node.child_up_);
	}
      }
#endif
      // last node
      DBNodeSimple node = branchingTree.back();
      int kNode = branchingTree.chosen_;
      branchingTree.pop_back();
#if defined(DEBUG_DYNAMIC_BRANCHING)
      if (dyn_debug >= 1000) {
	printf("Deleted current parent %d %d\n",branchingTree.size(),branchingTree.size_);
      }
#endif
      assert (! node.bothChildDone());
      numberNodes++;
      if (node.variable_ < 0) {
	// put back on tree and pretend both children are done. We want the
	// whole tree all the time.
	node.way_ = DBNodeSimple::WAY_UP_DOWN__BOTH_DONE;
	branchingTree.push_back(node);
        // Integer solution - save
        bestNode = node;
        // set cutoff (hard coded tolerance)
	const double limit = (bestNode.objectiveValue_-1.0e-5)*direction;
        model.setDblParam(OsiDualObjectiveLimit, limit);
        std::cout<<"Integer solution of "
                 <<bestNode.objectiveValue_
                 <<" found after "<<numberIterations
                 <<" iterations and "<<numberNodes<<" nodes"
                 <<std::endl;
#if defined(DEBUG_DYNAMIC_BRANCHING)
	if (dyn_debug >= 1) {
	  branchingTree.checkTree();
	}
#endif

      } else {
        // branch - do bounds
        for (i=0;i<numberIntegers;i++) {
          iColumn=which[i];
          model.setColBounds( iColumn,node.lower_[i],node.upper_[i]);
        }
        // move basis
        model.setWarmStart(node.basis_);
        // do branching variable
	const bool down_branch = node.advanceWay();
        if (down_branch) {
          model.setColUpper(which[node.variable_],floor(node.value_));
        } else {
          model.setColLower(which[node.variable_],ceil(node.value_));
        }
	// put back on tree anyway regardless whether any processing is left
	// to be done. We want the whole tree all the time.
	branchingTree.push_back(node);
#if defined(DEBUG_DYNAMIC_BRANCHING)
	if (dyn_debug >= 1000) {
	  printf("Added current parent %d %d\n",branchingTree.size(),branchingTree.size_);
	}
#endif
	
        // solve
        model.resolve();
        CoinWarmStart * ws = model.getWarmStart();
        const CoinWarmStartBasis* wsb =
          dynamic_cast<const CoinWarmStartBasis*>(ws);
        assert (wsb!=NULL); // make sure not volume
        numberIterations += model.getIterationCount();
        // fix on reduced costs
        int nFixed0=0,nFixed1=0;
        double cutoff;
        model.getDblParam(OsiDualObjectiveLimit,cutoff);
        double gap=(cutoff-model.getObjValue())*direction+1.0e-4;
	//        double
	//        gap=(cutoff-modelPtr_->objectiveValue())*direction+1.0e-4;
	bool did_reduced_cost_fixing_for_child = false;
        if (gap<1.0e10&&model.isProvenOptimal()&&!model.isDualObjectiveLimitReached()) {
          const double * dj = model.getReducedCost();
          const double * lower = model.getColLower();
          const double * upper = model.getColUpper();
          for (i=0;i<numberIntegers;i++) {
            iColumn=which[i];
            if (upper[iColumn]>lower[iColumn]) {
              double djValue = dj[iColumn]*direction;
              if (wsb->getStructStatus(iColumn)==CoinWarmStartBasis::atLowerBound&&
                  djValue>gap) {
                nFixed0++;
                model.setColUpper(iColumn,lower[iColumn]);
              } else if (wsb->getStructStatus(iColumn)==CoinWarmStartBasis::atUpperBound&&
                         -djValue>gap) {
                nFixed1++;
                model.setColLower(iColumn,upper[iColumn]);
              }
            }
          }
          if (nFixed0+nFixed1) {
	    // 	    printf("%d fixed to lower, %d fixed to upper\n",
	    // 		   nFixed0,nFixed1);
	    did_reduced_cost_fixing_for_child = true;
	  }
	}
	if (model.isAbandoned()) {
	  // THINK: What the heck should we do???
	  abort();
	}
	if (model.isIterationLimitReached()) {
	  // maximum iterations - exit
	  std::cout<<"Exiting on maximum iterations\n";
	  break;
	}

        const double parentGap = (cutoff-node.objectiveValue_)*direction + 1.0e-4;
	assert (parentGap >= 0);
	const bool smallGap = parentGap / fabs(cutoff) < 0.05;

	// We are not going to do any switching unless the gap is small
	if (smallGap) {
	  // THINK: If goes over the dual obj limit then clp sets dual obj limit
	  // reached *AND* poven primal infeas. So we need to run it to
	  // completition to know whether it's really infeas or just over the
	  // bound. Something faster would be better...
	  const bool mustResolve =
	    model.isDualObjectiveLimitReached() && !model.isProvenOptimal();
	  double oldlimit = 0;
	  if (mustResolve) {
	    model.getDblParam(OsiDualObjectiveLimit, oldlimit);
	    model.setDblParam(OsiDualObjectiveLimit, 1e100);
	    model.resolve();
	  }
	  LPresult lpres(model);
	  if (mustResolve) {
	    lpres.isDualObjectiveLimitReached = true;
	    model.setDblParam(OsiDualObjectiveLimit, oldlimit);
	  }
#if defined(DELETE_NODES)
	  int node_to_delete = node.nodeToSwitchWithParent(which, lpres, originalLower,
							   originalUpper, branchingTree);
	  bool canSwitch = false;
	  if(node_to_delete >= 0)
	    canSwitch = true;
#else
	  bool canSwitch =
	    node.canSwitchParentWithGrandparent(which, lpres, originalLower,
						originalUpper, branchingTree);
#endif
	  int cnt = 0;

#if defined(DEBUG_DYNAMIC_BRANCHING)
	  if (dyn_debug >= 1) {
	    branchingTree.checkTree();
	  }
#endif

#if defined(DEBUG_DYNAMIC_BRANCHING)
	  std::string tree0;
	  if (dyn_debug >= 10) {
	    if (canSwitch) {
	      tree0 = getTree(branchingTree);
	    }
	  }
#endif

	  while (canSwitch) {
#if defined(DELETE_NODES)
	    branchingTree.deleteSelectedNode(node_to_delete, which, model, node);
	    node_to_delete = node.nodeToSwitchWithParent(which, lpres, originalLower,
							 originalUpper, branchingTree);
	    canSwitch = false;
	    if(node_to_delete >= 0)
	      canSwitch = true;
#else
	    branchingTree.moveNodeUp(which, model, node);
	    canSwitch = node.canSwitchParentWithGrandparent(which, lpres,
							    originalLower,
							    originalUpper,
							    branchingTree);
#endif
#if defined(DEBUG_DYNAMIC_BRANCHING)
	    if (dyn_debug >= 1) {
	      branchingTree.checkTree();
	    }
#endif
	    ++cnt;
	  }
	  if (cnt > 0) {
	    printf("Before switch: opt: %c   inf: %c   dual_bd: %c   objVal: %f\n",
		   lpres.isProvenOptimal ? 'T' : 'F',
		   lpres.isProvenPrimalInfeasible ? 'T' : 'F',
		   lpres.isDualObjectiveLimitReached ? 'T' : 'F',
		   lpres.getObjSense * lpres.getObjValue);
	    model.resolve();
	    // This is horribly looking... Get rid of it when properly
	    // debugged...
#if 1
	    const bool mustResolve =
	      model.isDualObjectiveLimitReached() && !model.isProvenOptimal();
	    double oldlimit = 0;
	    
	    if (mustResolve) {
	      // THINK: Something faster would be better...
	      model.getDblParam(OsiDualObjectiveLimit, oldlimit);
	      model.setDblParam(OsiDualObjectiveLimit, 1e100);
	      model.resolve();
	    }
	    LPresult lpres1(model);
	    if (mustResolve) {
	      lpres.isDualObjectiveLimitReached = true;
	      model.setDblParam(OsiDualObjectiveLimit, oldlimit);
	    }
	    printf("After resolve: opt: %c   inf: %c   dual_bd: %c   objVal: %f\n",
		   lpres1.isProvenOptimal ? 'T' : 'F',
		   lpres1.isProvenPrimalInfeasible ? 'T' : 'F',
		   lpres1.isDualObjectiveLimitReached ? 'T' : 'F',
		   lpres.getObjSense * lpres.getObjValue);
	    assert(lpres.isAbandoned == model.isAbandoned());
	    assert(lpres.isDualObjectiveLimitReached == model.isDualObjectiveLimitReached());
	    assert(lpres.isDualObjectiveLimitReached ||
		   (lpres.isProvenOptimal == model.isProvenOptimal()));
	    assert(lpres.isDualObjectiveLimitReached ||
		   (lpres.isProvenPrimalInfeasible == model.isProvenPrimalInfeasible()));
	    assert(!lpres.isProvenOptimal || ! model.isProvenOptimal() ||
		   (lpres.isProvenOptimal && model.isProvenOptimal() &&
		    abs(lpres.getObjValue - model.getObjValue()) < 1.0e-8));
#endif
	    printf("Finished moving node %d up by %i levels.\n", node.node_id_, cnt);
	  }
	  
#if defined(DEBUG_DYNAMIC_BRANCHING)
	  if (dyn_debug >= 10) {
	    if (cnt > 0) {
	      std::string tree1 = getTree(branchingTree);
	      printf("=====================================\n");
	      printf("It can move node %i up. way_: 0x%x   brvar: %i\n",
		     node.node_id_, node.way_, node.variable_);
	      printTree(tree0, cnt+10);
	      printf("=====================================\n");
	      printf("Finished moving the node up by %i levels.\n", cnt);
	      printTree(tree1, cnt+10);
	      printf("=====================================\n");
	    }
	  }
#endif
	}
	if ((numberNodes%10)==0) 
	  printf("%d nodes, tree size %d\n",
		 numberNodes,branchingTree.size());
	if (CoinCpuTime()-time1>3600.0) {
	  printf("stopping after 3600 seconds\n");
	  exit(77);
	}
	DBNodeSimple newNode(numberNodes, model,numberIntegers,which,ws);
	// something extra may have been fixed by strong branching
	// if so go round again
	while (newNode.variable_==numberIntegers) {
	  model.resolve();
	  newNode = DBNodeSimple(numberNodes, model,numberIntegers,which,model.getWarmStart());
	  newNode.strong_branching_fixed_vars_ = true;
	}
	newNode.reduced_cost_fixed_vars_ = did_reduced_cost_fixing_for_child;
	if (newNode.objectiveValue_<1.0e100) {
	  newNode.parent_ = kNode;
	  // push on stack
	  branchingTree.push_back(newNode);
#if defined(DEBUG_DYNAMIC_BRANCHING)
	  if (dyn_debug >= 1000) {
	    printf("Added current child %d %d\n",branchingTree.size(),branchingTree.size_);
	  }
#endif
	  if (branchingTree.nodes_[kNode].workingOnDownChild()) {
	    branchingTree.nodes_[kNode].child_down_ = branchingTree.last_;
	  } else {
	    branchingTree.nodes_[kNode].child_up_ = branchingTree.last_;
	  }
	  if (newNode.variable_>=0) {
	    assert (fabs(newNode.value_-floor(newNode.value_+0.5))>1.0e-6);
	  }
#if 0
	  else {
	    // integer solution - save
	    bestNode = node;
	    // set cutoff (hard coded tolerance)
	    model.setDblParam(OsiDualObjectiveLimit,(bestNode.objectiveValue_-1.0e-5)*direction);
	    std::cout<<"Integer solution of "
		     <<bestNode.objectiveValue_
		     <<" found after "<<numberIterations
		     <<" iterations and "<<numberNodes<<" nodes"
		     <<std::endl;
	  }
#endif
	}
#if defined(DEBUG_DYNAMIC_BRANCHING)
	if (dyn_debug >= 1) {
	  branchingTree.checkTree();
	}
#endif
      }
    }
    ////// End main while of branch and bound
    std::cout<<"Search took "
             <<numberIterations
             <<" iterations and "<<numberNodes<<" nodes"
             <<std::endl;
    if (bestNode.numberIntegers_) {
      // we have a solution restore
      // do bounds
      for (i=0;i<numberIntegers;i++) {
        iColumn=which[i];
        model.setColBounds( iColumn,bestNode.lower_[i],bestNode.upper_[i]);
      }
      // move basis
      model.setWarmStart(bestNode.basis_);
      // set cutoff so will be good (hard coded tolerance)
      model.setDblParam(OsiDualObjectiveLimit,(bestNode.objectiveValue_+1.0e-5)*direction);
      model.resolve();
    } else {
      OsiClpSolverInterface* clp =
	dynamic_cast<OsiClpSolverInterface*>(&model);
      if (clp) {
	ClpSimplex* modelPtr_ = clp->getModelPtr();
	modelPtr_->setProblemStatus(1);
      }
    }
    delete [] which;
    delete [] originalLower;
    delete [] originalUpper;
    delete [] relaxedLower;
    delete [] relaxedUpper;
  } else {
    std::cout<<"The LP relaxation is infeasible"
             <<std::endl;
    OsiClpSolverInterface* clp =
      dynamic_cast<OsiClpSolverInterface*>(&model);
    if (clp) {
      ClpSimplex* modelPtr_ = clp->getModelPtr();
      modelPtr_->setProblemStatus(1);
    }
    //throw CoinError("The LP relaxation is infeasible or too expensive",
    //"branchAndBound", "OsiClpSolverInterface");
  }
}



int main(int argc, char* argv[])
{
  OsiClpSolverInterface model;
  model.readMps(argv[1]);
  branchAndBound(model);
  return 0;
}
