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

exe="${BUILD_DIR}/src/climate_sim"
if [ ! -x "${exe}" ]; then
  echo "ERROR: ${exe} not found or not executable. Build first with CMake." >&2
  exit 1
fi

ts=$(date +"%Y%m%d_%H%M%S")
out_dir="bench/results"
mkdir -p "${out_dir}"

strong_csv="${out_dir}/strong_${ts}.csv"
weak_csv="${out_dir}/weak_${ts}.csv"
strong_annot="${out_dir}/strong_annotated_${ts}.csv"

run_and_parse() {
  local np=$1 nx=$2 ny=$3 steps=$4
  local line time perstep
  line=$(${TIME_CMD} mpirun ${OVERSUB} -np "${np}" "${exe}" \
    --nx="${nx}" --ny="${ny}" --steps="${steps}" | grep "timing:")

  time=$(echo "$line" | sed -E 's/.*total_max=([0-9.e+-]+).*/\1/')
  perstep=$(awk -v t="$time" -v s="$steps" 'BEGIN {printf "%.8f", t/s}')
  echo "$time,$perstep"
}

# -------------------------
# Strong scaling
# -------------------------
echo "# strong scaling: Nx=${STRONG_NX}, Ny=${STRONG_NY}, steps=${STEPS}" > "${strong_csv}"
echo "ranks,nx,ny,steps,total_time,perstep_time" >> "${strong_csv}"

for p in ${STRONG_RANKS}; do
  echo "== strong: p=${p} =="
  vals=$(run_and_parse "$p" "$STRONG_NX" "$STRONG_NY" "$STEPS")
  echo "${p},${STRONG_NX},${STRONG_NY},${STEPS},${vals}" >> "${strong_csv}"
done

T1=$(awk -F, 'NR==3 {print $5}' "${strong_csv}")
if [ -n "${T1:-}" ]; then
  awk -F, -v T1="${T1}" '
    BEGIN {OFS=","}
    NR==1 {print; next}
    NR==2 {print $0,",speedup,efficiency,karp_flatt"; next}
    /^#/ {print; next}
    {
      P=$1; Tp=$5+0
      S=(Tp>0)? T1/Tp : 0
      E=(P>0)? S/P : 0
      KF=(P>1 && S>0)? ((1/S - 1/P)/(1 - 1/P)) : 0
      print $0,S,E,KF
    }
  ' "${strong_csv}" > "${strong_annot}"
  echo "Annotated strong-scaling results written to ${strong_annot}"
else
  echo "WARNING: could not determine T1 baseline; skipping annotation."
fi

# -------------------------
# Weak scaling
# -------------------------
echo "# weak scaling: tile=${WEAK_TILE_NX}x${WEAK_TILE_NY}, steps=${STEPS}" > "${weak_csv}"
echo "ranks,nx,ny,steps,total_time,perstep_time" >> "${weak_csv}"

for p in ${WEAK_RANKS}; do
  k=$(echo "sqrt(${p})" | bc)
  if [ $((k * k)) -lt "${p}" ]; then
    k=$((k + 1))
  fi
  NX=$(( WEAK_TILE_NX * k ))
  NY=$(( WEAK_TILE_NY * k ))

  echo "== weak: p=${p}, Nx=${NX}, Ny=${NY} =="
  vals=$(run_and_parse "$p" "$NX" "$NY" "$STEPS")
  echo "${p},${NX},${NY},${STEPS},${vals}" >> "${weak_csv}"
done

echo
echo "Wrote:"
echo "  ${strong_csv}"
[ -f "${strong_annot}" ] && echo "  ${strong_annot}"
echo "  ${weak_csv}"
