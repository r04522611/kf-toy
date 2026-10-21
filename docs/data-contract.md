# 10.txt／11.txt 重建實驗的資料契約

## 建議：每份 recording 一個長格式 events.csv＋manifest.json

不要合併10／11的 state 或 clock；可共用 schema，但每份 recording 獨立初始化。CSV 每列一個 event，保持 source order。建議欄位：

`recording_id,sequence,timestamp_s,type,gx_rad_s,gy_rad_s,gz_rad_s,ax_m_s2,ay_m_s2,az_m_s2,left_pulse,right_pulse,latitude_deg,longitude_deg,altitude_m,heading_deg,heading_valid`

不適用欄位留空；timestamp 原始 double 用17位有效數字 round trip；若來源精確整數時鐘則保留 timestamp_ns，不能從低精度文字憑空產生精度。同 timestamp 使用 sequence 決定穩定順序。絕不先排序改變 source order；先驗證原始 clock，異常則回報並固定 repair policy。

原 SLAM TxtIO 的 records：

- `IMU time gx gy gz ax ay az`，原程式 gyro 直接使用 rad/s，沒有乘 degree conversion。
- `ODOM time left_pulse right_pulse`，保留原 pulses，不只保留已處理 speed。
- `GNSS time lat lon alt heading heading_valid`，保留原 heading convention 與 validity。

還原既有 runner 的 wheel speed 需固定 radius=0.155m、1024 pulses、pulse window=0.1s 等參數；這些是目前 runner 的設定，非普遍硬體事實。Heading 使用 wrap((90-heading_deg)*pi/180)。GPS 天線位置需相同 projection、origin、lever arm 與安裝 yaw。

## manifest 必需內容

1. Source file與CSV SHA256、converter版本、row counts及每類event counts。
2. Timestamp單位、event順序、previous-sample ZOH、max substep、pulse aperture及offset。
3. Frame／axis／gravity convention、GNSS projection implementation與版本、UTM zone及origin。
4. Static IMU initializer thresholds／window、initial bg/ba/tilt/g、initial physical covariance。不能只留下兩筆GNSS而刪掉前段靜止IMU。
5. 前兩筆有效 position/heading fix 的選擇與第二筆完整更新流程。
6. Continuous noise intensity、measurement variances、bias RW、gravity mode、extrinsics與wheel/NHC假設。
7. Information mode：pure DR初始化後不融合GNSS；heading-only僅融合有效heading。GNSS位置可供scoring但不得洩漏到更新。
8. Recipe、KF/GN/LM solver設定、gate、anchor R與schedule、code revision、image ID、compiler／flags／architecture。
9. GNSS評分時刻mask、direct antenna residual或扣初始化residual的定義；不可混用。

除events.csv外，完整可重現的最低資料包還需 config、initialization protocol、scorer與reference outputs/hashes。真實資料沒有獨立完整pose truth，應稱GNSS residual proxy，不能把融合過的heading residual稱獨立yaw accuracy。

## 輸出契約（完整3D版本待實作）

- states.csv：timestamp、position、quaternion（明確wxyz）、velocity、bg、ba、extra states。
- innovations.csv：source、fused/evaluation-only、r、NIS、dimension。
- covariance：明確physical layout與chart，不混gravity／bnu。
- metrics.json：相同mask的水平／垂向／3D antenna residual RMSE、更新次數、clock checks。
- run-manifest.json：input/config/source/image hashes；elapsed runtime另列，不放入deterministic output hash。

逐階段驗證：TXT→CSV roundtrip fields/counts/order；相同原filter跑TXT與CSV比對；再與新library比對。跨數值演算法可允許明確tolerance，但同一binary重跑應逐byte一致。不能為湊回舊分數調參。

## 本版toy的不同schema

Toy使用同步解析fixture，欄位為 `t_s,gyro_z_rad_s,speed_m_s,heading_rad,truth_x_m,truth_y_m,truth_yaw_rad`。它只測通最小端到端管線，不是完整3D events schema。Truth目前用於scorer，replay只驗證欄位不參與融合。

## 已實作的獨立 3D pipeline

`tools/convert_events.py SOURCE OUTPUT --recording 10` 會產生上述 17 欄 CSV 及旁邊的 `.manifest.json`。每個數字保留原始 decimal token；未經浮點格式化，亦未重新排序。Converter 拒絕未知類型、欄位數錯誤、NaN/Inf、逆序 timestamp、不合法 validity。CSV 的 `sequence` 是零起算連續事件編號。原始註解和空白不屬於事件，不會輸出。

`replay3d --input data/events/10.csv --output results/my-run --recipe step2 --backend kf --mode pure` 使用固定、版本控制的 protocol。Recipe 為 `step1 step2 step3 step4 step5a step5b c15a c15b`，backend 為 `kf gn lm`，mode 為 `pure heading`。輸出目錄必須不存在；失敗後留下目錄供診斷，重跑須使用新目錄或自行移走失敗目錄。

`configs/research-twofix.json` 記錄 replay 的固定參數；它是人可讀的規格，不是假裝已支援的 runtime override。Replay 不讀取任意 JSON config，避免產生「修改設定檔但實驗未變」的錯覺。

### 初始化與 clock

1. ODOM 左右 pulse 都 `<5` 才視為靜止。開始靜止後收集超過 10 秒 IMU；queue 上限 2000；mean 依 source order 加總，variance 為 `n-1` unbiased sample variance。
2. gravity 由加速度平均方向設定，大小 9.81。Gyro bias 為 gyro mean；accel bias 為 accel+gravity mean。Variance norm 必須分別小於等於 0.5、0.05。
3. Init success 後下一筆 IMU 使 filter ready。Noise density 為初始化前正 IMU intervals 中位數乘 sample variance，bias RW 為固定工程參數。
4. Ready 後第一筆 valid-heading GNSS 作 origin，從 IMU tilt 再套天線 heading 設 yaw，position 為 `-R*lever`。15 維 prior std 為 `(2deg,2deg,1deg; .1m/s x3; .1m x3; .001rad/s x3; .1m/s² x3)`，包含 lever 引入的 attitude-position correlation。
5. 第二筆 valid-heading fix 使用選定 KF/GN/LM 完整 joint antenna position+heading update。Residual 必須小於 1m 及 5deg 才開始 evaluation。
6. 每個事件先 advance 到它的 timestamp；IMU 事件到達前使用前一筆 IMU held input。沒有 timestamp-only advance，也不將長 gap 靜默丟棄。Max substep 為 .01s。
7. Evaluation 後 position GNSS 只評分；pure 不融合 heading，heading 只融合原始 validity=1 的 heading。Wheel 的 `(speed,0,0)` 包含兩個 NHC 假設，不是三軸實測。

### Output 與比較

- `states.txt`：以空白分隔 `timestamp p_xyz q_wxyz v_xyz bg_xyz ba_xyz`；與研究 archive 相同的至少 .1 秒 snapshot 節奏。
- `covariance.csv`：至少 1 秒 snapshot，row-major 15×15 physical covariance，layout `(rotation,velocity,position,gyro-bias,accel-bias)`；不把 virtual bias 誤標為 gravity。
- `virtual.csv`：與 covariance 同時輸出 physical virtual-bias mean。Reduced 15 維 recipes 應為零。
- `gnss.csv`：與舊 archive 相同欄位；initialization=1 的兩筆不計分，其他每筆都計分。`pre_*` 在 heading update 前，`rx/ry/rz` 在 update 後。Heading invalid 留空；不捏造 yaw。
- `innovations.csv`：每次量測更新的 NIS、iterations、cost、累積 rejected trials 與 status。它不是獨立 ground truth，也不應將 C-star NIS 直接當作 exact-likelihood consistency 的證明。
- `config.json`、`initialization.json`、`runtime.json`：記錄實際推得的 noise、fix timestamps、row counts 與更新計數；不含 wall clock，以便逐 byte 重跑。
- `metrics.json`：`tools/score_replay.py` 對 initialization=0 rows 計算 direct horizontal/vertical/3D antenna residual RMSE，另外明確列出 subtract-second-fix 的 horizontal increment metric。報告 primary 使用 direct metric，不能混用。

`tools/score_replay.py RUN --reference OLD/gnss.csv` 同時檢查相同事件時刻與 mask，回報 residual maximum absolute difference。它不使用 rigid trajectory alignment 或調參去逼近舊分數。GNSS altitude 是原始高度，使用原 archive 的 UTM 實作，不額外改 geoid。

`tools/run_matrix.py --binary build/replay3d --jobs 2 --repeat` 跑 8×3×2×2=96 組。每組 `complete.json` 封存 binary/input/output hashes；只有完整且 hash 匹配才 resume。Binary 改變或輸出被改動會拒絕沿用。`--repeat` 另跑同一 binary 並比較每個 deterministic output 檔案的 SHA256，成功才刪除 repeat 目錄。Matrix 的失敗不會被補值；最後會回傳失敗 status 並列出原因。

逐 byte deterministic 只承諾同 image/compiler/binary/architecture/input。跨 compiler、Eigen、CPU 或演算法實作的 comparison 應使用具名 tolerance；不得把「同 binary 重跑一致」等同於「完整研究結論已被重新驗證」。原 archive 的 reference evidence 必須另行比較。

### Archive 與 uniform 數值 protocol

Replay 與 matrix 另提供 `--protocol archive|uniform`，預設 `archive`。Archive 對 Step1–4 使用原 physical reference 的 absolute timestamp substeps 及每次 update 的 eigen prior whitening；TG/C15 使用它們原本的 duration substeps 與 carried square root。這些選擇明確記在每次 `config.json`，不是隱藏調參。

`uniform` 讓全部 formulation 使用 duration substeps 與 carried square root，適合控制其他數值差異後比較 Step3／C15_A 的等價性。兩種 protocol 的 clock 在相同事件時刻對齊，但 epoch 浮點數的子步大小可能略不同；LM 的 diagonal damping 又依 whitening basis 而變。因此不能要求不同 protocol 的完整輸出逐 byte 相同。`--reference-tolerance METRES` 可使 matrix 對 archive horizontal/3D RMSE 差異超過容許值的 cell 回報失敗；未指定則誠實列出差值，仍強制相同 GNSS row counts 與 heading fusion counts。
