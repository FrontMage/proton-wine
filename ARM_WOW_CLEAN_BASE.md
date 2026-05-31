# ARM WoW Clean Base

Baseline commit: `1436d083c11a6753229ddad701ef97e089b39ec9`

Scope: last committed ARM WoW/native ARM exception-probe clean base before later local witness/attribution edits.

Intended RP6 pairing for the known `Game Initialization Failed: I` clean-base line:

- `FEX = fexcore-2508-baseline-ba7a4506.tzst`
- `wine-runtime asset hash observed on RP6 clean-base line = fa9cfadd34e52a658f692e492dd3df71dd31823f007b3d4a1b45c8ff7ba661cc`
- `prefix-template asset hash observed on RP6 clean-base line = cd139616362774f324dc5b73e6e19f8230d21374298b63a92a909fffbb4a9973`

Explicit caveat: the `fa9c/cd13` runtime assets were not traced to a preserved clean remote-build artifact and may include local dirty-tree state after `1436d083`; this fork publishes the committed proton source baseline only.
