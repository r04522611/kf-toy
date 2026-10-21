#!/usr/bin/env bash
set -euo pipefail
# Run in Ubuntu; mount only this standalone repository and readonly source data.
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_data=${1:-/home/pinghua/slam_in_autonomous_driving/data/ch3}
run_name=${2:-research}
case "$run_name" in *[!a-zA-Z0-9_-]*|'') echo 'run name must use letters/digits/_/-' >&2; exit 2;; esac
cd "$project_root"
docker build -t kf-toy:dev .
docker run --rm --user "$(id -u):$(id -g)" -v "$project_root:/work" -v "$source_data:/source:ro" -w /work -e KF_RUN_NAME="$run_name" kf-toy:dev bash -euo pipefail -c '
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
cmake -E chdir build ctest --output-on-failure
mkdir -p data/events
for recording in 10 11; do
 if [ ! -f "data/events/$recording.csv" ]; then
  python3 tools/convert_events.py "/source/$recording.txt" "data/events/$recording.csv" --recording "$recording"
 fi
done
python3 tools/verify_inputs.py /source data/events
python3 tools/run_matrix.py --binary build/replay3d --data data/events --output "results/$KF_RUN_NAME" --jobs 4 --repeat
python3 tools/report_validation.py --matrix "results/$KF_RUN_NAME" --output "results/$KF_RUN_NAME/validation.md"
./build/kf_teaching "results/$KF_RUN_NAME-teaching" 4 12
python3 tools/render_results.py "results/$KF_RUN_NAME-teaching"
'
image_id=$(docker image inspect kf-toy:dev --format '{{.Id}}')
python3 tools/seal_run.py --root . --output "results/$run_name/source-manifest.json" --image-id "$image_id" --inputs data/events/10.csv data/events/11.csv
