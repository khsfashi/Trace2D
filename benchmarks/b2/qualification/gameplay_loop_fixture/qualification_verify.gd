extends SceneTree

const EXPECTED_START_X := 32.0
const EXPECTED_STEP_UNITS := 2.0
const EXPECTED_TICKS := 8

func _init() -> void:
    var packed := load("res://main.tscn") as PackedScene
    if packed == null:
        _fail("main scene did not load")
        return

    var scene := packed.instantiate()
    root.add_child(scene)
    var probe := scene.get_node_or_null("Probe") as Node2D
    var status := scene.get_node_or_null("Status") as Label
    if probe == null or status == null:
        _fail("qualification nodes are missing")
        return
    if not probe.is_in_group("qualification_probe"):
        _fail("qualification semantic group is missing")
        return
    if not is_equal_approx(probe.position.x, EXPECTED_START_X):
        _fail("unexpected initial probe position")
        return
    if not probe.has_method("qualification_simulate"):
        _fail("qualification simulation entrypoint is missing")
        return

    for _index in range(EXPECTED_TICKS):
        probe.call("qualification_simulate", true)

    var expected_x := EXPECTED_START_X + EXPECTED_STEP_UNITS * EXPECTED_TICKS
    if not is_equal_approx(probe.position.x, expected_x):
        _fail("deterministic movement mismatch: expected %.1f, got %.1f" % [expected_x, probe.position.x])
        return
    if int(probe.get("active_ticks")) != EXPECTED_TICKS:
        _fail("deterministic active-tick count mismatch")
        return
    if not status.text.contains("ticks=8"):
        _fail("HUD did not reflect deterministic state")
        return

    print("qualification-independent-verifier-pass")
    quit(0)

func _fail(message: String) -> void:
    push_error("qualification-independent-verifier-fail: " + message)
    quit(1)
