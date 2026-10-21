# Deterministic replay 與封存數值重現是兩項驗證

最終程式的8組CTest通過。完整matrix每個cell在同binary／image／輸入／protocol下重跑一次，比較所有結果檔案的bytes。另以80個歷史cell核對direct GNSS antenna residual RMSE；C15 GN／LM的16個新cell沒有舊baseline。

最初宣告的archive嚴格容差為水平／3D RMSE差1mm。兩個11 pure DR cell超出：

| Cell | 新水平RMSE m | 封存水平RMSE m | 差異 m |
|---|---:|---:|---:|
| Step3 GN | 188.1338323648597 | 188.1350582074412 | -0.0012258425815 |
| Step4 LM | 111.12085871585587 | 111.11978187916851 | +0.0010768366874 |

**這兩項1mm檢查未通過。** 沒有修改filter參數或放寬容差把它們改成通過；同binary逐byte重跑是另一個已驗證條件。

## 唯讀診斷

兩位獨立review比對全部18,469筆solver事件與17,624筆states：

- Noise、median dt、first／second fix時刻、全部18,415列GNSS的timestamp／fusion／validity／initialization masks一致。
- 第一筆初始化physical state最大差約5.55e-17，bg／ba一致。
- Step3 GN全程沒有iteration count、status或rejected-trial差異。Position差異由second fix約2.34e-10m逐步累積，最大約2.91mm；因此不能把它全部歸給solver停止條件。
- Step4 LM第一個trial-rejection差異在update729（約73s），當時皆3iterations／step，final cost差約7e-13；首次status差異在update1556（約155s），step對stalled。最大state position差約2.97mm。
- Second-fix north residual差2.31684e-10m，約UTM northing半個ULP。原UTM wrapper使用lat*(pi/180)，新runner使用lat*pi/180，實數公式相同但浮點運算次序不同。這是一個可能擾動來源。

其他等價但數值路徑不同的因素包含：原Sophus quaternion exponential與Rodrigues matrix exponential、18D帶零gravity方向的eigen whitening與15D whitening、dense covariance與QR roots。現有證據支持小擾動經長程feedback放大，但沒有唯一隔離每個原因；不宣稱已證明單一根因。

## 如何讀取工具結果

`tools/run_matrix.py --reference-tolerance 0.001` 在這兩cell上回報失敗，即使execution與byte repeat成功。原始嚴格檢查結果會保留為 `results/research/matrix.strict-1mm.json`。

不帶該選項的matrix仍列出每個reference差異；`tools/report_validation.py` 永遠按1mm列出strict pass／failure清單，預設針對execution／byte determinism決定exit status。加 `--require-reference` 會令上述兩差異造成nonzero exit。這不是放寬數值門檻，而是避免把不同驗證概念混成單一綠燈。

預設一鍵reproduce產生完整report，讀者可自行要求strict numeric gate。`plan/`原研究表不被改寫，新的實驗報告保留自己的數值與差異。
