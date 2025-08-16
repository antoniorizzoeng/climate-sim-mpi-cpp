#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
STEPS="${STEPS:-200}"
OVERSUB="${OVERSUB:-}"
TIME_CMD="${TIME_CMD:-}"

STRONG_NX="${STRONG_NX:-1024}"
STRONG_NY="${STRONG_NY:-1024}"
STRONG_RANKS="${STRONG_RANKS:-1 2 4 8}"

WEAK_TILE_NX="${WEAK_TILE_NX:-256}"
WEAK_TILE_NY="${WEAK_TILE_NY:-256}"
WEAK_RANKS="${WEAK_RANKS:-1 4 16}"

if [ ! -x "${BUILD_DIR}/bench/bench_exchange" ]; then
  cmake -S . -B "${BUILD_DIR}" -DBUILD_TESTING=OFF -DBUILD_BENCH=ON
  cmake --build "${BUILD_DIR}" --parallel
fi

exe="${BUILD_DIR}/bench/bench_exchange"
ts=$(date +"%Y%m%d_%H%M%S")
out_dir="bench/results"
mkdir -p "${out_dir}"

strong_csv="${out_dir}/strong_${ts}.csv"
weak_csv="${out_dir}/weak_${ts}.csv"
strong_annot="${out_dir}/strong_annotated_${ts}.csv"

# -------------------------
# Strong scaling
# -------------------------
echo "# strong scaling: Nx=${STRONG_NX}, Ny=${STRONG_NY}, steps=${STEPS}" > "${strong_csv}"
echo "ranks,Px,Py,nx,ny,nx_local,ny_local,steps,halo,total_max,total_min,perstep_worst,rss_kb_max" >> "${strong_csv}"

for p in ${STRONG_RANKS}; do
  echo "== strong: p=${p} =="
  ${TIME_CMD} mpirun ${OVERSUB} -np "${p}" "${exe}" \
    --nx="${STRONG_NX}" --ny="${STRONG_NY}" --steps="${STEPS}" \
    | tail -n1 >> "${strong_csv}"
done

T1=$(awk -F, '
  NR==2 { for(i=1;i<=NF;i++) h[$i]=i; next }
  $0 ~ /^#/ { next }
  $1 ~ /^[[:space:]]*$/ { next }
  ( $h["ranks"]+0 )==1 { print $(h["total_max"]); exit }
' "${strong_csv}" || true)

if [ -n "${T1:-}" ]; then
  awk -F, -v T1="${T1}" '
    function map_header() { for(i=1;i<=NF;i++) h[$i]=i }
    NR==1 { print; next }
    NR==2 { map_header(); print $0",speedup,efficiency,karp_flatt"; next }
    $0 ~ /^#/ { print; next }
    {
      P = $(h["ranks"])+0
      Tp = $(h["total_max"])+0
      S = (Tp>0) ? (T1/Tp) : 0
      E = (P>0) ? (S/P) : 0
      if (P>1 && S>0) {
        KF = ( (1.0/S - 1.0/P) / (1.0 - 1.0/P) )
      } else {
        KF = 0.0
      }
      printf "%s,%.6f,%.6f,%.6f\n", $0, S, E, KF
    }
  ' "${strong_csv}" > "${strong_annot}"
else
  echo "WARNING: Could not determine T1 (no ranks=1 row?) — skipping Karp–Flatt annotation." >&2
  strong_annot=""
fi

# -------------------------
# Weak scaling
# -------------------------
echo "# weak scaling: tile=${WEAK_TILE_NX}x${WEAK_TILE_NY}, steps=${STEPS}" > "${weak_csv}"
echo "ranks,Px,Py,nx,ny,nx_local,ny_local,steps,halo,total_max,total_min,perstep_worst,rss_kb_max" >> "${weak_csv}"

for p in ${WEAK_RANKS}; do
  k=$(echo "sqrt(${p})" | bc)
  if [ $((k * k)) -lt "${p}" ]; then
    k=$((k + 1))
  fi

  NX=$(( WEAK_TILE_NX * k ))
  NY=$(( WEAK_TILE_NY * k ))

  echo "== weak: p=${p}, Nx=${NX}, Ny=${NY} =="
  ${TIME_CMD} mpirun ${OVERSUB} -np "${p}" "${exe}" \
    --nx="${NX}" --ny="${NY}" --steps="${STEPS}" \
    | tail -n1 >> "${weak_csv}"
done

echo
echo "Wrote:"
echo "  ${strong_csv}"
[ -n "${strong_annot}" ] && echo "  ${strong_annot}"
echo "  ${weak_csv}"