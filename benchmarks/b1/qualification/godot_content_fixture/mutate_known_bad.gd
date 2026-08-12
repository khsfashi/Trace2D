extends SceneTree

func _initialize() -> void:
	var packed := load("res://main.tscn") as PackedScene
	if packed == null:
		push_error("B1 known-bad mutator: main.tscn load failed")
		quit(2)
		return
	var root := packed.instantiate()
	var particles := root.get_node_or_null("B1Particles") as GPUParticles2D
	if particles == null:
		root.free()
		push_error("B1 known-bad mutator: B1Particles missing")
		quit(2)
		return

	particles.amount = 95
	var mutated := PackedScene.new()
	var pack_error := mutated.pack(root)
	root.free()
	if pack_error != OK:
		push_error("B1 known-bad mutator: pack failed")
		quit(2)
		return
	var save_error := ResourceSaver.save(mutated, "res://main.tscn")
	if save_error != OK:
		push_error("B1 known-bad mutator: save failed")
		quit(2)
		return
	print("B1 known-bad mutator: amount=95")
	quit(0)
