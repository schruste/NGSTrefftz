#ifndef FILE_TREFFTZELEMENT_HPP
#define FILE_TREFFTZELEMENT_HPP

#include <fem.hpp>
#include "helpers.hpp"
#include "scalarmappedfe.hpp"

namespace ngfem
{
    template <int D>
    class TrefftzWaveFE : public ScalarMappedElement<D>
    {
        private:
            const int ord;
            const int nbasis;
            const int npoly;
            const Matrix<int> pascal;
            const int basistype;
            Vec<D> elcenter=0;
            double elsize=1;
            float c = 1.0;
            ELEMENT_TYPE eltype;

        public:
            // TrefftzWaveFE();
            TrefftzWaveFE(int aord = 1, float ac = 1.0, ELEMENT_TYPE aeltype = ET_TRIG, int abasistype = 0);

            virtual ELEMENT_TYPE ElementType() const { return eltype; }

            using ScalarMappedElement<D>::CalcShape;
            virtual void CalcShape (const BaseMappedIntegrationPoint & mip, BareSliceVector<> shape) const;
            virtual void CalcShape (const SIMD_MappedIntegrationRule<D-1,D> & smir, BareSliceMatrix<SIMD<double>> shape) const;

            using ScalarMappedElement<D>::CalcDShape;
            virtual void CalcDShape (const BaseMappedIntegrationPoint & mip, SliceMatrix<> dshape) const;
            virtual void CalcDShape (const SIMD_MappedIntegrationRule<D-1,D> & smir, SliceMatrix<SIMD<double>> dshape) const;

            int GetNBasis() const { return nbasis; }

            TrefftzWaveFE<D> * SetCenter(Vec<D> acenter) {elcenter = acenter; return this;}
            TrefftzWaveFE<D> * SetElSize(double aelsize) {elsize = aelsize; return this;}
            // TrefftzWaveFE<D> * SetWavespeed(float ac) {c = ac; return this;}

        protected:
            void MakeIndices_inner(Matrix<int> &indice, Vec<D, int> numbers, int &count, int ordr, int dim) const;
            Matrix<int> MakeIndices() const;

            constexpr int IndexMap(Vec<D, int> index) const;
            Matrix<double> TrefftzBasis() const;
            Matrix<double> GetDerTrefftzBasis(int der) const;
            Matrix<int> pascal_sym() const;


            Matrix<> TB() const;
            void TB_inner(Matrix<> &trefftzbasis, Vec<D, int> coeffnum, int nbasis, int ordr, int dim, int &tracker) const;
    class Monomial : public RecursivePolynomial<Monomial>
    {
        public:
            Monomial () { ; }

            template <class S, class T>
                inline Monomial (int n, S x, T && values)
                {
                    Eval (n, x, values);
                }

            template <class S>
                static INLINE double P0(S x)  { return 1.0; }
            template <class S>
                static INLINE S P1(S x)  { return x; }
            template <class S, class Sy>
                static INLINE S P1(S x, Sy y)  { return P1(x); }

            static INLINE double A (int i) { return 1.0; }
            static INLINE double B (int i) { return 0; }
            static INLINE double C (int i) { return 0; }

            static INLINE double CalcA (int i) { return 1.0; }
            static INLINE double CalcB (int i) { return 0; }
            static INLINE double CalcC (int i) { return 0; }

            enum { ZERO_B = 1 };
    };
}


#ifdef NGS_PYTHON
#include <python_ngstd.hpp>
void ExportTrefftzElement(py::module m);
#endif // NGS_PYTHON


#endif // FILE_TrefftzElement_HPP
