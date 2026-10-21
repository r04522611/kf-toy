# Kalman Filter 教學 library：C++17／Ubuntu Docker

獨立專案：`/home/pinghua/kf-toy`，branch `codex/kf-cpp-toy`。原 SLAM repository 僅作唯讀公式、資料與封存結果 reference；新程式不需 ROS、Sophus、PCL、OpenCV 或原 repo 的 binary／headers。

## 已實作的內容

- `kf3d` static library：Step1、Step2、Step3、Step4、Step5A、Step5B、C15_A、C15_B；固定 gravity。
- 三種 backend：single-pass KF、fixed-prior GN、LM；共有24種具名組態。
- Square-root propagation／Joseph update、finite manifold reset、PSD prior support。
- 公開 virtual `Formulation`、`MeasurementModel`、`NonlinearSolver` 與 `Estimator`；`Filter<Recipe>` 提供 typed state／covariance facade。
- Body velocity＋顯式NHC使用約定、GNSS antenna position、mounted heading、TG-only explicit virtual-bias constraint。
- CSV replay：兩筆初始化，初始化後 pure DR 或 heading-only；完整10／11共96 cells。
- 來源／輸入／binary hashes、相同binary逐byte重跑、共同scorer、80個封存reference指標。
- 解析／有限差分測試及具有truth的教學情境；[教學導覽](docs/teaching.md)。

C15_A 與 fixed-g Step3 共用 formulation。C15_B 是同一15D formulation的 navigation midpoint output。A/B 的 GN／LM 使用相同 exact likelihood，因此預期一致；C-star 不當成 cost derivative。

此版本是可獨立使用、受測的研究／教學library。核心使用小型 dynamic Eigen matrices，typed facade不等於無heap或硬即時保證。ROS adapter、部署性能驗收、robust-loss／fixed-lag新研究不在本輪範圍。Estimated-gravity歷史變體也不與fixed-g結果混稱。

## 最小環境與測試

在 Ubuntu：

```bash
cd /home/pinghua/kf-toy
docker build -t kf-toy:dev .
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work kf-toy:dev bash -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2 && cmake -E chdir build ctest --output-on-failure'
```

只有 GCC、CMake、Make、Eigen 與 Python standard library；Ubuntu base digest已固定。建置image仍從apt取得套件，因此image ID、package versions與source hash必須封存，不能宣稱未來重新apt-build必定產生相同binary。

`typed_client` 是最小C++ library使用範例；`include/kf3d/typed.hpp` 區分15D PhysicalState與18D TangentGroupState。

```cpp
#include <kf3d/typed.hpp>
using namespace kf3d;
Filter<Recipe::Step2> filter(Backend::GN);
PhysicalState initial;
Filter<Recipe::Step2>::Covariance P = Filter<Recipe::Step2>::Covariance::Identity() * .01;
filter.initialize(initial, P);
filter.advance(.1, V3::Zero(), V3(0,0,9.81));
filter.observePosition(V3(1,0,0));
auto pose = filter.state();
```

`cmake --install build --prefix /chosen/prefix` 安裝library／headers；下游 `find_package(kf3d CONFIG REQUIRED)`、連結 `kf3d::kf3d`。

## 10／11轉換與單組replay

```bash
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" -v /home/pinghua/slam_in_autonomous_driving/data/ch3:/source:ro -w /work kf-toy:dev python3 tools/convert_events.py /source/10.txt data/events/10.csv --recording 10
```

Converter拒絕覆寫existing CSV。CSV保留數字token與原始順序，不做sensor interpolation。完整契約見 [data-contract.md](docs/data-contract.md)。

```bash
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work kf-toy:dev ./build/replay3d --input data/events/10.csv --output results/one --recipe step5b --backend kf --mode pure --protocol archive
```

Output directory必須不存在。`--recipe`：step1/step2/step3/step4/step5a/step5b/c15a/c15b；`--backend`：kf/gn/lm；`--mode`：pure/heading。

Replay採 `configs/research-twofix.json` 所描述的封存protocol固定設定；此JSON是機器可讀的protocol描述，目前runner不接受任意JSON覆寫。一般library呼叫者透過typed `Config`設定。未定義的調參不混入reference reproduction。

## 一次執行完整重現

```bash
./tools/reproduce.sh /home/pinghua/slam_in_autonomous_driving/data/ch3 research
```

包含build、CTest、CSV conversion（缺少時）、96 cells＋每cell第二次逐byte核對、4 seeds教學情境與SVG圖。完整矩陣比單一toy花費更多時間。`run_matrix.py` 可直接指定 `--recipes`、`--backends`、`--datasets`、`--modes`、`--jobs`做子集；子集使用獨立output以免誤寫總表。

```bash
python3 tools/run_matrix.py --binary build/replay3d --data data/events --output results/research --jobs 4 --repeat --protocol archive
```

已有complete seal且binary／input／script／protocol相同的run可resume。任何signature或輸出變動會拒絕重用，要求新output；失敗／中斷資料夾不當作完成。`--reference-tolerance METRES` 可強制封存水平與3D RMSE容差；預設先報告差異，避免把未驗證差異藏起來。

### Archive 與 uniform protocol

- `archive`：Step1–4使用legacy absolute timestamp子步；GN／LM重算native covariance eigen-root。TG使用carried roots與duration子步；C15沿其原本duration策略。這保留舊研究的數值選擇。
- `uniform`：全部使用carried-root whitening與duration子步，適合對照不同formulation；不承諾與舊LM有限迭代結果逐byte一致。

兩者sensor／noise／initialization相同。LM diagonal damping與停止條件受whitening basis影響；不能把這個數值策略差異歸因於geometry。每run config記錄實際profile。C15 GN／LM是新增能力，沒有捏造舊reference。

## 結果與判讀

- `states.txt`：timestamp、p、quaternion(wxyz)、v、bg、ba，約10Hz。
- `covariance.csv`：1Hz physical15 covariance，順序theta_body,v_world,p_world,bg_body,ba_body；不是native P。
- `virtual.csv`：TG physical bnu；C15的零是結構值。
- `gnss.csv`：相同evaluation時刻的antenna residual、fused flags與初始化標記。
- `innovations.csv`：NIS、solver iteration、cost與status；拒絕／budget狀態可見。
- `config.json`／`initialization.json`／`runtime.json`：noise、外參、solver設定、兩筆fix與clock checks。
- matrix `comparison.csv`／`matrix.json`：各cell分數、reference差與determinism結果。
- 教學 `metrics.csv`／`trace.csv`／`recovery.csv`／`remainder.csv`／`constraints.csv`／`anchor.csv`與SVG。

真實資料指標是GNSS antenna residual proxy，沒有獨立完整SE3／bias truth；heading-only融合後heading residual不是獨立yaw accuracy。相同binary重跑逐byte一致與「重現封存算法指標」是不同驗證，總表分開報告。跨CPU／compiler／libm的bitwise identity不作保證。

## 本輪驗證結果與已知差異

最終96個實測cells全部完成，且每cell重跑結果逐byte相同。8組CTest通過。80個歷史reference中，78個在原宣告1mm RMSE差內；11 pure DR的Step3 GN與Step4 LM差約1.226mm／1.077mm，這兩個嚴格檢查未通過。沒有調參或放寬門檻來改寫結果。

完整診斷見 [numerical-reproduction.md](docs/numerical-reproduction.md)。預設report分開execution／determinism與archive容差；若要求後者也必須通過，使用 `tools/report_validation.py --require-reference`。目前該命令應nonzero，這是已知、保留的結果。完整本地表在 `results/research/validation.md`，精簡摘要在 `references/validation-summary.json`。

## 教學執行

```bash
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work kf-toy:dev bash -c './build/kf_teaching results/teaching 4 12 && python3 tools/render_results.py results/teaching'
```

Closed-form truth獨立於filter propagation；IMU midpoint held sampling是近似matched case，coverage不是精確Bayesian證明。Seeds為獨立比較單位，時間samples相關。Scalar、SO3／ZOH、geometry、bias mismatch、heading/dropout、GN／LM恢復、Cstar remainder、TG structural／soft constraint各有對照；詳見[教學文件](docs/teaching.md)。

原始平面toy `kf`／`replay`／`tools/verify.py`仍保留為第一課；它不輸出完整pose covariance。

設計與研究報告位於忽略的 `plan/`。外部來源、UTM notices與file hashes見[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)及 `references/`。
