# English and Traditional Chinese editions

The public repositories are:

- [kf-toy: English and primary implementation](https://github.com/r04522611/kf-toy)
- [kf-toy-mandarin: Traditional Chinese documentation](https://github.com/r04522611/kf-toy-mandarin)

Both use the `codex/kf-cpp-toy` default branch and originate from shared commit `2dc5edb`. They ship the same C++ implementation, tests, build files, configurations, reference evidence and Python/shell tools. Language-specific content is restricted to `README*` and `docs/`. They are independent repositories, not automatically synchronized forks.

The English repository maintains the algorithms. Add mathematical or numerical fixes there first, then port the same shared-file changes to the Mandarin repository. Keep implementation commits separate from documentation translation commits where practical. The Mandarin repository can fetch the English source through an `upstream` remote and cherry-pick shared-code-only commits. Do not blindly merge an entire edition snapshot over translated documentation.

After synchronizing code, compare both checkouts:

```bash
python3 tools/verify_editions.py --peer /home/pinghua/kf-toy-mandarin
```

The checker compares file sets and SHA256 for all version-controlled assets outside `README*` and `docs/`, including source, tests, tools, vendor notices, configurations and archived metric references. It ignores build/data/results/plan outputs through Git's ignore rules. A nonzero exit identifies missing or differing shared files. This proves identical shared assets, not identical translations or cross-platform numerical output.

The report generator is shared. Without `docs/report-locale.json`, it writes English. The Mandarin repository supplies a text-only JSON locale with matching template keys. Both editions use the same sealed matrix, hashes, RMSE values, thresholds, exit-status policy and 96 numeric rows. Run reports from the same verified matrix to compare translations; `--require-reference` must still fail for the two known 1 mm discrepancies. Neither locale changes estimator inference.

Before publishing synchronized releases:

1. Run the edition checker and the eight Docker CTest suites for shared-code changes.
2. Check local documentation links and corresponding formulas/commands.
3. Keep the 96-run determinism result separate from the 78/80 strict archive tolerance result.
4. Push each repository and verify both remote HEAD commits and public visibility.

Source provenance and original research hashes remain unchanged. Historical commits still contain the earlier bilingual layout; the current English tree is English throughout. Local ignored plans, raw recordings and full experiment outputs are not published in either repository.
