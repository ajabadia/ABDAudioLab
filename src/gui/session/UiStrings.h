#pragma once

namespace abdaudiolab::gui::strings
{

// Primary Actions
inline constexpr const char* START_MEASUREMENT      = "START MEASUREMENT";
inline constexpr const char* PAUSE                  = "PAUSE";
inline constexpr const char* RESUME                 = "RESUME";
inline constexpr const char* CANCEL                 = "CANCEL";
inline constexpr const char* VIEW_RESULTS_EXPORT    = "VIEW RESULTS / EXPORT";

// Workspace Interaction Modes
// Los modos se explican como niveles de edición, no como dos productos separados.
// Según UX: "un flujo con automatización visible que ya existe, y un modo con distinto nivel de edición".
inline constexpr const char* MODE_GUIDED            = "Guided: less editing, more preset";
inline constexpr const char* MODE_LAB               = "Classic: more control, advanced setup";
inline constexpr const char* MODE_FREE              = "Free: no restrictions";

// Sprint 3 additions: Mode strings and tooltips for ProfileLoader validation
inline constexpr const char* MODE_CLASSIC           = "Classic";
inline constexpr const char* TOOLTIP_MODE_GUIDED      = "Modo Guiado: menos edición, más preset";
inline constexpr const char* TOOLTIP_MODE_CLASSIC     = "Modo Clásico: más control, setup avanzado";
inline constexpr const char* TOOLTIP_MODE_FREE         = "Modo Libre: acceso total a parámetros";

// Ad-hoc / Free Capture Controls
inline constexpr const char* FREE_CAPTURE           = "Capture Free Take";
inline constexpr const char* STOP                   = "Stop";
inline constexpr const char* PROMOTE_TO_RECIPE       = "Promote to Recipe";

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
// Según UX: explicar en términos de "qué necesita el usuario", no "qué modo usas"
// UX_REVISION_NIVEL_PRACTICO.md: "Modo actual: automatiza lo que ya puedes definir tú"
//                          "Modo guiado: menos edición, más cosas prefijadas."
//                          "Modo clásico: más control, requiere configuración avanzada."
inline constexpr const char* TOOLTIP_MODE_TOGGLE    = "Guided: less editing, more preset. Classic: more control, advanced setup.";
inline constexpr const char* TOOLTIP_CONFIRM_MANUAL = "Confirm physical knob position (Spacebar)";
inline constexpr const char* TOOLTIP_START_READY    = "Start automated recipe execution";
inline constexpr const char* TOOLTIP_START_BLOCKED  = "Please configure or select target hardware/plugin with a valid recipe before starting";
inline constexpr const char* TOOLTIP_PAUSE          = "Temporarily pause measurement and silence active notes";
inline constexpr const char* TOOLTIP_RESUME         = "Resume measurement from paused point";
inline constexpr const char* TOOLTIP_CANCEL         = "Cancel current measurement session and silence notes";
inline constexpr const char* TOOLTIP_VIEW_RESULTS   = "View results summary and generate export files";
inline constexpr const char* TOOLTIP_FREE_CAPTURE   = "Trigger immediate ad-hoc audio capture";
inline constexpr const char* TOOLTIP_FREE_STOP      = "Stop current recording take";
inline constexpr const char* TOOLTIP_PROMOTE_TO_RECIPE = "Promote current exploration into a reproducible MeasurementRecipe";

} // namespace abdaudiolab::gui::strings
