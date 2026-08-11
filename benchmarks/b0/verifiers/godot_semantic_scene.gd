extends SceneTree

func fail_verification(code: String, message: String) -> void:
    print(JSON.stringify({
        "status": "fail",
        "code": code,
        "message": message,
    }))
    quit(1)

func _init() -> void:
    var packed := load("res://main.tscn") as PackedScene
    if packed == null:
        fail_verification("scene_load_failed", "res://main.tscn did not load as PackedScene")
        return

    var root := packed.instantiate()
    if root == null:
        fail_verification("scene_instantiate_failed", "main scene could not be instantiated")
        return

    var player := root.get_node_or_null("Player") as Node2D
    if player == null:
        root.free()
        fail_verification("player_missing", "expected Node2D named Player")
        return

    if not player.is_in_group("player"):
        root.free()
        fail_verification("semantic_identity_missing", "Player must be in group 'player'")
        return

    if not is_equal_approx(player.position.x, 4.0) or not is_equal_approx(player.position.y, 1.0):
        var observed := player.position
        root.free()
        fail_verification(
            "position_mismatch",
            "expected position (4, 1), observed (%s, %s)" % [observed.x, observed.y]
        )
        return

    print(JSON.stringify({
        "status": "pass",
        "verifier": "godot-semantic-scene-v1",
        "entity_id": "player",
        "name": String(player.name),
        "position_x": player.position.x,
        "position_y": player.position.y,
    }))
    root.free()
    quit(0)
