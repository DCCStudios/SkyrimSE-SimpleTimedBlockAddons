Scriptname SimpleTimedBlockAddons_MCM Hidden

; ===== Addon Settings Getters =====
bool Function GetEnableSlowdown() Global Native
float Function GetSlowdownAmount() Global Native
float Function GetSlowdownDuration() Global Native
bool Function GetEnableSound() Global Native
string Function GetSoundPath() Global Native

; ===== Original Settings Getters =====
string Function GetBlockKey() Global Native
float Function GetStaggerDistance() Global Native
bool Function GetPerkLockedBlock() Global Native
bool Function GetOnlyWithShield() Global Native
bool Function GetPerkLockedStagger() Global Native

; ===== Addon Settings Setters =====
Function SetEnableSlowdown(bool value) Global Native
Function SetSlowdownAmount(float value) Global Native
Function SetSlowdownDuration(float value) Global Native
Function SetEnableSound(bool value) Global Native
Function SetSoundPath(string value) Global Native

; ===== Original Settings Setters =====
Function SetBlockKey(string value) Global Native
Function SetStaggerDistance(float value) Global Native
Function SetPerkLockedBlock(bool value) Global Native
Function SetOnlyWithShield(bool value) Global Native
Function SetPerkLockedStagger(bool value) Global Native

; ===== Settings Management =====
Function SaveSettings() Global Native
Function ReloadSettings() Global Native


