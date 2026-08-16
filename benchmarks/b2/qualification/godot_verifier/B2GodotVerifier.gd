extends Node

const REQUIRED_ACTIONS := ["move_left", "move_right", "move_up", "move_down", "attack"]
const PLAYER_ID := "player"
const ENEMY_ID := "enemy"
const PLAYER_HUD_ID := "hud.player_hp"
const ENEMY_HUD_ID := "hud.enemy_hp"
const VERIFIER_PRIORITY := 100000

var _frame := 0
var _player: Node2D
var _enemy: Node2D
var _player_hud: Range
var _enemy_hud: Range

func _ready() -> void:
    process_physics_priority = VERIFIER_PRIORITY
    for action in REQUIRED_ACTIONS:
        if not InputMap.has_action(action):
            _fail("required semantic action is missing: %s" % action)
            return

    _player = _find_semantic_node(PLAYER_ID) as Node2D
    _enemy = _find_semantic_node(ENEMY_ID) as Node2D
    _player_hud = _find_semantic_node(PLAYER_HUD_ID) as Range
    _enemy_hud = _find_semantic_node(ENEMY_HUD_ID) as Range
    if _player == null or _enemy == null:
        _fail("player/enemy semantic nodes are missing or are not Node2D")
        return
    if _player_hud == null or _enemy_hud == null:
        _fail("player/enemy HUD semantic nodes are missing or are not Range")
        return

    if not _position_is(_player, Vector2(0.0, 0.0)) or not _position_is(_enemy, Vector2(64.0, 0.0)):
        _fail("initial player/enemy position mismatch")
        return
    if not _combat_state_is(_player, 3, 3, false, 0):
        _fail("initial player combat state mismatch")
        return
    if not _combat_state_is(_enemy, 2, 2, false, 0):
        _fail("initial enemy combat state mismatch")
        return
    if not _feedback_state_is(_enemy, 0, 0):
        _fail("initial enemy feedback state mismatch")
        return
    if not _hud_is(_player_hud, 3.0, 3.0) or not _hud_is(_enemy_hud, 2.0, 2.0):
        _fail("initial HUD state mismatch")
        return

    Input.action_press("move_right")

func _physics_process(_delta: float) -> void:
    _frame += 1
    match _frame:
        8:
            Input.action_release("move_right")
            if not _position_is(_player, Vector2(32.0, 0.0)):
                _fail("eight-step movement mismatch")
                return
            if not _combat_state_is(_player, 3, 3, false, 0) or not _combat_state_is(_enemy, 2, 2, false, 0):
                _fail("combat state changed during movement")
                return
            Input.action_press("attack")
        9:
            Input.action_release("attack")
            if not _combat_state_is(_enemy, 1, 2, false, 0):
                _fail("first attack did not deal exactly one damage")
                return
            if not _feedback_state_is(_enemy, 1, 0):
                _fail("first attack feedback mismatch")
                return
            if not _hud_is(_enemy_hud, 1.0, 2.0):
                _fail("enemy HUD did not converge after first attack")
                return
        13:
            Input.action_press("attack")
        14:
            Input.action_release("attack")
            if not _combat_state_is(_enemy, 1, 2, false, 0):
                _fail("frame-14 attack bypassed the frozen six-step cooldown")
                return
            if not _feedback_state_is(_enemy, 1, 0):
                _fail("cooldown-blocked attack emitted feedback")
                return
        15:
            Input.action_press("attack")
        16:
            Input.action_release("attack")
            if not _combat_state_is(_player, 3, 3, false, 0):
                _fail("player HP changed during frozen acceptance sequence")
                return
            if not _combat_state_is(_enemy, 0, 2, true, 1):
                _fail("post-cooldown lethal attack/death transition mismatch")
                return
            if not _feedback_state_is(_enemy, 2, 1):
                _fail("lethal attack feedback mismatch")
                return
            if not _hud_is(_player_hud, 3.0, 3.0) or not _hud_is(_enemy_hud, 0.0, 2.0):
                _fail("final HUD state mismatch")
                return
            print("b2-godot-verifier-pass")
            get_tree().quit(0)

func _find_semantic_node(semantic_id: String) -> Node:
    return _find_semantic_node_recursive(get_tree().root, semantic_id)

func _find_semantic_node_recursive(node: Node, semantic_id: String) -> Node:
    if str(node.get_meta("semantic_id", "")) == semantic_id:
        return node
    for child in node.get_children():
        var found := _find_semantic_node_recursive(child, semantic_id)
        if found != null:
            return found
    return null

func _position_is(node: Node2D, expected: Vector2) -> bool:
    return node.position.is_equal_approx(expected)

func _combat_state_is(node: Node, hp: int, maximum_hp: int, dead: bool, death_transitions: int) -> bool:
    return (
        int(node.get_meta("hp", -999999)) == hp
        and int(node.get_meta("maximum_hp", -999999)) == maximum_hp
        and bool(node.get_meta("dead", not dead)) == dead
        and int(node.get_meta("death_transitions", -999999)) == death_transitions
    )

func _feedback_state_is(node: Node, hit_triggers: int, death_triggers: int) -> bool:
    return (
        int(node.get_meta("hit_particle_triggers", -999999)) == hit_triggers
        and int(node.get_meta("death_particle_triggers", -999999)) == death_triggers
    )

func _hud_is(node: Range, value: float, maximum: float) -> bool:
    return is_equal_approx(node.value, value) and is_equal_approx(node.max_value, maximum)

func _fail(message: String) -> void:
    Input.action_release("move_right")
    Input.action_release("attack")
    push_error("b2-godot-verifier-fail: " + message)
    get_tree().quit(1)
