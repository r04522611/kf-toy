# Standalone KF validation

[繁體中文](validation.zh-TW.md) | [English](validation.en.md)

- Execution/determinism: **96/96 passed**; each run byte-identical on repeat.
- Archived references: **80 cells**, maximum horizontal/3D RMSE discrepancy **0.00122629024 m** (declared tolerance 0.001 m).
- New C15 iterative cases: **16**, no invented archived baseline.
- Maximum runner clock error: **0 s**. No post-initialization GNSS position fusion.

**Strict archive tolerance:** 78/80 within 0.001 m; 2 outside. This gate is separate from deterministic execution and is not reported as fully passed.

- 11 / pure / step3 gn: horizontal delta -0.00122584258 m, 3D delta -0.00122629024 m.
- 11 / pure / step4 lm: horizontal delta 0.00107683669 m, 3D delta 0.00107708514 m.

Road scores are direct GNSS antenna residual proxies, not independent SE3 truth. Bitwise repeat applies to this image/binary/input/protocol, not arbitrary platforms. Archive compatibility and uniform numerical policies are recorded per run.

All 96 cells follow. Mode, recipe and backend identifiers match the source CSV. "new" means no archived reference, not zero error.

|Mode|Data|Method|Horizontal RMSE (m)|3D RMSE (m)|Archive delta horizontal (m)|
|---|---|---|---:|---:|---:|
|heading|10|c15a gn|5.100947|5.277635|new|
|heading|10|c15a kf|5.100989|5.279748|-1.4573498e-10|
|heading|10|c15a lm|5.100941|5.277629|new|
|heading|10|c15b gn|5.100947|5.277635|new|
|heading|10|c15b kf|5.089173|5.236896|-1.22390986e-10|
|heading|10|c15b lm|5.100941|5.277629|new|
|heading|10|step1 gn|5.122144|5.359570|-3.53525653e-09|
|heading|10|step1 kf|5.122174|5.364098|-1.35675471e-10|
|heading|10|step1 lm|5.122139|5.359567|5.79044581e-08|
|heading|10|step2 gn|5.122101|5.357455|-1.0361739e-08|
|heading|10|step2 kf|5.122125|5.361960|-1.34987133e-10|
|heading|10|step2 lm|5.122097|5.357452|4.45724231e-08|
|heading|10|step3 gn|5.100947|5.277635|1.48016062e-08|
|heading|10|step3 kf|5.100989|5.279748|-5.76283021e-10|
|heading|10|step3 lm|5.100942|5.277631|6.3205003e-08|
|heading|10|step4 gn|5.108915|5.288472|7.36450811e-09|
|heading|10|step4 kf|5.108947|5.290590|-5.67345282e-10|
|heading|10|step4 lm|5.108911|5.288467|1.00687556e-08|
|heading|10|step5a gn|5.456721|7.926150|1.0709587e-08|
|heading|10|step5a kf|5.456958|7.921163|-1.36598288e-10|
|heading|10|step5a lm|5.456714|7.926146|1.03911253e-08|
|heading|10|step5b gn|5.456721|7.926150|1.0709587e-08|
|heading|10|step5b kf|5.443621|8.003829|-1.49312562e-10|
|heading|10|step5b lm|5.456714|7.926146|1.03911253e-08|
|heading|11|c15a gn|19.481841|19.565613|new|
|heading|11|c15a kf|19.481400|19.565428|-1.42598822e-10|
|heading|11|c15a lm|19.481841|19.565613|new|
|heading|11|c15b gn|19.481841|19.565613|new|
|heading|11|c15b kf|19.504391|19.581470|-1.06712861e-10|
|heading|11|c15b lm|19.481841|19.565613|new|
|heading|11|step1 gn|19.452669|19.543395|-2.15157314e-09|
|heading|11|step1 kf|19.451699|19.542965|-1.15658594e-10|
|heading|11|step1 lm|19.452669|19.543395|-3.16694546e-08|
|heading|11|step2 gn|19.452902|19.542795|-2.43853293e-09|
|heading|11|step2 kf|19.451943|19.542372|-1.25659483e-10|
|heading|11|step2 lm|19.452902|19.542795|5.58526736e-09|
|heading|11|step3 gn|19.481841|19.565613|-1.43778699e-07|
|heading|11|step3 kf|19.481400|19.565428|-4.53034943e-10|
|heading|11|step3 lm|19.481841|19.565613|1.28668134e-07|
|heading|11|step4 gn|19.481365|19.566941|8.88692888e-08|
|heading|11|step4 kf|19.480902|19.566738|-4.434213e-10|
|heading|11|step4 lm|19.481365|19.566941|8.75776784e-08|
|heading|11|step5a gn|19.759095|19.801853|-1.46976333e-08|
|heading|11|step5a kf|19.758620|19.801227|-1.18344445e-10|
|heading|11|step5a lm|19.759095|19.801854|-4.07956939e-08|
|heading|11|step5b gn|19.759095|19.801853|-1.46976333e-08|
|heading|11|step5b kf|19.783509|19.830374|-1.22895472e-10|
|heading|11|step5b lm|19.759095|19.801854|-4.07956939e-08|
|pure|10|c15a gn|200.169685|200.174402|new|
|pure|10|c15a kf|200.586004|200.590773|1.71866077e-10|
|pure|10|c15a lm|200.170106|200.174823|new|
|pure|10|c15b gn|200.169685|200.174402|new|
|pure|10|c15b kf|155.229727|155.234511|1.47934998e-10|
|pure|10|c15b lm|200.170106|200.174823|new|
|pure|10|step1 gn|68.130052|68.150041|7.78358915e-05|
|pure|10|step1 kf|70.839583|70.859171|-1.15321086e-10|
|pure|10|step1 lm|68.130870|68.150859|-8.83729108e-05|
|pure|10|step2 gn|69.222544|69.242081|8.86456714e-05|
|pure|10|step2 kf|71.932990|71.952147|-4.67821337e-11|
|pure|10|step2 lm|69.223399|69.242935|-7.39293937e-05|
|pure|10|step3 gn|200.169613|200.174330|5.60552998e-05|
|pure|10|step3 kf|200.586004|200.590773|4.17344381e-10|
|pure|10|step3 lm|200.170116|200.174833|0.000189053912|
|pure|10|step4 gn|111.840831|111.850810|7.3402965e-05|
|pure|10|step4 kf|113.259022|113.268991|7.84481813e-10|
|pure|10|step4 lm|111.840732|111.850712|-3.56612321e-06|
|pure|10|step5a gn|112.166520|112.576591|-4.24536211e-08|
|pure|10|step5a kf|113.601913|114.001954|2.8563818e-12|
|pure|10|step5a lm|112.166512|112.576582|1.42998149e-06|
|pure|10|step5b gn|112.166520|112.576591|-4.24536211e-08|
|pure|10|step5b kf|53.976194|54.342335|-6.23856522e-12|
|pure|10|step5b lm|112.166512|112.576582|1.42998149e-06|
|pure|11|c15a gn|188.134286|188.144410|new|
|pure|11|c15a kf|192.003660|192.013618|4.71575419e-07|
|pure|11|c15a lm|188.135092|188.145216|new|
|pure|11|c15b gn|188.134286|188.144410|new|
|pure|11|c15b kf|66.123583|66.147838|3.44522562e-08|
|pure|11|c15b lm|188.135092|188.145216|new|
|pure|11|step1 gn|90.024088|90.045419|-8.47536748e-05|
|pure|11|step1 kf|84.332136|84.355063|-5.19264631e-11|
|pure|11|step1 lm|90.024297|90.045628|0.000313838947|
|pure|11|step2 gn|85.623755|85.646020|-0.000203197339|
|pure|11|step2 kf|79.941775|79.965788|3.12894599e-10|
|pure|11|step2 lm|85.623977|85.646242|0.000191535449|
|pure|11|step3 gn|188.133832|188.143956|-0.00122584258|
|pure|11|step3 kf|192.003660|192.013618|4.59029934e-08|
|pure|11|step3 lm|188.134887|188.145011|-0.000260536394|
|pure|11|step4 gn|111.120377|111.136362|0.000880726656|
|pure|11|step4 kf|114.976802|114.992311|3.74206039e-07|
|pure|11|step4 lm|111.120859|111.136845|0.00107683669|
|pure|11|step5a gn|107.924301|107.931537|-2.5721647e-11|
|pure|11|step5a kf|111.683593|111.690653|-1.16600063e-10|
|pure|11|step5a lm|107.924548|107.931784|5.67105104e-07|
|pure|11|step5b gn|107.924301|107.931537|-2.5721647e-11|
|pure|11|step5b kf|150.860870|150.862853|7.81597009e-12|
|pure|11|step5b lm|107.924548|107.931784|5.67105104e-07|
