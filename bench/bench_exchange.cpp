#include <mpi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static size_t read_rss_kb() {
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f)
        return 0;
    char line[512];
    size_t kb = 0;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "VmRSS:", 6) == 0) {
            std::sscanf(line + 6, "%zu", &kb);
            break;
        }
    }
    std::fclose(f);
    return kb;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int world_rank = 0, world_size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int nx = 1024, ny = 1024, steps = 100;
    int px = 0, py = 0, halo = 1;
    for (int i = 1; i < argc; i++) {
        auto eq = std::strstr(argv[i], "=");
        auto arg = std::string(argv[i]);
        auto take = [&](const char* k) { return arg.rfind(k, 0) == 0; };
        if (take("--nx="))
            nx = std::atoi(eq + 1);
        else if (take("--ny="))
            ny = std::atoi(eq + 1);
        else if (take("--px="))
            px = std::atoi(eq + 1);
        else if (take("--py="))
            py = std::atoi(eq + 1);
        else if (take("--steps="))
            steps = std::atoi(eq + 1);
        else if (take("--halo="))
            halo = std::atoi(eq + 1);
    }

    int dims[2] = {px, py};
    MPI_Dims_create(world_size, 2, dims);
    int periods[2] = {0, 0};
    MPI_Comm cart;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cart);
    int coords[2];
    int cart_rank = 0;
    MPI_Comm_rank(cart, &cart_rank);
    MPI_Cart_coords(cart, cart_rank, 2, coords);

    int base_nx = nx / dims[0], remx = nx % dims[0];
    int base_ny = ny / dims[1], remy = ny % dims[1];
    int nx_local = base_nx + (coords[0] == dims[0] - 1 ? remx : 0);
    int ny_local = base_ny + (coords[1] == dims[1] - 1 ? remy : 0);

    const int nx_tot = nx_local + 2 * halo;
    const int ny_tot = ny_local + 2 * halo;
    std::vector<double> a((size_t)nx_tot * ny_tot, 1.0);

    auto at = [&](int i, int j) -> double& { return a[(size_t)j * nx_tot + i]; };

    int nbrL, nbrR, nbrD, nbrU;
    MPI_Cart_shift(cart, 0, 1, &nbrL, &nbrR);
    MPI_Cart_shift(cart, 1, 1, &nbrD, &nbrU);

    MPI_Datatype col_t;
    MPI_Type_vector(ny_local, halo, nx_tot, MPI_DOUBLE, &col_t);
    MPI_Type_commit(&col_t);
    int row_cnt = nx_local * halo;

    MPI_Barrier(cart);

    double t0 = MPI_Wtime();
    double sum_step = 0.0, max_step = 0.0, min_step = 1e300;

    for (int s = 0; s < steps; ++s) {
        double ts = MPI_Wtime();

        MPI_Request reqs[8];
        int rq = 0;
        MPI_Irecv(&at(0, halo), 1, col_t, nbrL, 10, cart, &reqs[rq++]);
        MPI_Irecv(&at(nx_local + halo, halo), 1, col_t, nbrR, 11, cart, &reqs[rq++]);
        MPI_Irecv(&at(halo, 0), row_cnt, MPI_DOUBLE, nbrD, 12, cart, &reqs[rq++]);
        MPI_Irecv(&at(halo, ny_local + halo), row_cnt, MPI_DOUBLE, nbrU, 13, cart, &reqs[rq++]);

        MPI_Isend(&at(halo, halo), 1, col_t, nbrL, 11, cart, &reqs[rq++]);
        MPI_Isend(&at(nx_local, halo), 1, col_t, nbrR, 10, cart, &reqs[rq++]);
        MPI_Isend(&at(halo, halo), row_cnt, MPI_DOUBLE, nbrD, 13, cart, &reqs[rq++]);
        MPI_Isend(&at(halo, ny_local), row_cnt, MPI_DOUBLE, nbrU, 12, cart, &reqs[rq++]);

        MPI_Waitall(rq, reqs, MPI_STATUSES_IGNORE);

        volatile double acc = 0.0;
        for (int j = halo; j < ny_local + halo; ++j) {
            for (int i = halo; i < nx_local + halo; ++i) {
                acc += at(i - 1, j) + at(i + 1, j) + at(i, j - 1) + at(i, j + 1) - 4.0 * at(i, j);
            }
        }

        double te = MPI_Wtime();
        double dt = te - ts;
        sum_step += dt;
        max_step = std::max(max_step, dt);
        min_step = std::min(min_step, dt);
    }

    double t1 = MPI_Wtime();
    double total = t1 - t0;
    double avg_step = sum_step / steps;

    double total_max = 0, total_min = 0, total_avg = 0;
    MPI_Reduce(&total, &total_max, 1, MPI_DOUBLE, MPI_MAX, 0, cart);
    MPI_Reduce(&total, &total_min, 1, MPI_DOUBLE, MPI_MIN, 0, cart);
    MPI_Reduce(&avg_step, &total_avg, 1, MPI_DOUBLE, MPI_MAX, 0, cart);

    size_t rss_kb = read_rss_kb();
    size_t rss_max = 0;
    MPI_Reduce(&rss_kb, &rss_max, 1, MPI_UNSIGNED_LONG_LONG, MPI_MAX, 0, cart);

    if (world_rank == 0) {
        std::printf(
            "ranks,Px,Py,nx,ny,nx_local,ny_local,steps,halo,total_max,total_min,perstep_worst,rss_"
            "kb_max\n");
        std::printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%zu\n",
                    world_size,
                    dims[0],
                    dims[1],
                    nx,
                    ny,
                    nx_local,
                    ny_local,
                    steps,
                    halo,
                    total_max,
                    total_min,
                    total_avg,
                    rss_max);
        std::fflush(stdout);
    }

    MPI_Type_free(&col_t);
    MPI_Comm_free(&cart);
    MPI_Finalize();
    return 0;
}
