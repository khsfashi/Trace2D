extends Node2D

const SPEED := 10.0
var physics_ticks := 0
var input_ticks := 0

func _physics_process(delta: float) -> void:
    physics_ticks += 1
    if Input.is_key_pressed(KEY_D):
        input_ticks += 1
        position.x += SPEED * delta

func _mcp_state() -> Dictionary:
    return {
        "semantic_id": "player",
        "position_x": position.x,
        "position_y": position.y,
        "physics_ticks": physics_ticks,
        "input_ticks": input_ticks,
    }
