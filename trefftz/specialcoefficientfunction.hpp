#ifndef SPECIALCOEFFICIENTFUNCTION_HPP
#define SPECIALCOEFFICIENTFUNCTION_HPP

#include <fem.hpp>
#include <comp.hpp>
#include <multigrid.hpp>
#include <h1lofe.hpp>
#include <regex>
#include "trefftzwavefe.hpp"

using namespace ngcomp;
namespace ngfem
{

    class ClipCoefficientFunction : public CoefficientFunction
    {
        private:
            shared_ptr<CoefficientFunction> coef;
            double clipvalue;
            int clipdim;
        public:
            ///
            ClipCoefficientFunction(shared_ptr<CoefficientFunction> acoef,int adimension,int aclipdim,double aclipvalue, bool ais_complex = false)
                : CoefficientFunction(adimension,ais_complex), coef(acoef), clipdim(aclipdim), clipvalue(aclipvalue)
            { ; }
            ///
            virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const;
            ///
            virtual void Evaluate (const BaseMappedIntegrationRule & ir, FlatMatrix<double> values) const;
            virtual void EvaluateStdRule (const BaseMappedIntegrationRule & ir, FlatMatrix<double> values) const;
    };


    class IntegrationPointFunction : public CoefficientFunction
    {
        public:

            IntegrationPointFunction(shared_ptr<MeshAccess> mesh, IntegrationRule& intrule, Vector<> ipdata)
                : CoefficientFunction(1)
            {
                values.resize(mesh->GetNE());
                int elnr=0;
                for (auto& vec : values)
                {
                    vec.resize(intrule.GetNIP());
                    for (int i = 0;i < vec.size();i++)
                    {
                        // input data from vector with mip values sorted per element
                        vec[i] = ipdata[intrule.Size()*elnr+i];
                    }
                    elnr++;
                }
            }

            IntegrationPointFunction(shared_ptr<MeshAccess> mesh, IntegrationRule& intrule, Matrix<> ipdata)
                : CoefficientFunction(1)
            {
                values.resize(mesh->GetNE());
                int elnr=0;
                for (auto& vec : values)
                {
                    vec.resize(intrule.GetNIP());
                    for (int i = 0;i < vec.size();i++)
                    {
                        // input data from matrix with elnr in rows, mip values in cols
                        vec[i] = ipdata(elnr,i);
                    }
                    elnr++;
                }
            }

            virtual double Evaluate(const BaseMappedIntegrationPoint & ip) const
            {
                int p = ip.GetIPNr();
                int el = ip.GetTransformation().GetElementNr();

                if (p < 0 || p >= values[el].size())
                {
                    cout << "got illegal integration point number " << p << endl;
                    return 0;
                }

                return values[el][p];
            }

            void PrintTable()
            {
                for (int i = 0;i < values.size();i++)
                {
                    for (int j = 0;j < values[i].size();j++)
                    {
                        cout << values[i][j] << ", ";
                    }
                    cout << endl;
                }
                cout << endl;
            }

        private:
            vector<vector<double>> values;
    };


    class TrefftzCoefficientFunction : public CoefficientFunction
    {
        int basisfunction;
        TrefftzWaveFE<3> treff = TrefftzWaveFE<3>(4,1,ET_TRIG,0);

        public:
        TrefftzCoefficientFunction()
            : CoefficientFunction(1) { ; }

        TrefftzCoefficientFunction(int basis)
            : CoefficientFunction(1) { basisfunction = basis; }

        virtual double Evaluate(const BaseMappedIntegrationPoint& mip) const override
        {
            FlatVector<double> point = mip.GetPoint();

            int ndof = treff.GetNBasis();
            cout  << "nr: " << basisfunction << " / " << ndof << endl;
            Vector<double> shape(ndof);
            //Matrix<double> shape(ndof,2);
            treff.CalcShape(mip,shape);
            return shape[basisfunction];
        }
    };
}

#ifdef NGS_PYTHON
#include <python_ngstd.hpp>
void ExportSpecialCoefficientFunction(py::module m);
#endif // NGS_PYTHON

#endif