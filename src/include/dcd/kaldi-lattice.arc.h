// kaldi-lattice-arc.h
// Original copyright information from kaldi
// fstext/lattice-weight.h
// Copyright 2009-2012  Microsoft Corporation
//                      Johns Hopkins University (author: Daniel Povey)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

//Just a cut and paste job on the Kaldi lattice weight types for standalone
//Kaldi compatability
//
#ifndef FST_LATTICE_ARC_H__
#define FST_LATTICE_ARC_H__

#include <fstream>
#include <fst/float-weight.h>

//TODO Probably not a good idea, polluting the global namespace
using namespace std;
using namespace fst;

namespace fst {

  template<class FloatType>
  class KaldiLatticeWeightTpl;

  template <class FloatType>
	inline ostream &operator <<(ostream &strm, const KaldiLatticeWeightTpl<FloatType> &w);

	template <class FloatType>
	inline istream &operator >>(istream &strm, KaldiLatticeWeightTpl<FloatType> &w1);

  template<class FloatType>
  class KaldiLatticeWeightTpl {
  public:
    typedef FloatType T; // normally float.
    typedef KaldiLatticeWeightTpl ReverseWeight;

    inline T Value1() const { return value1_; }

    inline T Value2() const { return value2_; }

    inline void SetValue1(T f) { value1_ = f; }

    inline void SetValue2(T f) { value2_ = f; }

    KaldiLatticeWeightTpl() { }

    KaldiLatticeWeightTpl(T a, T b): value1_(a), value2_(b) {}

    KaldiLatticeWeightTpl(const KaldiLatticeWeightTpl &other): value1_(other.value1_), value2_(other.value2_) { }

    KaldiLatticeWeightTpl &operator=(const KaldiLatticeWeightTpl &w) {
      value1_ = w.value1_;
      value2_ = w.value2_;
      return *this;
    }

    KaldiLatticeWeightTpl<FloatType> Reverse() const {
      return *this;
    }

#ifdef _MSC_VER
		static const KaldiLatticeWeightTpl Zero() {
			return KaldiLatticeWeightTpl(FloatLimits<T>::PosInfinity(), FloatLimits<T>::kPosInfinity);
		}
#else
    static const KaldiLatticeWeightTpl Zero() {
      return KaldiLatticeWeightTpl(FloatLimits<T>::PosInfinity(), FloatLimits<T>::PosInfinity());
    }
#endif

    static const KaldiLatticeWeightTpl One() {
      return KaldiLatticeWeightTpl(0.0, 0.0);
    }

    static const string &Type() {
      static const string type = (sizeof(T) == 4 ? "lattice4" : "lattice8") ;
      return type;
    }

    bool Member() const {
      // value1_ == value1_ tests for NaN.
      // also test for no -inf, and either both or neither
      // must be +inf, and
      if (value1_ != value1_ || value2_ != value2_) return false; // NaN
      if (value1_ == FloatLimits<T>::NegInfinity()  ||
        value2_ == FloatLimits<T>::NegInfinity()) return false; // -infty not allowed
      if (value1_ == FloatLimits<T>::PosInfinity() ||
        value2_ == FloatLimits<T>::PosInfinity()) {
          if (value1_ != FloatLimits<T>::PosInfinity() ||
            value2_ != FloatLimits<T>::PosInfinity()) return false; // both must be +infty;
          // this is necessary so that the semiring has only one zero.
      }
      return true;
    }

    KaldiLatticeWeightTpl Quantize(float delta = kDelta) const {
      if (value1_+value2_ == FloatLimits<T>::NegInfinity()) {
        return KaldiLatticeWeightTpl(FloatLimits<T>::NegInfinity(), FloatLimits<T>::NegInfinity());
      } else if (value1_+value2_ == FloatLimits<T>::PosInfinity()) {
        return KaldiLatticeWeightTpl(FloatLimits<T>::PosInfinity(), FloatLimits<T>::PosInfinity());
      } else if (value1_+value2_ != value1_+value2_) { // NaN
        return KaldiLatticeWeightTpl(value1_+value2_, value1_+value2_);
      } else {
        return KaldiLatticeWeightTpl(floor(value1_/delta + 0.5F)*delta, floor(value2_/delta + 0.5F) * delta);
      }
    }
    static uint64 Properties() {
      return kLeftSemiring | kRightSemiring | kCommutative |
        kPath | kIdempotent;
    }

    // This is used in OpenFst for binary I/O.  This is OpenFst-style,
    // not Kaldi-style, I/O.
    istream &Read(istream &strm) {
      // Always read/write as float, even if T is double,
      // so we can use OpenFst-style read/write and still maintain
      // compatibility when compiling with different FloatTypes
      ReadType(strm, &value1_);
      ReadType(strm, &value2_);
      return strm;
    }


    // This is used in OpenFst for binary I/O.  This is OpenFst-style,
    // not Kaldi-style, I/O.
    ostream &Write(ostream &strm) const {
      WriteType(strm, value1_);
      WriteType(strm, value2_);
      return strm;
    }

    size_t Hash() const {
      size_t ans;
      union {
        T f;
        size_t s;
      } u;
      u.s = 0;
      u.f = value1_;
      ans = u.s;
      u.f = value2_;
      ans += u.s;
      return ans;
    }


  static const KaldiLatticeWeightTpl<T> NoWeight() {
    return KaldiLatticeWeightTpl<T>(FloatLimits<T>::NumberBad(), 
        FloatLimits<T>::NumberBad()); 
  }

  protected:
    inline static void WriteFloatType(ostream &strm, const T &f) {
      if (f == FloatLimits<T>::PosInfinity())
        strm << "Infinity";
      else if (f == FloatLimits<T>::NegInfinity())
        strm << "-Infinity";
      else if (f != f)
        strm << "BadNumber";
      else
        strm << f;
    }

    // Internal helper function, used in ReadNoParen.
    inline static void ReadFloatType(istream &strm, T &f) {
      string s;
      strm >> s;
      if (s == "Infinity") {
        f = FloatLimits<T>::PosInfinity();
      } else if (s == "-Infinity") {
        f = FloatLimits<T>::NegInfinity();
      } else if (s == "BadNumber") {
        f = FloatLimits<T>::PosInfinity();
        f -= f;; // get NaN
      } else {
        char *p;
        f = strtod(s.c_str(), &p);
        if (p < s.c_str() + s.size())
          strm.clear(std::ios::badbit);
      }
    }

    // Reads KaldiLatticeWeight when there are no parentheses around pair terms...
    // currently the only form supported.
    inline istream &ReadNoParen(
      istream &strm, char separator) {
        int c;
        do {
          c = strm.get();
        } while (isspace(c));

        string s1;
        while (c != separator) {
          if (c == EOF) {
            strm.clear(std::ios::badbit);
            return strm;
          }
          s1 += c;
          c = strm.get();
        }
        istringstream strm1(s1);
        ReadFloatType(strm1, value1_); // ReadFloatType is class member function
        // read second element
        ReadFloatType(strm, value2_);
        return strm;
    }

    //TODO(PAUL) fix these!
  friend istream &operator>> <FloatType>(istream&, KaldiLatticeWeightTpl<FloatType>&);
  friend ostream &operator<< <FloatType>(ostream&, const KaldiLatticeWeightTpl<FloatType>&);

  private:
    T value1_;
    T value2_;

  };
  
  template <class FloatType>
	inline ostream &operator <<(ostream &strm, const KaldiLatticeWeightTpl<FloatType> &w) {
		typedef FloatType T;
		KaldiLatticeWeightTpl<FloatType>::WriteFloatType(strm, w.Value1());
		CHECK(FLAGS_fst_weight_separator.size() == 1);
		strm << FLAGS_fst_weight_separator[0]; // comma by default;
		// may or may not be settable from Kaldi programs.
		KaldiLatticeWeightTpl<FloatType>::WriteFloatType(strm, w.Value2());
		return strm;
	}

	template <class FloatType>
	inline istream &operator >>(istream &strm, KaldiLatticeWeightTpl<FloatType> &w1) {
		CHECK(FLAGS_fst_weight_separator.size() == 1);
		// separator defaults to ','
		return w1.ReadNoParen(strm, FLAGS_fst_weight_separator[0]);
	}

  typedef float BaseFloat;
  typedef float FloatType;
  typedef KaldiLatticeWeightTpl<BaseFloat> KaldiLatticeWeight;
  typedef KaldiLatticeWeightTpl<BaseFloat> KaldiKaldiLatticeWeight;

  typedef fst::ArcTpl<KaldiLatticeWeight> KaldiLatticeArc;

	inline bool operator==(const KaldiLatticeWeightTpl<FloatType> &wa,
		const KaldiLatticeWeightTpl<FloatType> &wb) {
		// Volatile qualifier thwarts over-aggressive compiler optimizations
		// that lead to problems esp. with NaturalLess().
		volatile FloatType va1 = wa.Value1(), va2 = wa.Value2(),
			vb1 = wb.Value1(), vb2 = wb.Value2();
		return (va1 == vb1 && va2 == vb2);
	}

	template<class FloatType>
	inline bool operator!=(const KaldiLatticeWeightTpl<FloatType> &wa,
		const KaldiLatticeWeightTpl<FloatType> &wb) {
		// Volatile qualifier thwarts over-aggressive compiler optimizations
		// that lead to problems esp. with NaturalLess().
		volatile FloatType va1 = wa.Value1(), va2 = wa.Value2(),
			vb1 = wb.Value1(), vb2 = wb.Value2();
		return (va1 != vb1 || va2 != vb2);
	}


	// We define a Compare function KaldiLatticeWeightTpl even though it's
	// not required by the semiring standard-- it's just more efficient
	// to do it this way rather than using the NaturalLess template.

	/// Compare returns -1 if w1 < w2, +1 if w1 > w2, and 0 if w1 == w2.

	template<class FloatType>
	inline int Compare(const KaldiLatticeWeightTpl<FloatType> &w1,
		const KaldiLatticeWeightTpl<FloatType> &w2) {
		FloatType f1 = w1.Value1() + w1.Value2(),
			f2 = w2.Value1() + w2.Value2();
		if (f1 < f2) { return 1; } // having smaller cost means you're larger
		// in the semiring [higher probability]
		else if (f1 > f2) { return -1; }
		// mathematically we should be comparing (w1.value1_-w1.value2_ < w2.value1_-w2.value2_)
		// in the next line, but add w1.value1_+w1.value2_ = w2.value1_+w2.value2_ to both sides and
		// divide by two, and we get the simpler equivalent form w1.value1_ < w2.value1_.
		else if (w1.Value1() < w2.Value1()) { return 1; }
		else if (w1.Value1() > w2.Value1()) { return -1; }
		else { return 0; }
	}


	template<class FloatType>
	inline KaldiLatticeWeightTpl<FloatType> Plus(const KaldiLatticeWeightTpl<FloatType> &w1,
		const KaldiLatticeWeightTpl<FloatType> &w2) {
		return (Compare(w1, w2) >= 0 ? w1 : w2);
	}


	// For efficiency, override the NaturalLess template class.  
	template<class FloatType>
	class NaturalLess<KaldiLatticeWeightTpl<FloatType> > {
	public:
		typedef KaldiLatticeWeightTpl<FloatType> Weight;
		bool operator()(const Weight &w1, const Weight &w2) const {
			// NaturalLess is a negative order (opposite to normal ordering).
			// This operator () corresponds to "<" in the negative order, which
			// corresponds to the ">" in the normal order.
			return (Compare(w1, w2) == 1);
		}
	};

	template<class FloatType>
	inline KaldiLatticeWeightTpl<FloatType> Times(const KaldiLatticeWeightTpl<FloatType> &w1,
		const KaldiLatticeWeightTpl<FloatType> &w2) {
		return KaldiLatticeWeightTpl<FloatType>(w1.Value1() + w2.Value1(), w1.Value2() + w2.Value2());
	}

	// divide w1 by w2 (on left/right/any doesn't matter as
	// commutative).
	template<class FloatType>
	inline KaldiLatticeWeightTpl<FloatType> Divide(const KaldiLatticeWeightTpl<FloatType> &w1,
		const KaldiLatticeWeightTpl<FloatType> &w2,
		DivideType typ = DIVIDE_ANY) {
		typedef FloatType T;
		T a = w1.Value1() - w2.Value1(), b = w1.Value2() - w2.Value2();
		if (a != a || b != b || a == -numeric_limits<T>::infinity()
			|| b == -numeric_limits<T>::infinity()) {
			std::cerr << "KaldiLatticeWeightTpl::Divide, NaN or invalid number produced. "
				<< "[dividing by zero?]  Returning zero.";
			return KaldiLatticeWeightTpl<T>::Zero();
		}
		if (a == numeric_limits<T>::infinity() ||
			b == numeric_limits<T>::infinity())
			return KaldiLatticeWeightTpl<T>::Zero(); // not a valid number if only one is infinite.
		return KaldiLatticeWeightTpl<T>(a, b);
	}


	template<class FloatType>
	inline bool ApproxEqual(const KaldiLatticeWeightTpl<FloatType> &w1,
		const KaldiLatticeWeightTpl<FloatType> &w2,
		float delta = kDelta) {
		if (w1.Value1() == w2.Value1() && w1.Value2() == w2.Value2()) return true;  // handles Zero().
		return (fabs((w1.Value1() + w1.Value2()) - (w2.Value1() + w2.Value2())) <= delta);
	}
} //namespace fst

#endif
