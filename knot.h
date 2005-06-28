/*****
 * knot.h
 * Andy Hammerlindl 200/02/10
 *
 * Describes a knot, a point and its neighbouring specifiers, used as an
 * intermediate structure in solving paths.
 *****/

#ifndef KNOT_H
#define KNOT_H

#include <iostream>
#include <vector>
#include <algorithm>

#include "mod.h"
#include "pair.h"
#include "path.h"

namespace camp {

using mem::vector;

// The choice of branch cuts of atan2 disguishes between y=+0.0 and y=-0.0 in
// the case where x<0.  This can lead to strange looking paths being
// calculated from guides of the form a..b..cycle.  To avoid these degenerate
// cases, the niceAngle routine moves the branch cut so that the sign of a
// zero won't matter.
double niceAngle(pair z);

// A cyclic vector: ie. a vector where the index is taken mod the size of the
// vector.
template <typename T>
class cvector : public vector<T> {
public:
  cvector() {}
  cvector(size_t n) : vector<T>(n) {}
  cvector(size_t n, const T& t) : vector<T>(n,t) {}
  cvector(const vector<T>& v) : vector<T>(v) {}
  
  T& operator[](int j) {
    return vector<T>::operator[](imod(j,(int) this->size()));
  }
  const T& operator[](int j) const {
    return vector<T>::operator[](imod(j,(int) this->size()));
  }
};

// Forward declaration.
class knotlist;

/* A linear equation (one of a set of equations to solve for direction through
   knots in a path).  The i-th equation is:
  
      pre*theta[i-1] + piv*theta[i] + post*theta[i+1] = aug
  
    where indices are taken mod n.
*/
struct eqn {
  double pre,piv,post,aug;
  eqn(double pre, double piv, double post, double aug)
    : pre(pre), piv(piv), post(post), aug(aug) {}

  friend ostream& operator<< (ostream& out, const eqn& e)
  {
    return out << e.pre << " * pre + "
               << e.piv << " * piv + "  
               << e.post << " * post = "
               << e.aug;
  }
};


// A direction specifier, telling how the path behaves coming in or out of a
// point.  The base class represents the "open" specifier.
class spec : public gc {
public:
  virtual ~spec() {}
  
  // If the knot is open, it gives no restriction on the behavior of the
  // path.
  virtual bool open() { return true; }
  virtual bool controlled() { return false; }

  // When a knot has a restriction on one side and open on the other, the
  // restriction implies a restriction on the other side.  This is the partner
  // restriction defined here, where the pair argument is for the location of
  // the knot.
  virtual spec *outPartner(pair) { return this; }
  virtual spec *inPartner(pair) { return this; }

  virtual void print(ostream&) const {}
};

inline ostream& operator<< (ostream& out, spec& s)
{
  s.print(out);
  return out;
}


// Specifier used at an endpoint.
class endSpec : public spec {
public:
  bool open() { return false; }

  // Returns an equation used to solve for the thetas along the knot.  These are
  // called by eqnprop in the non-cyclic case for the first and last equations.
  virtual eqn eqnOut(int j, knotlist& l,
                     cvector<double>& d, cvector<double>& psi) = 0;
  virtual eqn eqnIn (int j, knotlist& l,
                     cvector<double>& d, cvector<double>& psi) = 0;
};

// A specifier with a given direction (in radians).
class dirSpec : public endSpec {
  double given;
public:
  // Direction should be given in the range [-PI,PI]
  dirSpec(double given)
    : given(given) {}
  dirSpec(pair z)
    : given(niceAngle(z)) {}

  eqn eqnOut(int j, knotlist& l, cvector<double>& d, cvector<double>& psi);
  eqn eqnIn (int j, knotlist& l, cvector<double>& d, cvector<double>& psi);

  void print(ostream& out) const {
    out << "{dir(" << given << ")}";
  }
};

// A curl specifier.  The curvature at the end knot should be gamma times the
// curvature at the neighbouring knot.
class curlSpec : public endSpec {
  double gamma;
public:
  // Gamma should be non-negative.
  curlSpec(double gamma=1.0)
    : gamma(gamma) {}

  eqn eqnOut(int j, knotlist& l, cvector<double>& d, cvector<double>& psi);
  eqn eqnIn (int j, knotlist& l, cvector<double>& d, cvector<double>& psi);

  void print(ostream& out) const {
    out << "{curl " << gamma << "}";
  }
};


// A specifier with a control point.  All information for this portion of the
// curve has been determined.
class controlSpec : public spec {
  pair cz;
public:
  controlSpec(pair cz)
    : cz(cz) {}
    
  bool open() { return false; }
  bool controlled() { return true; }
  pair control() { return cz; }

  // The partner spec will be a dirSpec in the same direction the specifier
  // takes the path, unless the velocity is zero, then it uses a curl
  // specifier.
  spec *outPartner(pair);
  spec *inPartner(pair);

  void print(ostream& out) const {
    // NOTE: This format cannot be read back in.
    out << "{control " << cz << "}";
  }
};


// The tension information for one side of a knot.
struct tension {
  double val;
  bool atleast;

  tension(double val=1.0, bool atleast=false)
    : val(val), atleast(atleast) {}
};

inline
ostream& operator<<(ostream& out, tension t)
{
  return out << "tension" << (t.atleast ? " atleast " : " ") << t.val;
}

// A knot, a point with specifiers to double the path coming in and going out
// of the knot.
struct knot {
  pair z;
  spec *in;
  spec *out;
  tension tin, tout;

  static spec defaultOpen;

  knot() {}
  knot(pair z, spec *in=&defaultOpen, spec *out=&defaultOpen,
       tension tin=tension(), tension tout=tension())
    : z(z), in(in), out(out), tin(tin), tout(tout) {}

  double alpha() { return 1.0/tout.val; }
  double beta() { return 1.0/tin.val; }
};

ostream& operator<<(ostream& out, const knot& k);

// Abstract base class for a section of a guide.
class knotlist {
public:
  virtual ~knotlist() {}
  
  virtual int length() = 0;
  virtual bool cyclic() = 0;

  // Returns the number of knots.
  int size() {
    return cyclic() ? length() : length() + 1;
  }

  virtual knot& cell(int) = 0;
  virtual knot& operator[] (int i) {
#if 0
    assert(cyclic() || (0 <= i && i <= length()));  // Bounds check.
#endif
    return cell(i);
  }
  knot& front() {
    return (*this)[0];
  }
  knot& back() {
    return (*this)[length()];
  }
};


// Defines a knotlist as a piece of another knotlist.
class subknotlist : public knotlist {
  knotlist& l;
  int a,b;
public:
  subknotlist(knotlist& l, int a, int b)
    : l(l), a(a), b(b) {}

  int length() { return b-a; }
  bool cyclic() { return false; }
  knot& cell(int i) { return l[a+i]; }
};

struct simpleknotlist : public knotlist {
  cvector<knot> nodes;
  bool cycles;

  simpleknotlist(cvector<knot> nodes, bool cycles=false)
    : nodes(nodes), cycles(cycles) {}

  int length() { return cycles ? (int) nodes.size() : (int) nodes.size() - 1; }
  bool cyclic() { return cycles; }
  knot& cell(int j) { return nodes[j]; }
};
    
// A protopath is a path being made.
struct protopath {
  bool cycles;
  int n;
  solvedKnot *nodes;

  protopath(int n, bool cycles)
    : cycles(cycles), n(n), nodes(new solvedKnot[n]) {}

  solvedKnot& operator[](int j) {
    return nodes[imod(j,n)];
  }

  bool& straight(int j) {
    return (*this)[j].straight;
  }
  pair& pre(int j) {
    return (*this)[j].pre;
  }
  pair& point(int j) {
    return (*this)[j].point;
  }
  pair& post(int j) {
    return (*this)[j].post;
  }

  void controlEnds() {
    if (!cycles) {
      solvedKnot& start=(*this)[0];
      solvedKnot& end=(*this)[n-1];

      start.pre=start.point;
      end.post=end.point;
    }
  }
  // Once all the control are set, return the final (constant) path.
  path fix() {
    return path(nodes,n,cycles);
  }
};


// Represents properties that can be computed along a knotlist.
// Examples include distances (d), turning angles (psi), and the linear
// equations used to solve for the thetas.
template <typename T>
class knotprop {
protected:
  knotlist& l;

  // Calculate the property for the usual case in the iteration (and for a
  // cyclic knot, the only case), at the index given.
  virtual T mid(int) = 0;

  // The special cases, these default to the usual case: mid.
  virtual T solo(int j)     // Calculates the property for a list of length 0.
  {
    return mid(j);
  }
  virtual T start(int j)    // Calculates it at the start of the list.
  {
    return mid(j);
  }
  virtual T end(int j)      // Calculate it at the end.
  {
    return mid(j);
  }

  virtual cvector<T> linearCompute()
  {
    int n=l.length();
    cvector<T> v;
    if (n==0)
      v.push_back(solo(0));
    else {
      v.push_back(start(0));
      for (int j=1; j<n; ++j)
        v.push_back(mid(j));
      v.push_back(end(n));
    }
    return v;
  }
  
  virtual cvector<T> cyclicCompute()
  {
    int n=l.length();
    cvector<T> v;
    for (int j=0; j<n; ++j)
      v.push_back(mid(j));
    return v;
  }

  virtual cvector<T> linearBackCompute()
  {
    int n=l.length();
    cvector<T> v;
    if (n==0)
      v.push_back(solo(0));
    else {
      v.push_back(end(n));
      for (int j=1; j<n; ++j)
        v.push_back(mid(n-j));
      v.push_back(start(0));
    }
    return v;
  }
  
  virtual cvector<T> cyclicBackCompute()
  {
    int n=l.length();
    cvector<T> v;
    for (int j=1; j<=n; ++j)
      v.push_back(mid(n-j));
    return v;
  }

public:
  virtual ~knotprop() {}
  
  virtual cvector<T> compute() {
    return l.cyclic() ? cyclicCompute() : linearCompute();
  }

  // Compute the values in the opposite order.  This is needed for instance if
  // the i-th calculation needed a result computed in the i+1-th, such as in the
  // back substitution for solving thetas.
  virtual cvector<T> backCompute() {
    cvector<T> v=l.cyclic() ? cyclicBackCompute() : linearBackCompute();

    // Even though they are computed in the backwards order, return them in the
    // standard order.
    reverse(v.begin(),v.end());
    return v;
  }

  knotprop(knotlist& l)
    : l(l) {}
};

// A knot transforms, it takes in one knotlist and transforms it knot for knot
// into a new one.
class knottrans : public knotprop<knot> {
protected:
  virtual knot mid(int j) {
    /* By default, just copy the knot. */
    return l[j];
  }

public:
  virtual ~knottrans() {}
  
  knottrans(knotlist& l)
    : knotprop<knot>(l) {}

  virtual simpleknotlist trans() {
    return simpleknotlist(compute(),l.cyclic());
  }
};

// Like a knotprop, but it doesn't compute a vector of values for the knot.  It
// iterates over the knotlist calling method for side-effect.  For instance,
// this is used to plug control points into protopaths.
class knoteffect {
protected:
  knotlist& l;

  virtual void mid(int) = 0;

  // The special cases, these default to the usual case: mid.
  virtual void solo(int j) {
    mid(j);
  }
  virtual void start(int j) {
    mid(j);
  }
  virtual void end(int j) {
    mid(j);
  }

  virtual void linearExec()
  {
    int n=l.length();
    if (n==0)
      solo(0);
    else {
      start(0);
      for (int j=1; j<n; ++j)
        mid(j);
      end(n);
    }
  }
  
  virtual void cyclicExec()
  {
    int n=l.length();
    for (int j=0; j<n; ++j)
      mid(j);
  }

  virtual void linearBackExec()
  {
    int n=l.length();
    if (n==0)
      solo(0);
    else {
      end(n);
      for (int j=1; j<n; ++j)
        mid(n-j);
      start(0);
    }
  }
  
  virtual void cyclicBackExec()
  {
    int n=l.length();
    for (int j=1; j<=n; ++j)
      mid(n-j);
  }

public:
  virtual ~knoteffect() {}
  
  virtual void exec() {
    if (l.cyclic())
      cyclicExec();
    else
      linearExec();
  }

  virtual void backCompute() {
    if (l.cyclic())
      cyclicBackExec();
    else
      linearBackExec();
  }

  knoteffect(knotlist& l)
    : l(l) {}
};

path solve(knotlist& l);

path solveSimple(cvector<pair>& z);

} // namespace camp

#endif // KNOT_H
