#pragma once

namespace abdaudiolab::gui::strings
{

// Primary Actions
inline constexpr const char* START_MEASUREMENT      = "▶  START MEASUREMENT";
inline constexpr const char* PAUSE                  = "❚❚  PAUSE";
inline constexpr const char* RESUME                 = "▶  RESUME";
inline constexpr const char* CANCEL                 = "CANCEL";
inline constexpr const char* VIEW_RESULTS_EXPORT    = "✓  VIEW RESULTS / EXPORT";

// Workspace Interaction Modes
inline constexpr const char* MODE_GUIDED            = "Mode: Guided";
inline constexpr const char* MODE_LAB               = "Mode: Lab Bench";
inline constexpr const char* MODE_FREE              = "Mode: Free Capture";

// Ad-hoc / Free Capture Controls
inline constexpr const char* FREE_CAPTURE           = "Capture Free Take";
inline constexpr const char* STOP                   = "Stop";

// Coordinator Lifecycle States
inline constexpr const char* STATE_NO_SESSION       = "No Session";
inline constexpr const char* STATE_PROFILE_SELECTED = "Profile Selected";
inline constexpr const char* STATE_READY            = "Ready";
inline constexpr const char* STATE_AWAITING_OP      = "Awaiting Operator (Space)";
inline constexpr const char* STATE_AUTOMATING       = "Applying Automation";
inline constexpr const char* STATE_CAPTURING        = "Capturing Audio";
inline constexpr const char* STATE_VALIDATING       = "Validating Take";
inline constexpr const char* STATE_PERSISTING       = "Sealing to Disk";
inline constexpr const char* STATE_POINT_COMPLETED  = "Take Completed";
inline constexpr const char* STATE_SESSION_COMPLETED= "Session Completed";
inline constexpr const char* STATE_REANALYSIS_AVAIL = "Reanalysis Available";
inline constexpr const char* STATE_ERROR            = "Error";
inline constexpr const char* STATE_ABORTED          = "Session Cancelled";

// Device & Plugin Categories
inline constexpr const char* BADGE_INSTRUMENT       = " [Instrument]";
inline constexpr const char* BADGE_EFFECT           = " [Effect]";
inline constexpr const char* BTN_KEYBOARD           = "Keyboard";
inline constexpr const char* BTN_VIRTUAL_KEYBOARD   = "Virtual Keyboard";
inline constexpr const char* TOOLTIP_KEYBOARD       = "Virtual MIDI Keyboard - Open interactive on-screen keyboard to play plugins.";

// Tooltips & Action Reasons
inline constexpr const char* TOOLTIP_MODE_TOGGLE    = "Switch between Guided Mode (clean execution view) and Lab Bench Mode (technical curves & test queue)";
inline constexpr const char* TOOLTIP_CONFIRM_MANUAL = "Confirm physical knob position (Spacebar)";
inline constexpr const char* TOOLTIP_START_READY    = "Start automated recipe execution";
inline constexpr const char* TOOLTIP_START_BLOCKED  = "Please configure or select target hardware/plugin with a valid recipe before starting";
inline constexpr const char* TOOLTIP_PAUSE          = "Temporarily pause measurement and silence active notes";
inline constexpr const char* TOOLTIP_RESUME         = "Resume measurement from paused point";
inline constexpr const char* TOOLTIP_CANCEL         = "Cancel current measurement session and silence notes";
inline constexpr const char* TOOLTIP_VIEW_RESULTS   = "View results summary and generate export files";
inline constexpr const char* TOOLTIP_FREE_CAPTURE   = "Trigger immediate ad-hoc audio capture";
inline constexpr const char* TOOLTIP_FREE_STOP      = "Stop current recording take";

} // namespace abdaudiolab::gui::strings
