extends Node2D

func _ready() -> void:
	var animation := Animation.new()
	animation.length = 0.5
	animation.loop_mode = Animation.LOOP_LINEAR

	var frame_track := animation.add_track(Animation.TYPE_VALUE)
	animation.track_set_path(frame_track, NodePath("Subject:frame"))
	animation.value_track_set_update_mode(frame_track, Animation.UPDATE_DISCRETE)
	animation.track_insert_key(frame_track, 0.0, 0)
	animation.track_insert_key(frame_track, 0.1, 1)
	animation.track_insert_key(frame_track, 0.25, 2)

	var event_track := animation.add_track(Animation.TYPE_METHOD)
	animation.track_set_path(event_track, NodePath("Subject"))
	animation.track_insert_key(event_track, 0.249, {"method": &"queue_redraw", "args": []})

	var library := AnimationLibrary.new()
	library.add_animation(&"attack", animation)
	$B1Animation.add_animation_library(&"", library)
