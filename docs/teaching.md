# KF 教學案例：從可解析答案到研究邊界

這份教材搭配 `kf_teaching` 與 `kf_math_tests`。目標是讓讀者在同一組資料、prior、noise 與評分下，辨認 formulation、output approximation 與 nonlinear solver 各自改了什麼。它們不是算法排名，也不以真實資料 GNSS proxy 當成完整 pose truth。

## 執行與產物

在 README 的 Docker 環境完成編譯後：

```bash
./build/kf_math_tests
./build/kf_teaching results/teaching 4 12
python3 tools/render_results.py results/teaching
# optional: python3 tools/plot_examples.py results/teaching
```

第三個參數是 seeds 數，第四個參數是總秒數；沒有參數時為 `results/teaching 4 12`。繪圖是可選步驟，需要 matplotlib；它不參與 estimator、評分或 deterministic 核對。最小 C++ runtime 不依賴 Python 或 ROS。繪圖可以在 Ubuntu host 上執行。

輸出如下：

| 檔案 | 內容 |
|---|---|
| `metrics.csv` | 每個 scenario × seed × recipe × backend 的位置／姿態 RMSE、normalized NEES15、z coverage、velocity NIS |
| `trace.csv` | seed 0 的時間序列，包含 truth、estimate、bias error、NEES 與 heading availability |
| `remainder.csv` | 七個方向、多個尺度的 ordinary／midpoint noiseless remainder |
| `constraints.csv` | 線性 tangent support 與 nonlinear constrained state 的差異 |
| `recovery.csv` | 同一個大姿態誤差的 KF／GN／LM 單次量測更新 |
| `anchor.csv` | 另行比較 TG B 無 anchor、有限 noise anchor、C15_B structural constraint |
| `integration.csv` | held／varying force、left／midpoint sample 的 Step1／Step2 步長掃描 |
| `so3.csv` | SO(3) exp、線性加法與 finite reset 的大小角度比較 |
| `assumptions.csv` | 正確 heading、錯誤 heading likelihood、錯誤而過度自信 prior |
| `slip.csv` | 直接 3D velocity 與 forward＋NHC，在無側滑／有側滑的比較 |
| `scalar.csv` | 手算可對照的一維 random walk KF |
| `manifest.txt` | seed、採樣、truth、dropout 與 model mismatch 定義 |

同一個 binary、平台與輸入參數應輸出完全相同的 CSV。編譯器、Eigen、libm 或架構變更後，不宣稱輸出仍逐 byte 相同；應重新跑數學測試與 numerical regression。runtime 不寫入 deterministic CSV。

## 第一課：一維 KF 如何分配信任

`scalar.csv` 使用每步固定加上已知輸入 0.1 的一維 random walk。process variance 為 0.01，measurement variance 為 0.04。公式是：

\[
P^- = P^+ + Q,\quad K=P^-/(P^-+R),\quad
\hat x^+=\hat x^-+K(z-\hat x^-).
\]

Joseph form：

\[
P^+=(1-K)^2P^-+K^2R.
\]

先讓同事手算第一筆，再觀察 gain 的穩定值。這個標量例子直接實作公式，用作進入 3D library 前的概念教材；`math_contracts` 另外用 library 的線性 position observation 驗證解析 posterior，防止只測到教材本身。

## 第二課：離散化前提，比「精確」這個字重要

數學測試使用固定 angular velocity \(\omega_z=2\) 與 body specific force \(f_x=3\)，時間長度 \(h=0.5\)。精確答案：

\[
v_x=3\sin(2h)/2,\quad v_y=3(1-\cos(2h))/2,
\]

\[
p_x=3(1-\cos(2h))/4,\quad
p_y=3(h/2-\sin(2h)/4).
\]

Step2 的 ZOH flow 應符合答案；Step1 會有可見誤差。這裡「精確」是步內 body input 不變的 mean flow；不代表 sample 內真實訊號一定不變，也不代表 stochastic covariance 是所有情況的精確解。

完整 teaching trajectory 的 speed 隨時間變化，IMU 取 interval midpoint 再作 held input。這刻意保留小量時間離散誤差，避免 truth generator 直接呼叫 estimator 的 Gamma helper，得到循環驗證。

## 第三課：chart、physical covariance 與 finite correction

`math_contracts` 在非零姿態、速度、位置與 bias 上檢驗：

1. `local(prior,retract(prior,delta)) ≈ delta`。
2. native correction 到 physical error 的 Jacobian，以 central finite difference 核對。
3. correction 後新 tangent 的 reset Jacobian，以 central finite difference 核對。
4. SO(3) 在接近零與接近 pi 的 exp/log round trip。

共同模型 Step1 與 Step2 均使用精確 SO(3) right Jacobian reset，符合本次 fixed-gravity reference；更早 baseline／mean-only 的 first-order reset 是另一個近似，不應混稱。

Physical error 順序固定為 `[theta_body, v_world, p_world, bg_body, ba_body]`。比較不同 formulation 的 uncertainty 必須先映到此 physical chart；native covariance 的同一個 index 不代表同一個物理方向。

## 第四課：Step3 與 C15_A 是等價性教材

固定 gravity、相同 prior、事件、noise 與 output 下，Step3 與 C15_A 共用 right-invariant navigation＋physical additive bias formulation。測試同時跑 KF／GN／LM，核對多步 state 與 physical covariance。

這兩個 recipe 名稱保留研究脈絡，不當成兩份獨立性能證據。若兩條線完全重疊，這正是預期結果。Production 應共用底層公式，而非複製一份 C15_A 再讓兩份公式逐漸分歧。

## 第五課：C15_B 的 output 階數

令 navigation perturbation 為 \(\eta=(\theta,\rho_v,\rho_p)\)，\(z\) 由無噪聲 perturbed state 生成，\(r=\hat Rz-\hat v\)。比較：

\[
e_A=r-\rho_v,\qquad
e_B=r-\rho_v-\tfrac12[r]_\times\theta.
\]

同時縮小所有 perturbation 分量，ordinary remainder 應是二階，midpoint remainder 是三階；尺度減半時，norm 比約為 4 與 8。數學測試以七個方向及五個尺度核對；CSV 額外輸出較小尺度供繪圖。

這是無噪聲的 local remainder 性質。含噪時 update matrix 依 measurement 改變，不能把 cubic remainder 直接解讀成 calibrated posterior 或更低長程 RMSE。實際 replay 的跨資料取捨見 `plan/` 研究報告，不以這個小 toy 推翻或替代原結果。

## 第六課：GN／LM 是如何使用同一筆資訊

`recovery.csv` 設定 prior yaw=1.2 rad，已知 world velocity 接近 `(2,0,0)`，body velocity measurement 為 `(2,0,0)`。姿態 prior 寬、velocity prior 窄，使 body velocity residual 提供姿態恢復訊息。

每種 backend 使用同一 prior 與 likelihood。GN／LM 迭代時固定 prior，不把上一輪 posterior 當成新的 prior；否則同一 measurement 會被重複計算，covariance 人為縮小。

閱讀順序：

1. 看 KF 的單次 correction 與 nonlinear residual 是否仍大。
2. 看 GN／LM 的 `initial_cost`、`final_cost`、iterations 與 status。
3. 比較線性 position 測試：KF／GN／LM 必須回到解析答案，迭代次數不能改變 evidence 數量。
4. 看 C15_A／C15_B 與 Step5A／Step5B 在 GN／LM 下的 equivalence test。

Ordinary／C-star 只影響 single-pass output approximation。GN／LM 使用相同 exact likelihood 與 derivative，因此 A/B iterative 結果應一致。LM damping 是 solver 的穩定工具，不能直接當成最終 posterior 的資訊。

「更充分求解」並不保證「假設更正確」；接著看 bias mismatch case。

## 第七課：matched、heading dropout、pure DR 與模型錯配

全部 scenarios 使用同一條 closed-form varying-speed circle：

\[
R=R_z(0.3t),\quad v=R[2+0.3\sin(0.7t),0,0]^\top.
\]

位置以解析積分生成；IMU mean 由曲線的導數直接計算，沒有呼叫 estimator 的 propagation。noise 使用固定 seed 的 mt19937 與明確 Box–Muller 轉換，所有 recipe 共用同一組 samples 與 physical prior。

| Scenario | 假設 | 要觀察什麼 |
|---|---|---|
| `matched_heading` | 固定 physical biases；正確 IMU／velocity／heading noise | baseline error、NEES、coverage，勿預期小 sample 正好 95% |
| `heading_dropout` | 中間 35%–75% 時間沒有 heading | 姿態／bias uncertainty 與 dropout 後恢復 |
| `pure_dr` | 全程只用 IMU＋完整 body velocity | global yaw 的 gauge freedom，外援資訊缺失不能只靠換 chart 解決 |
| `bias_mismatch` | 半程後額外 +0.12 rad/s gyro z step，filter bias random walk 為零 | 錯誤 process model 下，較低 optimizer cost 可能仍過度自信 |

這裡 velocity 是真正三維 body velocity observation，包含明確加上的三軸 noise；不是 forward wheel speed 加兩個真實 sensor zeros。真車若使用 NHC，側向／垂向為 pseudo constraints，必須獨立描述其 noise 與失效條件。

Prior error 的各分量以 physical chart 指定，所有 formulation 使用相同 physical covariance。初始姿態由 true attitude 右乘 physical rotation perturbation；速度、位置與 physical bias 加性擾動。

四個 seed 的 teaching default 是小型示範，不能用來宣稱統計顯著或 calibrated coverage。需要穩定統計時增加 seeds，保留 paired comparisons，分開報告 seed 間變異。時間序列 samples 相關，不能把每個 timestamp 當成獨立 Monte Carlo trial 計算信賴區間。

### 評分定義

\[
e=[\operatorname{Log}(\hat R^TR),\ v-\hat v,\ p-\hat p,\ b_g-\hat b_g,\ b_a-\hat b_a],
\]

\[
\operatorname{NEES}_{15}=e^TP_{physical,15}^{-1}e.
\]

`normalized_nees15` 是每個 velocity event 的 NEES 平均後除以 15；`z_95_coverage` 是 \(|e_z|\le1.95996398454\sqrt{P_{zz}}\) 的比例。`mean_velocity_nis` 在 velocity update 完成後、heading update 前讀取；它不混入 heading residual。不同量測型別的 NIS 必須分開報告。

Pose RMSE 是 world frame 下直接誤差，不作事後 SE(3) alignment。此 toy 有真值；10／11 GNSS antenna residual 只能作 proxy，不能與這裡的 pose truth RMSE 混用。

## 第八課：結構約束不等於 anchor

`constraints.csv` 使用已推導的簡化子空間：prior navigation identity，perturbation 只有 position x 與 gyro bias y。先由 physical Jacobian 的 inverse 建立**線性 virtual perturbation 為零**的 TG native correction，再作 finite joint retraction。

\[
b_{\nu,new}=-\tfrac12\delta p\times\delta b_g.
\]

完整 TG finite correction 可得到非零 virtual bias；C15 的 reduced retraction 則自然保持 \(b_\nu=0\)。圖中 prediction 只適用於這個子空間，不應宣稱是任意 state 的全域公式。

這展示四種不同操作：

- reduced state representation：每個 candidate 都在 constraint surface 上。
- finite-noise anchor likelihood：保留 latent state，只增加 evidence。
- zero first-order tangent variance：限制線性 Gaussian support。
- hard mean clamp：事後改 mean，若 uncertainty 沒有相容處理就不是完整 Bayesian update。

主要 `metrics.csv` comparison **anchor 關閉**，以避免把 output comparison 與約束資訊混為一談。另有 `anchor.csv`，在 matched-heading 與 pure-DR 同樣 samples 下比較 TG_B 無 anchor、TG_B finite-noise anchor 與 C15_B structural constraint。該 study 的 TG virtual prior variance 為 0.01、anchor variance 為 0.001、每 0.2 秒更新一次；它使用明確的共同 physical prior。這是刻意設計的教學組態，不是封存實驗的參數替代品。C15 沒有 virtual state，不加入 anchor。這個 toy 沒有證明 anchor 失效的唯一根因是 curvature；C15 同時改了 dynamics、bias retraction 與 Gaussian family。


## 第九課：用步長掃描拆開「積分公式」與「sample 假設」

`integration.csv` 將總時長固定為 1 秒，角速度固定 2 rad/s；掃描步長 1、0.5、0.2、0.1、0.05、0.025 秒。每個步長分別使用 interval 左端 sample 和 midpoint sample。

- `held_force`：body x force 恆為 3。Step2 的 mean 應符合解析答案，與步長無關；Step1 隨步長縮小改善。
- `varying_force`：body x force 為 3 cos(1.7t)。真實訊號在 sample 之間持續變化，Step2 即使對 held sample 積分精確，仍有 input approximation error。

第二種 case 的 true velocity／position，由 cos(1.7t)cos(2t) 與 cos(1.7t)sin(2t) 的解析積分獨立生成。不是用更小 dt 呼叫同一套 Gamma helper 作 truth。對照 left／midpoint sampling，可以看出更準確地描述 interval input，與換用更好的 held-input flow，是兩個不同設計選擇。

不要將所有步長的相對優劣硬編成「某法必勝」測試；只有符合前提的 held-force Step2 使用解析 exactness oracle。

## 第十課：SO(3) 加法近似如何失效

`so3.csv` 固定一個一般三維旋轉軸，掃描接近零到接近 pi 的角度，輸出：

- exp(phi) 的 orthogonality error。
- I＋hat(phi) 的 orthogonality error。
- exp(log(exp(phi))) 的 rotation matrix round-trip error。
- exact right reset Jacobian 與 I－0.5hat(phi) 的差。

這是局部近似的教學，不把 rotation matrix 的逐項加法當成合法 estimator recipe。靠近 pi 時 log 的向量表示有分支；測試比較重建的 rotation matrix，不要求跨分支的 rotation vector 逐分量相同。

## 第十一課：optimizer 收斂，也可能相信錯的世界

`assumptions.csv` 另外固定 true yaw 為零、prior yaw 為 1.2 rad。三種 case 分別為：

| Case | Prior yaw variance | Heading observation | 想說明的事 |
|---|---|---|---|
| `correct_likelihood` | 2 rad² | 0 rad | 可信量測可修正大初始誤差 |
| `wrong_heading` | 2 rad² | 0.8 rad | 最佳化正確解出了一個偏離真實世界的 likelihood |
| `overconfident_wrong_prior` | 1e-5 rad² | 0 rad | 正確量測也可能被錯誤而過窄的 prior 壓過 |

三者 measurement standard deviation 都為 0.05 rad。每個 case 跑完整八種 recipe 與三種 backend。看 `final_cost` 的同時必須看 `absolute_truth_error`；cost 只描述設定的問題，不能驗證設定正確。KF 的 diagnostics cost 並非完整 optimizer 記錄，cost 下降只對 GN／LM 解讀。

這裡選擇單一純 yaw case，是讓所有 formulation 都能清楚呈現資訊權重問題；非線性改善另看 `recovery.csv` 的 body-velocity 大姿態恢復。

## 第十二課：真實 body velocity 與 forward＋NHC 不能混稱

`slip.csv` 固定 Step2，分別跑 KF／GN／LM。它使用獨立解析 circle truth，body velocity 為 `(2, lateral, 0)`，`lateral` 分別為 0 和 0.6 m/s。specific force 的水平分量直接由 omega cross body_velocity 計算，因而包含真實側滑的運動學。

兩種 update：

- `measured_3D_velocity`：三軸都是真實 body velocity 加 measurement noise。
- `forward_plus_NHC`：forward 軸沿用同一 noisy sample，側向與垂向是零的 pseudo constraint。

每個 variance 都明確設為 0.0025 (m/s)²；NHC 的兩個零沒有偽裝成獨立實測 noisy sample。無側滑時 NHC 假設成立，可以提供有用約束；有側滑時它是錯誤 likelihood，可能造成 velocity error、錯誤 bias feedback 或過低 coverage。heading 始終存在，避免把 NHC 失效與缺少全域 heading 混在一起。

此表輸出 pose RMSE、velocity NIS、最終 accelerometer bias norm 與 body lateral velocity 的 95% interval coverage。body lateral error／variance 都以 true orientation 的側向軸投影 physical velocity error／covariance；這是 toy 已知 truth 的評分軸，不是 estimator 偷看 truth 作更新。

這些 seed 與時長仍是小型教材，不以幾筆 coverage 宣稱統計精準，也不聲稱 GN／LM 能修復錯誤 NHC。

## 驗收邊界

`kf_math_tests` 是數學契約測試；teach CSV 是可以閱讀與重跑的實驗，不為每個 scenario 硬編碼「某算法必須勝出」。測試涵蓋解析 position posterior、ZOH mean、geometry derivatives、C15 constraints、chart 與 GNLM equivalence、nonlinear recovery、PSD 與 boundary rejection。

這不取代完整 10／11 protocol regression，也不構成 ROS、hard real-time、allocation budget、真實 sensor calibration 或安全認證。資料轉換、初始化與 GNSS 資訊模式見資料契約文件。

來源脈絡：研究方程與命名來自本專案 `plan/KF.md` 及唯讀 SLAM research POC；tests 的 finite-difference 與 remainder 結構參考原 `constrained_tg/test_constrained.cc`，此處以新 library public API 重新表達。研究來源與完整 provenance 請見專案來源紀錄。
