# Coordinate projection provenance

The four `utm`/`tranmerc` source/header files were copied byte-for-byte from
`slam_in_autonomous_driving/thirdparty/utm_convert` to preserve the exact UTM
projection used by the reference replays. Their original header notices are
retained. `utm.cc` identifies the U.S. Army Topographic Engineering Center origin
and states that no licenses or restrictions apply to that component. See the
individual files for their full original notices. This statement does not assign
any license to other copied or adapted project code.

Projection defaults, zone selection and arithmetic remain unchanged. Input
latitude/longitude are converted from degrees to radians exactly once. Altitude
is retained directly; no geoid conversion is introduced.
