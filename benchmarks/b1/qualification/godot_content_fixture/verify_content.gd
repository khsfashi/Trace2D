extends SceneTree

const ANIMATION_NAME := "content_probe"
const EXPECTED_EVENT_TIME := 0.375
const EXPECTED_PARTICLE_AMOUNT := 96
const EXPECTED_PARTICLE_LIFETIME := 0.8

func fail(message: String) -> void:
	push_error("B1 content verifier: " + message)
	quit(1)

func _initialize() -> void:
	var packed := load("res://main.tscn") as PackedScene
	if packed == null:
		fail("main.tscn did not load as PackedScene")
		return

	var root := packed.instantiate()
	if root == null:
		fail("main.tscn did not instantiate")
		return

	var player := root.get_node_or_null("B1Animation") as AnimationPlayer
	if player == null:
		root.free()
		fail("B1Animation AnimationPlayer missing")
		return
	if not player.has_animation(ANIMATION_NAME):
		root.free()
		fail("content_probe animation missing")
		return

	var animation := player.get_animation(ANIMATION_NAME)
	if animation == null:
		root.free()
		fail("content_probe animation could not be read")
		return
	if absf(animation.length - 1.0) > 0.0000001:
		root.free()
		fail("animation length must be exactly 1.0s")
		return

	var event_found := false
	for track_index in range(animation.get_track_count()):
		if animation.track_get_type(track_index) != Animation.TYPE_METHOD:
			continue
		if String(animation.track_get_path(track_index)) != "Subject":
			continue
		if animation.track_get_key_count(track_index) != 1:
			continue
		if absf(animation.track_get_key_time(track_index, 0) - EXPECTED_EVENT_TIME) > 0.0000001:
			continue
		if String(animation.method_track_get_name(track_index, 0)) != "queue_redraw":
			continue
		if animation.method_track_get_params(track_index, 0).size() != 0:
			continue
		event_found = true
		break

	if not event_found:
		root.free()
		fail("exact Subject.queue_redraw event at 0.375s missing")
		return

	var particles := root.get_node_or_null("B1Particles") as GPUParticles2D
	if particles == null:
		root.free()
		fail("B1Particles GPUParticles2D missing")
		return
	if particles.amount != EXPECTED_PARTICLE_AMOUNT:
		root.free()
		fail("particle amount must be 96")
		return
	if absf(particles.lifetime - EXPECTED_PARTICLE_LIFETIME) > 0.0000001:
		root.free()
		fail("particle lifetime must be 0.8s")
		return
	if particles.emitting:
		root.free()
		fail("particles must be authored non-emitting for deterministic qualification")
		return

	root.free()
	print("B1 content verifier: OK")
	quit(0)
