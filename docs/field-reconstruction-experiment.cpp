#include "tpc/analytics/analytics_manager/analytics_manager.hpp"
#include "tpc/analytics/models/basis_models.hpp"
#include <iostream>
#include <iomanip>
#include <numbers>

namespace a = tpc::analytics;
const std::array<double,10> c{5000,.3,.02,.002,.4,-.2,.01,.02,.04,-.03};
std::array<double,3> exact(double r,double phi,double z) {
    std::array<double,3> b{};
    for (int k=0;k<10;++k) {
        b[0]+=c[k]*a::models::DefaultBasisR(k,r,phi,z);
        b[1]+=c[k]*a::models::DefaultBasisPhi(k,r,phi,z);
        b[2]+=c[k]*a::models::DefaultBasisZ(k,r,phi,z);
    }
    return b;
}
int main() {
    std::cout<<std::setprecision(12);
    std::cout<<"noise_amplitude_G,rank,condition,max_residual_G,rms_residual_G,max_control_error_G,rms_control_error_G,control_points\n";
    for(double noise:{0.,.05,.1,.2}) {
        auto basis=a::AnalyticsManager::create_default_basis(a::models::DefaultBasisR,a::models::DefaultBasisPhi,a::models::DefaultBasisZ,10);
        auto solver=a::AnalyticsManager::create(std::move(basis));
        std::vector<a::models::Measurement> data;
        int row=0;
        for(int side:{-1,1}) for(int n=0;n<6;++n) {
            double phi=n*std::numbers::pi/3, z=side*3.5;
            auto b=exact(4,phi,z);
            for(auto& v:b) v+=noise*std::sin(1.7*(++row));
            data.push_back({{{4,phi,z},a::models::CoordinateType::Cylindric}, {b,a::models::CoordinateType::Cylindric}});
        }
        a::models::ReconstructionLimits limits;
        limits.warnings_only=true;
        auto fitted=solver->calculate_svd_coefficients(data,limits.svd_threshold,limits);
        if(!fitted) {std::cerr<<fitted.error()<<'\n'; return 1;}
        auto q=solver->field_quality();
        double maxerr=0,sq=0;std::size_t count=0;
        for(int iz=0;iz<17;++iz) for(int iy=0;iy<17;++iy) for(int ix=0;ix<17;++ix) {
            double x=-4+ix*.5,y=-4+iy*.5,z=-3.5+iz*7./16;
            if(x*x+y*y>16) continue;
            double r=std::hypot(x,y),phi=std::atan2(y,x);
            auto truth=exact(r,phi,z);
            auto b=solver->evaluate_for_point({r,phi,z});
            if(!b) return 2;
            for(int j=0;j<3;++j) {double e=b->components[j]-truth[j];maxerr=std::max(maxerr,std::abs(e));sq+=e*e;}
            ++count;
        }
        std::cout<<noise<<','<<q.rank<<','<<q.condition_number<<','<<q.maximum_residual_gauss<<','<<q.rms_residual_gauss<<','<<maxerr<<','<<std::sqrt(sq/(3*count))<<','<<count<<'\n';
    }
}
