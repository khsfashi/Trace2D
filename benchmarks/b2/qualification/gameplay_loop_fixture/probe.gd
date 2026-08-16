extends Node2D

const STEP_UNITS := 2.0
const REPORT_TICKS := 8

var active_ticks := 0

func _ready() -> void:
    _refresh_status()
    print("qualification-ready")

func _physics_process(_delta: float) -> void:
    if InputMap.has_action("qualification_move"):
        qualification_simulate(Input.is_action_pressed("qualification_move"))

func qualification_simulate(input_active: bool) -> void:
    if not input_active:
        return
    position.x += STEP_UNITS
    active_ticks += 1
    _refresh_status()
    if active_ticks == REPORT_TICKS:
        print("qualification-input-8")

func _mcp_state() -> Dictionary:
    return {
        "semantic_id": "qualification_probe",
        "position_x": position.x,
        "active_ticks": active_ticks,
    }

func _refresh_status() -> void:
    var status := get_node_or_null("../Status") as Label
    if status != null:
        status.text = "qualification  ticks=%d  x=%.0f" % [active_ticks, position.x]
