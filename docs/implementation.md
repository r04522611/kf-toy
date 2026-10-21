# 實作契約與設計對照

本輪依 `plan/KF.md` 實作 fixed-gravity Step1–5B／C15、KF／GN／LM、CSV replay 與教學；ROS、硬即時、全歷史 H1–H7／graph experiments 的重跑不在本輪範圍。

## Formula → source

| 契約 | 實作 |
|---|---|
| SO3／SE₂(3)、semidirect／TG finite maps | `src/kf3d/geometry.cpp` |
| ZOH Gamma與bias sensitivity、3點Q quadrature | `src/kf3d/propagation.cpp` |
| TG analytic A／RK4 transition／Simpson noise columns | 同上 |
| Virtual Formulation／exact MeasurementModel／solver strategy | `include/kf3d/filter.hpp` |
| QR Gaussian engine、Joseph、fixed-prior iteration、posterior builder | `src/kf3d/filter.cpp` |
| Compile-time recipe／state／covariance facade | `include/kf3d/typed.hpp` |
| Static initializer、UTM、兩筆fix、event order | `src/replay3d.cpp` |
| CSV roundtrip、source hash、score／matrix／repeat | `tools/` |
| Independent analytic／FD tests | `tests/math_contracts.cpp` |
| Input／information-leak integration tests | `tests/test_replay_protocol.py` |

## 維度與狀態

Production-facing typed interface 分開 `PhysicalState` 和 `TangentGroupState`，其 covariance固定15／18維。Virtual核心採一個internal State storage方便runtime recipes，包含fixed g與bnu槽；非TG初始化若給非零bnu會拒絕，不靜默忽略。Physical covariance依theta_body,v_world,p_world,bg_body,ba_body[,bnu_body]排列。g是參數，沒有gravity covariance。

目前不是全static Eigen實作。Facade以template固定對外維度，內部dynamic matrices與QR workspace會分配記憶體。這是目前已驗證的數值核心，不宣稱滿足無allocation或硬即時規格；部署前需另profile及配置buffer。

## Atomic update

所有measurement在local candidate上計算；state／root通過finite checks後一起提交。失敗保持prior。Out-of-order／nonfinite IMU、wrong covariance／custom measurement shapes、invalid configuration拒絕。`state()`在initialize之前拒絕。

GN／LM每次迭代使用同一prior與noise，candidate `Retract(prior,L*z)`。Final covariance使用undamped資訊，將root映到final tangent一次。PSD prior允許zero directions；不加入虛構jitter。TG不由native完整P重新factor，維持tiny virtual conditional root。

## Solver 行為

- GN：fixed-prior GN step＋backtracking。
- LM：diagonal-scaled damping，拒絕後lambda×10，接受後÷3，與封存reference一致。
- 最大iteration／trial、step／gradient／cost tolerance可設定。
- 可選time budget，預設0（disabled），維持deterministic replay。
- `status`區分kf、gradient、step、cost、capped、stalled、time_budget。
- Capped或budget停止時，輸出最後有效candidate與其local posterior；不可把status當作已收斂。
- 預設診斷記錄每次update的iteration count／initial-final cost／拒絕次數；不記錄所有trial矩陣，避免熱路徑I/O。若需每trial trace，可另加observer，不修改inference公式。

## 數值compatibility

Archive profile記錄legacy absolute clock與eigen-whitening；uniform profile使用duration與carried roots。這是可追溯的numerical policy，不是資料調參。Library預設uniform numerical behavior，runner預設archive，以重現歷史protocol。

Original physical full-P propagation重構成root QR會有roundoff差異。跨實作的archive比較使用事前指定1mm水平／3D RMSE差；相同binary第二次輸出則逐byte比較。請看實際run summary，不把這兩種驗證混為一談。

## Measurement assumptions

`observeVelocity(z)`表示三維body velocity，noise取Config.wheel_std。Replay傳入(speed,0,0)時，側向／垂向是NHC pseudo constraints，不是兩個真實sensor zeros。若要真正一維wheel，可透過public exact MeasurementModel提供相應H與1×1 noise。Cstar是受支援配對的single-pass output，exact custom models不會走Cstar。

`observeVirtualBias`只允許TG，target與finite-noise covariance必須顯式給定。核心不決定schedule；replay預設無anchor，教學anchor case獨立記錄其R與頻率。C15沒有virtual latent state，不接受anchor。

## 可重現性邊界

同image／binary／input／protocol無time-budget時，CSV可逐byte重現。Compiler evaluation order、Eigen、libm與architecture改變可能影響浮點與noise sample order；不聲稱跨平台bitwise。教學truth與filter不共用flow helper，但time-varying IMU的held-input離散化仍有近似，因此coverage只作診斷。

當前matrix資料是10／11原始recordings，同一recording多方法不是獨立道路證據；教學新seeds也不是新增實車真值。未實作全局模式切換器、robust loss、fixed-lag、未知clock或estimated gravity。

## 自訂formulation／solver

`makeEstimator(compatibility_recipe, backend, config, custom_formulation, custom_solver)` 可直接接收使用者繼承的物件。Null選內建；KF拒絕nonlinear solver。Compatibility recipe仍定義內建measurement/output的state layout與對稱性前提；自訂formulation須保留該契約，不能只因維度相同就假定C-star仍適用。

`tests/custom_components.cpp` 示範裝飾內建formulation、注入GN solver及自訂一維position measurement，並核對確實呼叫derived物件。新的hypothesis可替換相容dynamics／noise／chart/reset，無需複製Gaussian engine。若更換整個state schema或output symmetry，需新增明確profile及其數學測試。
