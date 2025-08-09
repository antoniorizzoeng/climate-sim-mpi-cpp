#include <gtest/gtest.h>
#include "field.hpp"
#include "diffusion.hpp"

static void zero_ghosts(Field& f) {
    for (int i=0;i<f.nx_total();++i) { f.at(i,0)=0.0; f.at(i,f.ny_total()-1)=0.0; }
    for (int j=0;j<f.ny_total();++j) { f.at(0,j)=0.0; f.at(f.nx_total()-1,j)=0.0; }
}

TEST(Diffusion, SingleImpulseOneStep) {
    Field u(3,3,1,1.0,1.0);
    Field v(3,3,1,1.0,1.0);
    u.at(2,2) = 1.0;
    zero_ghosts(u);

    const double D=0.1, dt=0.1;
    const double alpha = D*dt/(u.dx*u.dx);
    ASSERT_LE(alpha, 0.25);

    diffusion_step(u, v, D, dt);

    EXPECT_NEAR(v.at(2,2), 1.0 - 4*alpha, 1e-12);
    EXPECT_NEAR(v.at(1,2), alpha, 1e-12);
    EXPECT_NEAR(v.at(3,2), alpha, 1e-12);
    EXPECT_NEAR(v.at(2,1), alpha, 1e-12);
    EXPECT_NEAR(v.at(2,3), alpha, 1e-12);
}
