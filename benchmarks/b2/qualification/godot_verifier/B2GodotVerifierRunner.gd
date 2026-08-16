extends SceneTree

func _init() -> void:
    var packed := load("res://main.tscn") as PackedScene
    if packed == null:
        push_error("b2-godot-verifier-fail: main scene did not load")
        quit(1)
        return
    var candidate := packed.instantiate()
    candidate.name = "Candidate"
    root.add_child(candidate)

    var verifier_script := load("res://__trace2d_b2_verifier.gd") as Script
    if verifier_script == null:
        push_error("b2-godot-verifier-fail: verifier script did not load")
        quit(1)
        return
    var verifier := Node.new()
    verifier.name = "Trace2DB2Verifier"
    verifier.set_script(verifier_script)
    root.add_child(verifier)
