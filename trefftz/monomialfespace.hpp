#ifndef FILE_MONOMIALFESPACE_HPP
#define FILE_MONOMIALFESPACE_HPP
#include "monomialfe.hpp"


namespace ngcomp
{

    class MonomialFESpace : public FESpace
    {
        int D;
        int fullD;
        int order;
        size_t ndof;
        int nel;
        int nvert;
        int local_ndof;
        float c=1;
        int useshift=1;
        int useqt=0;
        shared_ptr<CoefficientFunction> wavespeedcf;

        public:


        MonomialFESpace (shared_ptr<MeshAccess> ama, const Flags & flags);

        void SetWavespeed(shared_ptr<CoefficientFunction> awavespeedcf) {
            wavespeedcf=awavespeedcf;
        }

        string GetClassName () const override { return "monomialfespace"; }

        void GetDofNrs (ElementId ei, Array<DofId> & dnums) const override;

        FiniteElement & GetFE (ElementId ei, Allocator & alloc) const override;

        size_t GetNDof () const override { return this->ndof; }

        static DocInfo GetDocu ();

        protected:

        template<int D>
        double Adiam(ElementId ei, double c) const;

        template<int D>
        double Adiam(ElementId ei, shared_ptr<CoefficientFunction> c) const;

        template<int D>
        Vec<D+1> ElCenter(ElementId ei) const;
    };
}

#ifdef NGS_PYTHON
#include <python_ngstd.hpp>
void ExportMonomialFESpace(py::module m);
#endif // NGS_PYTHON

#endif
