# Original sources and read-only boundaries

[Traditional Chinese version](https://github.com/r04522611/kf-toy-mandarin/blob/codex/kf-cpp-toy/docs/source-provenance.md)

See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) for source notices. Exact SHA256 values for research source files are recorded in `references/source-manifest.json`; verified archived GNSS metric summaries are in `references/archived-metrics.json`.

The independent repository is `/home/pinghua/kf-toy`; `/home/pinghua/slam_in_autonomous_driving` is read-only reference material. Core equations were adapted into a standalone Eigen implementation without ROS or Sophus linkage. The original UTM files retain their reuse, license and restriction notices.

Data conversion uses a read-only Docker mount. CSV preserves source numeric tokens and event order. Each data manifest records complete source-recording hashes. Road-data GNSS proxies and teaching metrics with known truth are presented separately; replaying multiple methods does not create independent road recordings.
