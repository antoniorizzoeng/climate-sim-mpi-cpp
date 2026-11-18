#include <gtest/gtest.h>
#include <mpi.h>

#include <cmath>

#include "advection.hpp"
#include "diffusion.hpp"
#include "init.hpp"
#include "step.hpp"

namespace {
void compare_steps(SimConfig cfg, const Decomp2D& dec, const Field& initial) {
    SCOPED_TRACE(bc_to_string(cfg.bc.left) + "/" + bc_to_string(cfg.bc.right) + "/" +
                 bc_to_string(cfg.bc.bottom) + "/" + bc_to_string(cfg.bc.top));
    Field ref = initial, next = initial;
    Field local(dec.nx_local, dec.ny_local, 1, cfg.dx, cfg.dy);
    for (int j = 0; j < local.ny_total(); ++j)
        for (int i = 0; i < local.nx_total(); ++i)
            local.at(i, j) = initial.at(dec.x_offset + i, dec.y_offset + j);
    Field scratch = local;

    struct Physics {
        double vx, vy, D;
    };
    const Physics cases[] = {{0.4, -0.3, 0.06},
                             {-0.7, -0.3, 0.06},
                             {0.4, 0.8, 0.06},
                             {-0.7, 0.8, 0.06},
                             {0.0, 0.0, 0.06},
                             {-0.7, 0.0, 0.0}};
    for (const auto& physics : cases) {
        cfg.vx = physics.vx;
        cfg.vy = physics.vy;
        cfg.D = physics.D;
        apply_boundary(ref, Decomp2D{}, cfg.bc, 0.0);
        diffusion_step(ref, next, cfg.D, cfg.dt);
        advection_step(ref, next, cfg.vx, cfg.vy, cfg.dt);
        ref.data.swap(next.data);

        advance_field(local, scratch, dec, cfg);
        for (int j = 1; j <= dec.ny_local; ++j)
            for (int i = 1; i <= dec.nx_local; ++i)
                EXPECT_NEAR(local.at(i, j), ref.at(dec.x_offset + i, dec.y_offset + j), 1e-13)
                    << "v=(" << cfg.vx << "," << cfg.vy << ") at " << dec.x_offset + i << ","
                    << dec.y_offset + j;
    }
}
}  // namespace

TEST(Unit_Step, MatchesGlobalReferenceForEveryBoundaryCombination) {
    SimConfig cfg;
    cfg.nx = 13;
    cfg.ny = 11;
    cfg.dx = 0.8;
    cfg.dy = 1.7;
    Decomp2D dec;
    dec.init(MPI_COMM_WORLD, cfg.nx, cfg.ny);
    Field initial(cfg.nx, cfg.ny, 1, cfg.dx, cfg.dy);
    for (int j = 0; j < initial.ny_total(); ++j)
        for (int i = 0; i < initial.nx_total(); ++i)
            initial.at(i, j) = std::sin(0.7 * i + j) + (i * j % 7);

    const BCType faces[] = {BCType::Dirichlet, BCType::Neumann, BCType::Periodic};
    for (auto left : faces)
        for (auto right : faces)
            for (auto bottom : faces)
                for (auto top : faces) {
                    cfg.bc = {left, right, bottom, top};
                    compare_steps(cfg, dec, initial);
                }
    dec.finalize();
}

TEST(Unit_Step, EmptyPresetTileRelaysThePreviousGhost) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm pair;
    MPI_Comm_split(MPI_COMM_WORLD, rank / 2, rank, &pair);
    Decomp2D dec;
    dec.init(pair, 1, 5);
    SimConfig cfg;
    cfg.D = 0.1;
    cfg.dt = 1.0;
    cfg.bc = {BCType::Neumann, BCType::Neumann, BCType::Neumann, BCType::Neumann};
    Field u(dec.nx_local, dec.ny_local, 1, 1.0, 1.0);
    Field scratch = u;
    if (dec.nx_local == 1)
        for (int j = 1; j <= dec.ny_local; ++j) u.at(1, j) = 1.0;

    advance_field(u, scratch, dec, cfg);
    advance_field(u, scratch, dec, cfg);
    // The two-rank grid sees left ghosts 0 then 1, giving 0.9 then 0.91.
    // An unpaired rank has two physical Neumann faces and stays constant.
    if (dec.nx_local == 1)
        for (int j = 1; j <= dec.ny_local; ++j)
            EXPECT_NEAR(u.at(1, j), dec.dims[0] == 2 ? 0.91 : 1.0, 1e-14);
    dec.finalize();
    MPI_Comm_free(&pair);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    initialize_mpi(argc, argv);
    const int result = RUN_ALL_TESTS();
    MPI_Finalize();
    return result;
}
