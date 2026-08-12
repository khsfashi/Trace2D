extends SceneTree

var _task_id := ""

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 2 or args[0] != "--task":
		_fail("usage: -- --task <task-id>")
		return
	_task_id = args[1]
	call_deferred("_run")

func _run() -> void:
	var packed := load("res://main.tscn") as PackedScene
	if packed == null:
		_fail("failed to load res://main.tscn")
		return
	var root := packed.instantiate()
	if root == null:
		_fail("failed to instantiate res://main.tscn")
		return
	get_root().add_child(root)

	var accepted := false
	match _task_id:
		"b1-sprite-normalize-repair":
			accepted = _verify_sprite(root)
		"b1-animation-exact-event":
			accepted = _verify_animation(root)
		"b1-particle-budget-repair":
			accepted = _verify_particle(root)
		_:
			_fail("unknown task id: %s" % _task_id)
			return

	print("B1_FIXTURE_QUALIFICATION task=%s result=%s" % [_task_id, "accepted" if accepted else "rejected"])
	root.queue_free()
	quit(0 if accepted else 1)

func _verify_sprite(root: Node) -> bool:
	var hero := root.get_node_or_null("Hero") as Sprite2D
	if hero == null:
		return false
	return hero.region_enabled \
		and hero.region_rect == Rect2(2, 1, 12, 14) \
		and hero.offset == Vector2(0, 0) \
		and hero.texture_filter == CanvasItem.TEXTURE_FILTER_NEAREST

func _verify_animation(root: Node) -> bool:
	var player := root.get_node_or_null("B1Animation") as AnimationPlayer
	if player == null or not player.has_animation(&"attack"):
		return false
	var animation := player.get_animation(&"attack")
	if animation == null or not is_equal_approx(animation.length, 0.5):
		return false

	var frame_track := -1
	var method_track := -1
	for track in animation.get_track_count():
		if animation.track_get_type(track) == Animation.TYPE_VALUE:
			frame_track = track
		elif animation.track_get_type(track) == Animation.TYPE_METHOD:
			method_track = track
	if frame_track < 0 or method_track < 0:
		return false
	if animation.track_get_path(frame_track) != NodePath("Subject:frame"):
		return false
	if animation.track_get_key_count(frame_track) != 3:
		return false
	var expected_times := [0.0, 0.1, 0.25]
	var expected_frames := [0, 1, 2]
	for index in 3:
		if not is_equal_approx(animation.track_get_key_time(frame_track, index), expected_times[index]):
			return false
		if int(animation.track_get_key_value(frame_track, index)) != expected_frames[index]:
			return false
	if animation.track_get_key_count(method_track) != 1:
		return false
	if not is_equal_approx(animation.track_get_key_time(method_track, 0), 0.25):
		return false
	var event_value = animation.track_get_key_value(method_track, 0)
	if typeof(event_value) != TYPE_DICTIONARY:
		return false
	return event_value.has("method") and StringName(event_value["method"]) == &"queue_redraw"

func _verify_particle(root: Node) -> bool:
	var particles := root.get_node_or_null("HitSpark") as GPUParticles2D
	if particles == null:
		return false
	return particles.amount <= 64 \
		and is_equal_approx(particles.lifetime, 0.8) \
		and not particles.emitting \
		and particles.one_shot

func _fail(message: String) -> void:
	push_error(message)
	quit(2)
