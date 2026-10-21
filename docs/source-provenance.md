# 原始來源與唯讀邊界

完整來源說明見 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。原研究檔案的精確SHA256保存在 `references/source-manifest.json`，已核對封存GNSS結果的摘要在 `references/archived-metrics.json`。

新repository位於 `/home/pinghua/kf-toy`；原 `/home/pinghua/slam_in_autonomous_driving` 僅唯讀。核心公式適配為獨立Eigen實作，沒有連結ROS／Sophus；UTM原始檔保留其reuse/license/restriction註記。

資料以唯讀Docker mount轉換，CSV保留原始token與順序。完整source recording hashes在各資料manifest。研究結果的GNSS proxy與真值教學metrics分開呈現，不將多方法replay當成多份獨立道路資料。
