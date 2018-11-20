#include "scalarmappedfe.hpp"
#include "h1lofe.hpp"
#include "l2hofe.hpp"


namespace ngfem
{

    void BaseScalarMappedElement ::
    CalcShape (const BaseMappedIntegrationPoint & mip,
               BareSliceVector<Complex> shape) const
    {
        CalcShape (mip, SliceVector<double> (ndof, 2*shape.Dist(), reinterpret_cast<double*> (&shape(0))));
        SliceVector<double> imag_part(ndof, 2*shape.Dist(), reinterpret_cast<double*> (&shape(0))+1);
        imag_part = 0.0;
    }

    void BaseScalarMappedElement ::
    CalcShape (const BaseMappedIntegrationRule & mir,
               SliceMatrix<> shape) const
    {
        for (int i = 0; i < mir.Size(); i++)
            CalcShape (mir[i], shape.Col(i));
    }

    void BaseScalarMappedElement ::
    CalcShape (const SIMD_BaseMappedIntegrationRule & mir,
               BareSliceMatrix<SIMD<double>> shape) const
    {
        throw ExceptionNOSIMD("SIMD - CalcShape not overloaded");
    }

    double BaseScalarMappedElement ::
    Evaluate (const BaseMappedIntegrationPoint & mip, BareSliceVector<double> x) const
    {
        VectorMem<20, double> shape(ndof);
        CalcShape (mip, shape);
        return InnerProduct (shape, x);
    }

    void BaseScalarMappedElement ::
    Evaluate (const BaseMappedIntegrationRule & mir, BareSliceVector<double> coefs, FlatVector<double> vals) const
    {
        for (size_t i = 0; i < mir.Size();i++) //.GetNIP(); i++)
            vals(i) = Evaluate (mir[i], coefs);
    }

    void BaseScalarMappedElement ::
    Evaluate (const SIMD_BaseMappedIntegrationRule & mir, BareSliceVector<> coefs, BareVector<SIMD<double>> values) const
    {
        throw ExceptionNOSIMD (string("Evaluate (simd) not implemented for class ")+typeid(*this).name());
    }

    void BaseScalarMappedElement ::
    Evaluate (const SIMD_BaseMappedIntegrationRule & mir, SliceMatrix<> coefs, BareSliceMatrix<SIMD<double>> values) const
    {
        for (size_t i = 0; i < coefs.Width(); i++)
            Evaluate (mir, coefs.Col(i), values.Row(i));
    }

    void BaseScalarMappedElement ::
    Evaluate (const BaseMappedIntegrationRule & mir, SliceMatrix<> coefs, SliceMatrix<> values) const
    {
        VectorMem<100> shapes(coefs.Height());
        for (size_t i = 0; i < mir.Size(); i++)
        {
            CalcShape (mir[i], shapes);
            values.Row(i) = Trans(coefs) * shapes;
        }
    }

    void BaseScalarMappedElement ::
    EvaluateTrans (const BaseMappedIntegrationRule & mir, FlatVector<double> vals, BareSliceVector<double> coefs) const
    {
        VectorMem<20, double> shape(ndof);
        coefs.AddSize(ndof) = 0.0;
        for (int i = 0; i < mir.Size(); i++) //GetNIP()
        {
            CalcShape (mir[i], shape);
            coefs.AddSize(ndof) += vals(i) * shape;
        }
    }

    void BaseScalarMappedElement ::
    AddTrans (const SIMD_BaseMappedIntegrationRule & mir, BareVector<SIMD<double>> values, BareSliceVector<> coefs) const
    {
        throw ExceptionNOSIMD (string("AddTrans (simd) not implemented for class ")+typeid(*this).name());
    }

    void BaseScalarMappedElement ::
    AddTrans (const SIMD_BaseMappedIntegrationRule & mir, BareSliceMatrix<SIMD<double>> values, SliceMatrix<> coefs) const
    {
        for (int i = 0; i < coefs.Width(); i++)
            AddTrans (mir, values.Row(i), coefs.Col(i));
    }

    void BaseScalarMappedElement ::
    CalcMappedDShape (const SIMD_BaseMappedIntegrationRule & mir,
                      BareSliceMatrix<SIMD<double>> dshapes) const
    {
        throw ExceptionNOSIMD("SIMD - CalcDShape not overloaded");
    }

    void BaseScalarMappedElement ::
    EvaluateGrad (const SIMD_BaseMappedIntegrationRule & ir, BareSliceVector<> coefs, BareSliceMatrix<SIMD<double>> values) const
    {
        throw ExceptionNOSIMD (string("EvaluateGrad (simd) not implemented for class ")+typeid(*this).name());
    }

    void BaseScalarMappedElement ::
    EvaluateGrad (const SIMD_IntegrationRule & ir, BareSliceVector<> coefs, BareSliceMatrix<SIMD<double>> values) const
    {
        throw ExceptionNOSIMD (string("EvaluateGrad (simd) not implemented for class ")+typeid(*this).name());
    }

    void BaseScalarMappedElement ::
    AddGradTrans (const SIMD_BaseMappedIntegrationRule & ir, BareSliceMatrix<SIMD<double>> values,
                  BareSliceVector<> coefs) const
    {
        throw ExceptionNOSIMD (string("AddGradTrans (simd) not implemented for class ")+typeid(*this).name());
    }



    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    template<int D>
    string ScalarMappedElement<D> :: ClassName() const
    {
        return "ScalarMappedElement";
    }


    template<int D>
    void ScalarMappedElement<D> :: CalcDShape (const BaseMappedIntegrationRule & mir,
                                            SliceMatrix<> dshapes) const
    {
        for (int i = 0; i < mir.Size(); i++)
            CalcDShape (mir[i], dshapes.Cols(i*D,(i+1)*D));
    }


    template<int D>
    void ScalarMappedElement<D> ::
    CalcMappedDShape (const MappedIntegrationPoint<D,D> & mip,
                      SliceMatrix<> dshape) const
    {
        //no mapping - no inner derivative
        CalcDShape (mip, dshape);
    }

    template<int D>
    void ScalarMappedElement<D> ::
    CalcMappedDShape (const MappedIntegrationRule<D,D> & mir,
                      SliceMatrix<> dshapes) const
    {
        for (int i = 0; i < mir.Size(); i++)
            CalcMappedDShape (mir[i], dshapes.Cols(i*D,(i+1)*D));
    }


    template<int D>
    Vec<D> ScalarMappedElement<D> ::
    EvaluateGrad (const BaseMappedIntegrationPoint & ip, BareSliceVector<double> x) const
    {
        MatrixFixWidth<D> dshape(ndof);
        CalcDShape (ip, dshape);
        Vec<D> grad = Trans (dshape) * x;
        return grad;
    }


    template<int D>
    void ScalarMappedElement<D> ::
    EvaluateGrad (const BaseMappedIntegrationRule & ir, BareSliceVector<double> coefs, FlatMatrixFixWidth<D,double> vals) const
    {
        for (size_t i = 0; i < ir.Size(); i++)
            vals.Row(i) = EvaluateGrad (ir[i], coefs);
    }



    template<int D>
    void ScalarMappedElement<D> ::
    EvaluateGradTrans (const BaseMappedIntegrationRule & ir, FlatMatrixFixWidth<D,double> vals,
                       BareSliceVector<double> coefs) const
    {
        MatrixFixWidth<D> dshape(ndof);
        coefs.AddSize(ndof) = 0.0;
        for (int i = 0; i < ir.Size(); i++)
        {
            CalcDShape (ir[i], dshape);
            coefs.AddSize(ndof) += dshape * vals.Row(i);
        }
    }


    template<int D>
    void ScalarMappedElement<D> ::
    EvaluateGradTrans (const BaseMappedIntegrationRule & ir, SliceMatrix<> values, SliceMatrix<> coefs) const
    {
#ifndef __CUDA_ARCH__
        cout << "EvalGradTrans not overloaded" << endl;
#endif
    }

    template <int D>
        void ScalarMappedElement<D> ::
        GetPolOrders (FlatArray<PolOrder<D> > orders) const
        {
#ifndef __CUDA_ARCH__
            throw Exception (string ("GetPolOrders not implemnted for element") + ClassName());
#endif
        }

    template class ScalarMappedElement<1>;
    template class ScalarMappedElement<2>;
    template class ScalarMappedElement<3>;
    template class ScalarMappedElement<4>;

}