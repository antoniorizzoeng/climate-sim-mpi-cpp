#include "kokkos_step.hpp"

#include <Kokkos_Core.hpp>
#include <algorithm>
#include <limits>
#include <utility>

#include "stencil_math.hpp"

namespace {
using Buffer = Kokkos::View<double*>;
using HostBuffer = Kokkos::View<double*, Kokkos::HostSpace>;
using HostInput = Kokkos::View<const double*, Kokkos::HostSpace, Kokkos::MemoryUnmanaged>;
using HostOutput = Kokkos::View<double*, Kokkos::HostSpace, Kokkos::MemoryUnmanaged>;
using Range = Kokkos::RangePolicy<Kokkos::IndexType<size_t>>;
using namespace stencil_math;

Kokkos::InitializationSettings settings(MPI_Comm comm) {
    Kokkos::InitializationSettings options;
    int provided;
    MPI_Query_thread(&provided);
    if (provided < MPI_THREAD_FUNNELED)
        options.set_num_threads(1);
#if defined(KOKKOS_ENABLE_CUDA) || defined(KOKKOS_ENABLE_HIP)
    int rank, local_rank;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm node;
    MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, rank, MPI_INFO_NULL, &node);
    MPI_Comm_rank(node, &local_rank);
    MPI_Comm_free(&node);
    // Kokkos reports the device count visible to this process, including masks
    // applied by a scheduler. A single visible device always maps to zero.
    options.set_device_id(local_rank % Kokkos::num_devices());
#else
    (void)comm;
#endif
    return options;
}

// The stencil only reads face interiors. Transverse edges and corners can be
// omitted, including for periodic exchange. All offsets index the padded field.
struct FaceLayout {
    size_t width, height;
    size_t fast_stride, slow_stride;
    std::ptrdiff_t inside, ghost, wrapped;
    size_t size() const { return width * height; }
    KOKKOS_INLINE_FUNCTION size_t offset(size_t q) const {
        return q / width * slow_stride + q % width * fast_stride;
    }
};

struct Face {
    FaceLayout layout;
    int peer, tag;
    BCType boundary;
    Buffer send, recv;
#ifndef KOKKOS_GPU_AWARE_MPI
    HostBuffer host_send, host_recv;
#endif

    Face(FaceLayout layout, int peer, int tag, BCType boundary)
        : layout(layout), peer(peer), tag(tag), boundary(boundary) {
        if (peer != MPI_PROC_NULL) {
            send = Buffer("send halo", layout.size());
            recv = Buffer("receive halo", layout.size());
#ifndef KOKKOS_GPU_AWARE_MPI
            host_send = Kokkos::create_mirror(Kokkos::HostSpace{}, send);
            host_recv = Kokkos::create_mirror(Kokkos::HostSpace{}, recv);
#endif
        }
    }

    void pack(Buffer field) const {
        const auto f = layout;
        const auto packed = send;
        Kokkos::parallel_for(
            "pack halo", Range(0, f.size()), KOKKOS_LAMBDA(size_t q) {
                packed(q) = field(f.inside + f.offset(q));
            });
    }

    void post(MPI_Comm comm, std::vector<MPI_Request>& pending) const {
#ifdef KOKKOS_GPU_AWARE_MPI
        double* out = send.data();
        double* in = recv.data();
#else
        Kokkos::deep_copy(host_send, send);
        double* out = host_send.data();
        double* in = host_recv.data();
#endif
        // MPI-3 counts are int. Split very large volume faces into ordered messages.
        for (size_t offset = 0; offset < layout.size();) {
            int n = static_cast<int>(
                std::min<size_t>(layout.size() - offset, std::numeric_limits<int>::max()));
            MPI_Irecv(in + offset, n, MPI_DOUBLE, peer, tag ^ 1, comm, &pending.emplace_back());
            MPI_Isend(out + offset, n, MPI_DOUBLE, peer, tag, comm, &pending.emplace_back());
            offset += n;
        }
    }

    void unpack(Buffer field) const {
#ifndef KOKKOS_GPU_AWARE_MPI
        Kokkos::deep_copy(recv, host_recv);
#endif
        const auto f = layout;
        const auto packed = recv;
        Kokkos::parallel_for(
            "unpack halo", Range(0, f.size()), KOKKOS_LAMBDA(size_t q) {
                field(f.ghost + f.offset(q)) = packed(q);
            });
    }

    void fill(Buffer field) const {
        const auto f = layout;
        const auto bc = boundary;
        Kokkos::parallel_for(
            "local boundary", Range(0, f.size()), KOKKOS_LAMBDA(size_t q) {
                const auto offset = f.offset(q);
                field(f.ghost + offset) =
                    bc == BCType::Dirichlet
                        ? 0.0
                        : field((bc == BCType::Periodic ? f.wrapped : f.inside) + offset);
            });
    }
};

struct DeviceWind {
    Buffer vx, vy, vz;

    void upload(const WindTile& tile) {
        auto component = [](Buffer& dst, const std::vector<double>& src) {
            if (dst.extent(0) != src.size())
                dst = Buffer(Kokkos::view_alloc(Kokkos::WithoutInitializing, "wind"), src.size());
            if (!src.empty())
                Kokkos::deep_copy(dst, HostInput(src.data(), src.size()));
        };
        component(vx, tile.vx);
        component(vy, tile.vy);
        component(vz, tile.vz);
    }
};

struct WindRecord {
    MPI_Offset index = -1;
    DeviceWind tile;
    void load(MPI_Offset requested, const WindTile& host) {
        if (index != requested || requested < 0) {
            tile.upload(host);
            index = requested;
        }
    }
};

struct Axis {
    double spacing, diff;
    Weights uniform;

    template <bool robust>
    static Axis make(double dt, double spacing, double diffusivity, double velocity) {
        double diff = diffusion<robust>(dt, diffusivity, spacing);
        return {spacing, diff, upwind(diff, dt * velocity / spacing)};
    }
};

template <bool volume, bool file>
struct InteriorStencil {
    const double* source;
    double* dst;
    int nx, ny, pitch;
    std::ptrdiff_t origin, plane;
    Axis x, y, z;
    DeviceWind lo, hi;
    double fraction, dt;

    KOKKOS_INLINE_FUNCTION void operator()(int k, int j, int i) const {
        Weights wx = x.uniform, wy = y.uniform, wz = z.uniform;
        if constexpr (file) {
            const size_t q = (static_cast<size_t>(k) * ny + j) * nx + i;
            wx = upwind(x.diff, dt * interpolate(lo.vx(q), hi.vx(q), fraction) / x.spacing);
            wy = upwind(y.diff, dt * interpolate(lo.vy(q), hi.vy(q), fraction) / y.spacing);
            if constexpr (volume) {
                const double vz = lo.vz.extent(0) ? interpolate(lo.vz(q), hi.vz(q), fraction) : 0.0;
                wz = upwind(z.diff, dt * vz / z.spacing);
            }
        }
        const auto pos = origin + k * plane + static_cast<std::ptrdiff_t>(j) * pitch + i;
        const double* cell = source + pos;
        double delta = difference(cell, 1, wx) + difference(cell, pitch, wy);
        if constexpr (volume)
            delta += difference(cell, plane, wz);
        dst[pos] = *cell + delta;
    }
};
}  // namespace

struct KokkosStep::State {
    // Views must be released before Kokkos finalizes.
    Kokkos::ScopeGuard runtime;
    Buffer u, scratch;
    const Decomposition& dec;
    const SimConfig& cfg;
    const int nx, ny, nz, pitch, hz;
    const std::ptrdiff_t plane, origin;
    std::vector<Face> neighbors, boundaries;
    std::vector<MPI_Request> requests;
    WindRecord lower, upper;

    State(const Field& initial, const Decomposition& dec, const SimConfig& cfg)
        : runtime(settings(dec.io_comm)),
          u("field", initial.data.size()),
          scratch("scratch", initial.data.size()),
          dec(dec),
          cfg(cfg),
          nx(initial.nx_local),
          ny(initial.ny_local),
          nz(initial.nz_local),
          pitch(initial.nx_total()),
          hz(initial.z_halo),
          plane(initial.plane_size()),
          origin(hz * plane + pitch + 1) {
        Kokkos::deep_copy(u, HostInput(initial.data.data(), initial.data.size()));
        if (dec.cart_comm == MPI_COMM_NULL)
            return;
        const int local[] = {nx, ny, nz}, global[] = {dec.nx_global, dec.ny_global, dec.nz_global};
        const std::ptrdiff_t strides[] = {1, pitch, plane};
        const int* peers[] = {dec.nbr_lr, dec.nbr_du, dec.nbr_fb};
        const BCType types[] = {
            cfg.bc.left, cfg.bc.right, cfg.bc.bottom, cfg.bc.top, cfg.bc.front, cfg.bc.back};
        for (int axis = 0; axis < (hz ? 3 : 2); ++axis) {
            const auto stride = strides[axis];
            const auto last = origin + (local[axis] - 1) * stride;
            const bool wrap =
                dec.periods[axis] && (dec.dims[axis] == 1 || global[axis] < dec.dims[axis]);
            for (int side = 0; side < 2; ++side) {
                const int tag = 2 * axis + side;
                const auto inside = side ? last : origin;
                FaceLayout f{static_cast<size_t>(axis == 0 ? ny : nx),
                             static_cast<size_t>(axis == 2 ? ny : nz),
                             static_cast<size_t>(axis == 0 ? pitch : 1),
                             static_cast<size_t>(axis == 2 ? pitch : plane),
                             inside,
                             inside + (side ? stride : -stride),
                             side ? origin : last};
                if (f.size()) {
                    const int peer = wrap ? MPI_PROC_NULL : peers[axis][side];
                    auto& faces = peer == MPI_PROC_NULL ? boundaries : neighbors;
                    faces.emplace_back(f, peer, tag, types[tag]);
                }
            }
        }
    }

    void halos() {
        if (!neighbors.empty()) {
            requests.clear();
            for (const auto& face : neighbors) face.pack(u);
            // MPI may read a device pointer immediately after the pack.
            Kokkos::fence();
            for (const auto& face : neighbors) face.post(dec.cart_comm, requests);
            MPI_Waitall(static_cast<int>(requests.size()), requests.data(), MPI_STATUSES_IGNORE);
            for (const auto& face : neighbors) face.unpack(u);
        }
        // Empty legacy 2D tiles can copy a just-received ghost to their physical face.
        for (const auto& face : boundaries) face.fill(u);
    }

    template <bool volume, bool file>
    void stencil(DeviceWind lo, DeviceWind hi, double a) {
        constexpr bool robust = file || volume;
        const auto x = Axis::make<robust>(cfg.dt, cfg.dx, cfg.D, cfg.vx);
        const auto y = Axis::make<robust>(cfg.dt, cfg.dy, cfg.D, cfg.vy);
        const auto z =
            volume ? Axis::make<true>(cfg.dt, cfg.dz, cfg.vertical_diffusion(), cfg.vz) : Axis{};
        using Policy =
            Kokkos::MDRangePolicy<Kokkos::Rank<3, Kokkos::Iterate::Right, Kokkos::Iterate::Right>>;
        InteriorStencil<volume, file> update{
            u.data(), scratch.data(), nx, ny, pitch, origin, plane, x, y, z, lo, hi, a, cfg.dt};
        Kokkos::parallel_for(
            "advection diffusion", Policy({0, 0, 0}, {nz, ny, nx}, {1, 4, 32}), update);
        std::swap(u, scratch);
    }
};

KokkosStep::KokkosStep(const Field& initial, const Decomposition& dec, const SimConfig& cfg)
    : state_(std::make_unique<State>(initial, dec, cfg)) {}
KokkosStep::~KokkosStep() = default;

void KokkosStep::advance(const std::optional<WindSample>& wind) {
    auto& s = *state_;
    s.halos();
    if (!s.nx || !s.ny || !s.nz)
        return;
    if (wind) {
        if (wind->lower_index >= 0 && wind->lower_index == s.upper.index)
            std::swap(s.lower, s.upper);
        s.lower.load(wind->lower_index, wind->lower);
        const bool same = &wind->lower == &wind->upper;
        if (!same)
            s.upper.load(wind->upper_index, wind->upper);
        auto hi = same ? s.lower.tile : s.upper.tile;
        if (s.hz)
            s.stencil<true, true>(s.lower.tile, hi, wind->fraction);
        else
            s.stencil<false, true>(s.lower.tile, hi, wind->fraction);
    } else if (s.hz)
        s.stencil<true, false>({}, {}, 0);
    else
        s.stencil<false, false>({}, {}, 0);
}

void KokkosStep::stage_output(Field& host) const {
    Kokkos::deep_copy(HostOutput(host.data.data(), host.data.size()), state_->u);
}

void KokkosStep::fence() const { Kokkos::fence(); }
