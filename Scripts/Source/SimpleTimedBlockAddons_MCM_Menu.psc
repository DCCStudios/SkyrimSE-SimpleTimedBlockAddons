Scriptname SimpleTimedBlockAddons_MCM_Menu extends SKI_ConfigBase

; ===== MCM Properties =====
int OID_EnableSlowdown
int OID_SlowdownAmount
int OID_SlowdownDuration
int OID_EnableSound
int OID_SoundPath

int OID_BlockKey
int OID_StaggerDistance
int OID_PerkLockedBlock
int OID_OnlyWithShield
int OID_PerkLockedStagger

; ===== Cache =====
bool _enableSlowdown
float _slowdownAmount
float _slowdownDuration
bool _enableSound
string _soundPath

string _blockKey
float _staggerDistance
bool _perkLockedBlock
bool _onlyWithShield
bool _perkLockedStagger

; ===== Events =====

Event OnConfigInit()
    ModName = "Simple Timed Block"
    Pages = new string[2]
    Pages[0] = "General"
    Pages[1] = "Addons"
EndEvent

Event OnPageReset(string page)
    SetCursorFillMode(TOP_TO_BOTTOM)
    
    If page == "General"
        ShowGeneralPage()
    ElseIf page == "Addons"
        ShowAddonsPage()
    EndIf
EndEvent

Function ShowGeneralPage()
    ; Load current values
    _blockKey = SimpleTimedBlockAddons_MCM.GetBlockKey()
    _staggerDistance = SimpleTimedBlockAddons_MCM.GetStaggerDistance()
    _perkLockedBlock = SimpleTimedBlockAddons_MCM.GetPerkLockedBlock()
    _onlyWithShield = SimpleTimedBlockAddons_MCM.GetOnlyWithShield()
    _perkLockedStagger = SimpleTimedBlockAddons_MCM.GetPerkLockedStagger()
    
    AddHeaderOption("Block Settings")
    OID_BlockKey = AddInputOption("Block Key", _blockKey)
    OID_StaggerDistance = AddSliderOption("Stagger Distance", _staggerDistance, "{0}")
    
    AddEmptyOption()
    AddHeaderOption("Perk Requirements")
    OID_PerkLockedBlock = AddToggleOption("Perk Required for Block", _perkLockedBlock)
    OID_OnlyWithShield = AddToggleOption("Shield Only", _onlyWithShield)
    OID_PerkLockedStagger = AddToggleOption("Perk Required for Stagger", _perkLockedStagger)
EndFunction

Function ShowAddonsPage()
    ; Load current values
    _enableSlowdown = SimpleTimedBlockAddons_MCM.GetEnableSlowdown()
    _slowdownAmount = SimpleTimedBlockAddons_MCM.GetSlowdownAmount()
    _slowdownDuration = SimpleTimedBlockAddons_MCM.GetSlowdownDuration()
    _enableSound = SimpleTimedBlockAddons_MCM.GetEnableSound()
    _soundPath = SimpleTimedBlockAddons_MCM.GetSoundPath()
    
    AddHeaderOption("Slowdown Effect")
    OID_EnableSlowdown = AddToggleOption("Enable Slowdown", _enableSlowdown)
    OID_SlowdownAmount = AddSliderOption("Slowdown Amount (%)", _slowdownAmount * 100, "{0}%")
    OID_SlowdownDuration = AddSliderOption("Slowdown Duration (s)", _slowdownDuration, "{1}s")
    
    AddEmptyOption()
    AddHeaderOption("Sound Effect")
    OID_EnableSound = AddToggleOption("Enable Sound", _enableSound)
    OID_SoundPath = AddInputOption("Sound Path/EditorID", _soundPath)
EndFunction

; ===== Option Events =====

Event OnOptionSelect(int option)
    If option == OID_EnableSlowdown
        _enableSlowdown = !_enableSlowdown
        SimpleTimedBlockAddons_MCM.SetEnableSlowdown(_enableSlowdown)
        SetToggleOptionValue(option, _enableSlowdown)
    ElseIf option == OID_EnableSound
        _enableSound = !_enableSound
        SimpleTimedBlockAddons_MCM.SetEnableSound(_enableSound)
        SetToggleOptionValue(option, _enableSound)
    ElseIf option == OID_PerkLockedBlock
        _perkLockedBlock = !_perkLockedBlock
        SimpleTimedBlockAddons_MCM.SetPerkLockedBlock(_perkLockedBlock)
        SetToggleOptionValue(option, _perkLockedBlock)
    ElseIf option == OID_OnlyWithShield
        _onlyWithShield = !_onlyWithShield
        SimpleTimedBlockAddons_MCM.SetOnlyWithShield(_onlyWithShield)
        SetToggleOptionValue(option, _onlyWithShield)
    ElseIf option == OID_PerkLockedStagger
        _perkLockedStagger = !_perkLockedStagger
        SimpleTimedBlockAddons_MCM.SetPerkLockedStagger(_perkLockedStagger)
        SetToggleOptionValue(option, _perkLockedStagger)
    EndIf
    
    ; Auto-save on change
    SimpleTimedBlockAddons_MCM.SaveSettings()
EndEvent

Event OnOptionSliderOpen(int option)
    If option == OID_SlowdownAmount
        SetSliderDialogStartValue(_slowdownAmount * 100)
        SetSliderDialogDefaultValue(25)
        SetSliderDialogRange(1, 100)
        SetSliderDialogInterval(1)
    ElseIf option == OID_SlowdownDuration
        SetSliderDialogStartValue(_slowdownDuration)
        SetSliderDialogDefaultValue(0.5)
        SetSliderDialogRange(0.1, 5.0)
        SetSliderDialogInterval(0.1)
    ElseIf option == OID_StaggerDistance
        SetSliderDialogStartValue(_staggerDistance)
        SetSliderDialogDefaultValue(128)
        SetSliderDialogRange(0, 500)
        SetSliderDialogInterval(8)
    EndIf
EndEvent

Event OnOptionSliderAccept(int option, float value)
    If option == OID_SlowdownAmount
        _slowdownAmount = value / 100.0
        SimpleTimedBlockAddons_MCM.SetSlowdownAmount(_slowdownAmount)
        SetSliderOptionValue(option, value, "{0}%")
    ElseIf option == OID_SlowdownDuration
        _slowdownDuration = value
        SimpleTimedBlockAddons_MCM.SetSlowdownDuration(_slowdownDuration)
        SetSliderOptionValue(option, value, "{1}s")
    ElseIf option == OID_StaggerDistance
        _staggerDistance = value
        SimpleTimedBlockAddons_MCM.SetStaggerDistance(_staggerDistance)
        SetSliderOptionValue(option, value, "{0}")
    EndIf
    
    ; Auto-save on change
    SimpleTimedBlockAddons_MCM.SaveSettings()
EndEvent

Event OnOptionInputOpen(int option)
    If option == OID_BlockKey
        SetInputDialogStartText(_blockKey)
    ElseIf option == OID_SoundPath
        SetInputDialogStartText(_soundPath)
    EndIf
EndEvent

Event OnOptionInputAccept(int option, string value)
    If option == OID_BlockKey
        _blockKey = value
        SimpleTimedBlockAddons_MCM.SetBlockKey(_blockKey)
        SetInputOptionValue(option, _blockKey)
    ElseIf option == OID_SoundPath
        _soundPath = value
        SimpleTimedBlockAddons_MCM.SetSoundPath(_soundPath)
        SetInputOptionValue(option, _soundPath)
    EndIf
    
    ; Auto-save on change
    SimpleTimedBlockAddons_MCM.SaveSettings()
EndEvent

Event OnOptionHighlight(int option)
    If option == OID_EnableSlowdown
        SetInfoText("Enable time slowdown effect when performing a successful timed block.")
    ElseIf option == OID_SlowdownAmount
        SetInfoText("How slow time becomes during the effect. 25% means time runs at 1/4 speed.")
    ElseIf option == OID_SlowdownDuration
        SetInfoText("How long the slowdown effect lasts in seconds.")
    ElseIf option == OID_EnableSound
        SetInfoText("Play a sound effect when performing a successful timed block.")
    ElseIf option == OID_SoundPath
        SetInfoText("Sound descriptor EditorID or path. Use EditorIDs from Sound Descriptors for best results.")
    ElseIf option == OID_BlockKey
        SetInfoText("The key to use for blocking. Use single letter (e.g. V) or key name.")
    ElseIf option == OID_StaggerDistance
        SetInfoText("Distance in units for the stagger effect radius.")
    ElseIf option == OID_PerkLockedBlock
        SetInfoText("Require a specific perk to perform timed blocks.")
    ElseIf option == OID_OnlyWithShield
        SetInfoText("Only allow timed blocks when holding a shield.")
    ElseIf option == OID_PerkLockedStagger
        SetInfoText("Require a specific perk to apply the stagger effect.")
    EndIf
EndEvent














