# Source provenance

The standalone 3D formulations and replay protocol are derived from the local research POC in `slam_in_autonomous_driving/src/ch3/research`, as extended during the odometry experiments. Original upstream project: https://github.com/gaoxiang12/slam_in_autonomous_driving ; local reference checkout HEAD `27f8e94798296650f6dcf72d198719e60a92fb04`. Local research files include uncommitted additions and changes; HEAD alone does not identify them. Exact file hashes are recorded in `references/source-manifest.json`.

This repository does not depend on or modify that checkout at build/runtime. The reference is read only. No ROS, g2o, PCL, OpenCV or Sophus code is linked into the new library. Existing mathematical implementations and initialization protocol are adapted with their provenance retained; no new license for third-party content is asserted here.

`vendor/utm/{utm,tranmerc}.{cc,h}` are copied without algorithmic modifications from that checkout's `thirdparty/utm_convert`. Their original US Army Topographic Engineering Center comments, reuse notes, licenses and restriction notices remain in each file. Consult those files for terms.

Raw 10.txt/11.txt road recordings are external inputs, excluded from Git. Their hashes and source locations are captured by conversion manifests. Archived metrics in `references/archived-metrics.json` are verified summaries, not independent truth or new experiments.
