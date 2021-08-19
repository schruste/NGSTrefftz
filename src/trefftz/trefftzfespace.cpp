#include <comp.hpp>    // provides FESpace, ...
#include <h1lofe.hpp>
#include <regex>
#include <python_comp.hpp>
#include <fem.hpp>
#include <multigrid.hpp>

#include "trefftzfespace.hpp"
#include "diffopmapped.hpp"
#include "trefftzheatfe.hpp"

namespace ngcomp
{

    TrefftzFESpace :: TrefftzFESpace (shared_ptr<MeshAccess> ama, const Flags & flags)
        : FESpace (ama, flags)
    {
        type="trefftzfespace";

        //cout << "======== Constructor of TrefftzFESpace =========" << endl;
        //cout << "Flags:" << endl << flags;

        D = ma->GetDimension() - 1;

        this->dgjumps = true;
        heat = flags.GetNumFlag("heat",0);
        heattest = flags.GetNumFlag("heattest",0);
        order = int(flags.GetNumFlag ("order", 3)) * (1+(heat>0));
        c = flags.GetNumFlag ("wavespeed", 1);
        wavespeedcf = make_shared<ConstantCoefficientFunction>(c);
        basistype = flags.GetNumFlag ("basistype", 0);
        useshift = flags.GetNumFlag("useshift",1);
        usescale = flags.GetNumFlag("usescale",1);
        useqt = flags.GetNumFlag("useqt",0);


        local_ndof = BinCoeff(D + order, order) + BinCoeff(D + order-1, order-1) * (!heat);
        nel = ma->GetNE();
        ndof = local_ndof * nel;

        switch (D)
        {
            case 1:
                {
                    evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpMapped<2>>>();
                    flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpMappedGradient<2>>>();
                    additional_evaluators.Set ("hesse", make_shared<T_DifferentialOperator<DiffOpMappedHesse<2>>> ());
                    basismat = TWaveBasis<1>::Basis(order, basistype);
                    basis = new QTWaveBasis<1>;
                    break;
                }
            case 2:
                {
                    evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpMapped<3>>>();
                    flux_evaluator[VOL] = make_shared<T_DifferentialOperator<DiffOpMappedGradient<3>>>();
                    additional_evaluators.Set ("hesse", make_shared<T_DifferentialOperator<DiffOpMappedHesse<3>>> ());
                    basismat = TWaveBasis<2>::Basis(order, basistype);
                    basis = new QTWaveBasis<2>;
                    break;
                }
        }
    }

    void TrefftzFESpace :: SetWavespeed(shared_ptr<CoefficientFunction> awavespeedcf, shared_ptr<CoefficientFunction> aBBcf, shared_ptr<CoefficientFunction> aGGcf)
    {
        wavespeedcf=awavespeedcf;
        if(aBBcf || useqt)
        {
            //wavespeedcf = UnaryOpCF(aBBcf/awavespeedcf,GenericSqrt());
            cout << "started auto diff.... ";
            shared_ptr<CoefficientFunction> GGcf = make_shared<ConstantCoefficientFunction>(1)/(awavespeedcf*awavespeedcf);
            shared_ptr<CoefficientFunction> GGcfx = make_shared<ConstantCoefficientFunction>(1)/(awavespeedcf*awavespeedcf);
            if(aGGcf || useqt)
            {
                GGcf = aGGcf;
                GGcfx = aGGcf;
            }

            static Timer timerder("QTrefftzDerivatives");
            static Timer timereval("QTrefftzDerEval");
            timerder.Start();
            GGder.SetSize(this->order-1,(this->order-2)*(D==2)+1);
            for(int ny=0;ny<=(this->order-2)*(D==2);ny++)
            {
                for(int nx=0;nx<=this->order-2;nx++)
                {
                    GGder(nx,ny) = GGcfx;
                    GGcfx = GGcfx->Diff(MakeCoordinateCoefficientFunction(0).get(), make_shared<ConstantCoefficientFunction>(1) );
                }
                GGcf = GGcf->Diff(MakeCoordinateCoefficientFunction(1).get(), make_shared<ConstantCoefficientFunction>(1) );
                GGcfx = GGcf;
            }
            timerder.Stop();


            if(!aBBcf){
                aBBcf = make_shared<ConstantCoefficientFunction>(1);
                cout << "SETTING BB TO 1" << endl;
            }
            static Timer timerbb("QTrefftzBB");
            timerbb.Start();
            shared_ptr<CoefficientFunction> BBcf = aBBcf;
            shared_ptr<CoefficientFunction> BBcfx = aBBcf;
            BBder.SetSize(this->order,(this->order-1)*(D==2)+1);
            for(int ny=0;ny<=(this->order-1)*(D==2);ny++)
            {
                for(int nx=0;nx<=this->order-1;nx++)
                {
                    BBder(nx,ny) = BBcfx;
                    BBcfx = BBcfx->Diff(MakeCoordinateCoefficientFunction(0).get(), make_shared<ConstantCoefficientFunction>(1) );
                }
                BBcf = BBcf->Diff(MakeCoordinateCoefficientFunction(1).get(), make_shared<ConstantCoefficientFunction>(1) );
                BBcfx = BBcf;
            }
            timerbb.Stop();
            cout << "finish" << endl;
        }
    }


    void TrefftzFESpace :: GetDofNrs (ElementId ei, Array<DofId> & dnums) const
    {
        dnums.SetSize(0);
        if (!DefinedOn (ei) || ei.VB() != VOL) return;
        // int n_vert = ma->GetNV();		int n_edge = ma->GetNEdges();		int n_cell = ma->GetNE(); Ngs_Element ngel = ma->GetElement (ei);
        for (int j = ei.Nr()*local_ndof; j<local_ndof*(ei.Nr()+1); j++)
        {
            dnums.Append (j);
        }
        //cout << "GetDofNrs: ei.Nr() = " << ei.Nr() << " local_ndof:" << local_ndof << " ndof: " << ndof << " dnums: \n" << dnums << endl <<
        //"================================================" << endl ;
    }


    FiniteElement & TrefftzFESpace :: GetFE (ElementId ei, Allocator & alloc) const
    {

        //auto vertices_index = ma->GetElVertices(ei);
        //cout << "element vectice coord: \n"  << ma->GetPoint<3>(vertices_index[0]) << endl<< ma->GetPoint<3>(vertices_index[1]) <<endl<<ma->GetPoint<3>(vertices_index[2])<<endl<<ma->GetPoint<3>(vertices_index[3])<<endl;

        Ngs_Element ngel = ma->GetElement(ei);
        ELEMENT_TYPE eltype = ngel.GetType();

        if (ei.IsVolume())
        {
            switch (ma->GetElType(ei)) {
                case ET_SEGM:
                    {
                    }
                case ET_QUAD:
                case ET_TRIG:
                    {
                        LocalHeap lh(1000 * 1000);
                        const ELEMENT_TYPE eltyp = ET_TRIG ;
                        const int D=2;
                        IntegrationRule ir (eltyp, 0);
                        MappedIntegrationPoint<D,D> mip(ir[0], ma->GetTrafo (ElementId(0), lh));
                        mip.Point() = ElCenter<1>(ei).Range(0,1);
                        if(BBder.Height()!=0 || useqt)
                        {
                            CSR basismat = static_cast<QTWaveBasis<1>*>(basis)->Basis(order, ElCenter<1>(ei), GGder, BBder);
                            return *(new (alloc) ScalarMappedElement<2>(local_ndof,order,basismat,eltype,ElCenter<1>(ei),1.0));
                        }
                        else
                            return *(new (alloc) ScalarMappedElement<2>(local_ndof,order,basismat,eltype,ElCenter<1>(ei),Adiam<1>(ei,c),c));
                        break;
                    }
                case ET_HEX:
                case ET_PRISM:
                case ET_PYRAMID:
                case ET_TET:
                    {
                        LocalHeap lh(1000 * 1000);
                        const ELEMENT_TYPE eltyp = ET_TRIG ;
                        const int D=3;
                        IntegrationRule ir (eltyp, 0);
                        MappedIntegrationPoint<D,D> mip(ir[0], ma->GetTrafo (ElementId(0), lh));
                        mip.Point() = ElCenter<2>(ei).Range(0,2);

                        if(BBder.Height()!=0 || useqt)
                        {
                            CSR basismat = static_cast<QTWaveBasis<2>*>(basis)->Basis(order, ElCenter<1>(ei), GGder, BBder);
                            return *(new (alloc) ScalarMappedElement<3>(local_ndof,order,basismat,eltype,ElCenter<2>(ei),1.0));
                        }
                        else
                            return *(new (alloc) ScalarMappedElement<3>(local_ndof,order,basismat,eltype,ElCenter<2>(ei),Adiam<2>(ei,c),c));
                    }
                    break;
            }
        }
        //else
        try
        {
            return SwitchET<ET_POINT,ET_SEGM,ET_TRIG,ET_QUAD>
                (eltype,
                 [&alloc] (auto et) -> FiniteElement&
                 { return * new (alloc) DummyFE<et.ElementType()>; });
        }
        catch (Exception e)
        {
            throw Exception("illegal element type in Trefftz::GetSurfaceFE");
        }
    }


    template<int D>
    double TrefftzFESpace :: Adiam(ElementId ei, double c) const
    {
        double anisotropicdiam = 0.0;
        auto vertices_index = ma->GetElVertices(ei);
        for(auto vertex1 : vertices_index)
        {
            for(auto vertex2 : vertices_index)
            {
                Vec<D+1> v1 = ma->GetPoint<D+1>(vertex1);
                Vec<D+1> v2 = ma->GetPoint<D+1>(vertex2);
                //cout << "v1: " << v1 << " v1 part: " << v1(1,D-1) << "norm " << L2Norm(v1) << endl ;
                anisotropicdiam = max( anisotropicdiam, sqrt( L2Norm2(v1.Range(0,D) - v2.Range(0,D)) + pow(c*(v1(D)-v2(D)),2) ) );
            }
        }
        return anisotropicdiam * usescale + (usescale==0);
    }


    template<int D>
    double TrefftzFESpace :: Adiam(ElementId ei, shared_ptr<CoefficientFunction> c) const
    {
        LocalHeap lh(1000 * 1000);
        double anisotropicdiam = 0.0;
        auto vertices_index = ma->GetElVertices(ei);

        for(auto vertex1 : vertices_index)
        {
            for(auto vertex2 : vertices_index)
            {
                Vec<D+1> v1 = ma->GetPoint<D+1>(vertex1);
                Vec<D+1> v2 = ma->GetPoint<D+1>(vertex2);
                //cout << "v1: " << v1 << " v1 part: " << v1.Range(0,D) << " el type: " << ma->GetElType(ei) << " norm " << L2Norm(v1) << endl ;
                IntegrationRule ir (ma->GetElType(ei), 0);
                ElementTransformation & trafo = ma->GetTrafo (ei, lh);
                MappedIntegrationPoint<D+1,D+1> mip(ir[0], trafo);
                mip.Point() = v1;
                double c1 = wavespeedcf->Evaluate(mip);
                mip.Point() = v2;
                double c2 = wavespeedcf->Evaluate(mip);

                anisotropicdiam = max( anisotropicdiam, sqrt( L2Norm2(v1.Range(0,D) - v2.Range(0,D)) + pow(c1*v1(D)-c2*v2(D),2) ) );
            }
        }
        return anisotropicdiam * usescale + (usescale==0);
    }

    template<int D>
    Vec<D+1> TrefftzFESpace :: ElCenter(ElementId ei) const
    {
        Vec<D+1> center = 0;
        auto vertices_index = ma->GetElVertices(ei);
        for(auto vertex : vertices_index) center += ma->GetPoint<D+1>(vertex);
        center *= (1.0/vertices_index.Size()) * useshift;
        return center;
    }

    DocInfo TrefftzFESpace :: GetDocu ()
    {
        auto docu = FESpace::GetDocu();
        docu.Arg("useshift") = "bool = True\n"
            "  use shift of basis functins to element center and scale them";
        docu.Arg("basistype") = "bool = True\n"
            "  use shift of basis functins to element center and scale them";
        docu.Arg("wavespeed") = "bool = True\n"
            "  use shift of basis functins to element center and scale them";
        return docu;
    }

    /*
       register fe-spaces
       Object of type TrefftzFESpace can be defined in the pde-file via
       "define fespace v -type=trefftzfespace"
       */
    static RegisterFESpace<TrefftzFESpace> initi_trefftz ("trefftzfespace");


	// k-th coeff of Legendre polynomial of degree n in monomial basis
	constexpr double LegCoeffMonBasis(int n, int k)
	{
		if(n==0) return 1;
		if(k>n) return 0;
		if((n+k)%2) return 0;
		double coeff = pow(2,-n) * pow(-1,floor((n-k)/2)) * BinCoeff(n,floor((n-k)/2)) * BinCoeff(n+k,n);
		// double coeff = pow(2,-n) * pow(-1,k) * BinCoeff(n,k) * BinCoeff(2*n-2*k,n);
		return coeff;
	}

	// k-th coeff of Chebyshev polynomial of degree n in monomial basis
	constexpr double ChebCoeffMonBasis(int n, int k)
	{
		if(n==0) return 1;
		if(k>n) return 0;
		if((n+k)%2) return 0;
		double coeff = pow(2,k-1)*n*pow(-1,floor((n-k)/2)) * tgamma((n+k)/2)/(tgamma(floor((n-k)/2)+1)*tgamma(k+1));
		return coeff;
	}

    template<int D>
    CSR TWaveBasis<D> :: Basis(int ord, int basistype)
    {
        CSR tb;
        const int ndof = (BinCoeff(D + ord, ord) + BinCoeff(D + ord-1, ord-1));
        const int npoly = (BinCoeff(D+1 + ord, ord));
        Matrix<> trefftzbasis(ndof,npoly);
        trefftzbasis = 0;
        Vec<D+1, int>  coeff = 0;
        int count = 0;
        for(int b=0;b<ndof;b++)
        {
            int tracker = 0;
            TB_inner(ord, trefftzbasis, coeff, b, D+1, tracker, basistype);
        }
        MatToCSR(trefftzbasis,tb);
        return tb;
    }


    template<int D>
    void TWaveBasis<D> :: TB_inner(int ord, Matrix<> &trefftzbasis, Vec<D+1, int> coeffnum, int basis, int dim, int &tracker, int basistype, double wavespeed)
    {
        if (dim>0)
        {
            while(coeffnum(dim-1)<=ord)
            {
                TB_inner(ord,trefftzbasis,coeffnum,basis, dim-1, tracker, basistype, wavespeed);
                coeffnum(dim-1)++;
            }
        }
        else
        {
            int sum=0;
            for(int i=0;i<D+1;i++)
                sum += coeffnum(i);
            if(sum<=ord)
            {
                if(tracker >= 0) tracker++;
                int indexmap = IndexMap2(coeffnum, ord);
                int k = coeffnum(D);
                if(k==0 || k==1)
                {
                    switch (basistype) {
                        case 0:
                            if(tracker>basis)
                            {
                                //trefftzbasis( i, setbasis++ ) = 1.0; //set the l-th coeff to 1
                                trefftzbasis(basis,indexmap) = 1;
                                tracker = -1;
                            }
                            //i += ndof-1;	//jump to time = 2 if i=0
                            break;
                        case 1:
                            if((k == 0 && basis < BinCoeff(D + ord, ord)) || (k == 1 && basis >= BinCoeff(D + ord, ord))){
                                trefftzbasis( basis,indexmap ) = 1;
                                for(int exponent :  coeffnum.Range(0,D)) trefftzbasis( basis,indexmap ) *= LegCoeffMonBasis(basis,exponent);}
                            break;
                        case 2:
                            if((k == 0 && basis < BinCoeff(D + ord, ord)) || (k == 1 && basis >= BinCoeff(D + ord, ord))){
                                trefftzbasis( basis,indexmap ) = 1;
                                for(int exponent :  coeffnum.Range(0,D)) trefftzbasis( basis,indexmap ) *= ChebCoeffMonBasis(basis,exponent);}
                            break;
                    }
                }
                else if(coeffnum(D)>1)
                {
                    for(int m=0;m<D;m++) //rekursive sum
                    {
                        Vec<D+1, int> get_coeff = coeffnum;
                        get_coeff[D] = get_coeff[D] - 2;
                        get_coeff[m] = get_coeff[m] + 2;
                        trefftzbasis( basis, indexmap) += (coeffnum(m)+1) * (coeffnum(m)+2) * trefftzbasis(basis, IndexMap2(get_coeff, ord));
                    }
                    trefftzbasis(basis, indexmap) *= wavespeed*wavespeed/(k * (k-1));
                }
            }
        }
    }

    template<int D>
    int TWaveBasis<D> :: IndexMap2(Vec<D+1, int> index, int ord)
    {
        int sum=0;
        int temp_size = 0;
        for(int d=0;d<D+1;d++){
            for(int p=0;p<index(d);p++){
                sum+=BinCoeff(D - d + ord - p - temp_size, ord - p - temp_size);
            }
            temp_size+=index(d);
        }
        return sum;
    }

    template class TWaveBasis<1>;
    template class TWaveBasis<2>;
    template class TWaveBasis<3>;

    constexpr int factorial(int n)
    {
        return n>1 ? n * factorial(n-1) : 1;
    }

    template<int D>
    CSR QTWaveBasis<D> :: Basis(int ord, Vec<D+1> ElCenter, Matrix<shared_ptr<CoefficientFunction>> GGder, Matrix<shared_ptr<CoefficientFunction>> BBder, double elsize, int basistype)
    {
        lock_guard<mutex> lock(gentrefftzbasis);
        string encode = to_string(ord) + to_string(elsize);
        for(int i=0;i<D;i++)
            encode += to_string(ElCenter[i]);

        if ( gtbstore[encode][0].Size() == 0)
        {
            IntegrationRule ir (D==3?ET_TET:D==2?ET_TRIG:ET_SEGM, 0);
            Mat<D,D> dummy;
            FE_ElementTransformation<D,D> et(D==3?ET_TET:D==2?ET_TRIG:ET_SEGM,dummy);
            MappedIntegrationPoint<D,D> mip(ir[0],et,0);
            for(int i=0;i<D;i++)
                mip.Point()[i] = ElCenter[i];

            Matrix<> BB(ord,(ord-1)*(D==2)+1);
            Matrix<> GG(ord-1,(ord-2)*(D==2)+1);
            for(int ny=0;ny<=(ord-1)*(D==2);ny++)
            {
                for(int nx=0;nx<=ord-1;nx++)
                {
                    double fac = (factorial(nx)*factorial(ny));
                    BB(nx,ny) = BBder(nx,ny)->Evaluate(mip)/fac * pow(elsize,nx+ny);
                    if(nx<ord-1 && ny<ord-1)
                    GG(nx,ny) = GGder(nx,ny)->Evaluate(mip)/fac * pow(elsize,nx+ny);
                }
            }

            const int nbasis = (BinCoeff(D + ord, ord) + BinCoeff(D + ord-1, ord-1));
            const int npoly = BinCoeff(D+1 + ord, ord);
            Matrix<> qbasis(nbasis,npoly);
            qbasis = 0;

            for(int t=0, basisn=0;t<2;t++)
                for(int x=0;x<=ord-t;x++)
                    for(int y=0;y<=(ord-x-t)*(D==2);y++)
                    {
                        Vec<D+1, int> index;
                        index[D] = t;
                        index[0] = x;
                        if(D==2) index[1]=y;
                        qbasis( basisn++, TWaveBasis<D>::IndexMap2(index, ord))=1.0;
                    }

            for(int basisn=0;basisn<nbasis;basisn++)
            {
                for(int ell=0;ell<ord-1;ell++)
                {
                    for(int t=0;t<=ell;t++)
                    {
                        for(int x=(D==1?ell-t:0);x<=ell-t;x++)
                        {
                            int y = ell-t-x;
                            Vec<D+1, int> index;
                            index[1] = y; index[0] = x; index[D] = t+2;
                            double* newcoeff =& qbasis( basisn, TWaveBasis<D>::IndexMap2(index, ord));
                            *newcoeff = 0;

                            for(int betax=0;betax<=x;betax++)
                                for(int betay=(D==2)?0:y;betay<=y;betay++)
                                {
                                    index[1] = betay; index[0] = betax+1; index[D] = t;
                                    int getcoeffx = TWaveBasis<D>::IndexMap2(index, ord);
                                    index[1] = betay+1; index[0] = betax; index[D] = t;
                                    int getcoeffy = TWaveBasis<D>::IndexMap2(index, ord);
                                    index[1] = betay; index[0] = betax+2; index[D] = t;
                                    int getcoeffxx = TWaveBasis<D>::IndexMap2(index, ord);
                                    index[1] = betay+2; index[0] = betax; index[D] = t;
                                    int getcoeffyy = TWaveBasis<D>::IndexMap2(index, ord);

                                    *newcoeff +=
                                        (betax+2)*(betax+1)/((t+2)*(t+1)*GG(0)) * BB(x-betax,y-betay)
                                        * qbasis( basisn, getcoeffxx)
                                        + (x-betax+1)*(betax+1)/((t+2)*(t+1)*GG(0)) * BB(x-betax+1,y-betay)
                                        * qbasis( basisn, getcoeffx);
                                    if(D==2)
                                    *newcoeff +=
                                        (betay+2)*(betay+1)/((t+2)*(t+1)*GG(0)) * BB(x-betax,y-betay)
                                        * qbasis( basisn, getcoeffyy)
                                        + (y-betay+1)*(betay+1)/((t+2)*(t+1)*GG(0)) * BB(x-betax,y-betay+1)
                                        * qbasis( basisn, getcoeffy);
                                    if(betax+betay == x+y) continue;
                                    index[1] = betay; index[0] = betax; index[D] = t+2;
                                    int getcoeff = TWaveBasis<D>::IndexMap2(index, ord);

                                    *newcoeff
                                        -= GG(x-betax,y-betay)*qbasis( basisn, getcoeff) / GG(0);
                                }
                        }
                    }
                }
            }

            MatToCSR(qbasis,gtbstore[encode]);
        }

        if ( gtbstore[encode].Size() == 0)
        {
            stringstream str;
            str << "failed to generate trefftz basis of order " << ord << endl;
            throw Exception (str.str());
        }

        return gtbstore[encode];
    }

    template class QTWaveBasis<1>;
    template class QTWaveBasis<2>;


}


#ifdef NGS_PYTHON
void ExportTrefftzFESpace(py::module m)
{
    using namespace ngcomp;
    //using namespace ngfem;
    //[>
    //We just export the class here and use the FESpace constructor to create our space.
    //This has the advantage, that we do not need to specify all the flags to parse (like
    //dirichlet, definedon,...), but we can still append new functions only for that space.
    //*/
    //py::class_<TrefftzFESpace, shared_ptr<TrefftzFESpace>, FESpace>
    //(m, "TrefftzFESpace", "FESpace with first order and second order trigs on 2d mesh")
    //.def("GetNDof", &TrefftzFESpace::GetNDof)
    //;
    //m.def("GetNDof", [](shared_ptr<FESpace> fes) {
    //cout << typeid(*fes).name() << endl;
    ////fes->GetNDof();
    //});

    ExportFESpace<TrefftzFESpace>(m, "trefftzfespace")
        .def("GetDocu", &TrefftzFESpace::GetDocu)
        .def("GetNDof", &TrefftzFESpace::GetNDof)
        .def("SetWavespeed", &TrefftzFESpace::SetWavespeed, py::arg("Wavespeed"), py::arg("BBcf")=nullptr, py::arg("GGcf")=nullptr)
        ;
}
#endif // NGS_PYTHON