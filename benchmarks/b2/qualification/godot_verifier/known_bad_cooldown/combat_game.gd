extends Node2D

const ATTACK_COOLDOWN_STEPS := 5
const MOVE_STEP := 4.0
const ATTACK_RANGE := 32.0

@onready var player: Node2D = $Player
@onready var enemy: Node2D = $Enemy
@onready var player_hud: ProgressBar = $HUD/PlayerHP
@onready var enemy_hud: ProgressBar = $HUD/EnemyHP
@onready var hit_particles: CPUParticles2D = $Enemy/HitParticles
@onready var death_particles: CPUParticles2D = $Enemy/DeathParticles

var cooldown_remaining := 0

func _physics_process(_delta: float) -> void:
    if Input.is_action_pressed("move_left"):
        player.position.x -= MOVE_STEP
    if Input.is_action_pressed("move_right"):
        player.position.x += MOVE_STEP
    if Input.is_action_pressed("move_up"):
        player.position.y -= MOVE_STEP
    if Input.is_action_pressed("move_down"):
        player.position.y += MOVE_STEP

    if cooldown_remaining > 0:
        cooldown_remaining -= 1
    if Input.is_action_just_pressed("attack") and cooldown_remaining == 0 and not bool(enemy.get_meta("dead")):
        var delta := enemy.position - player.position
        if absf(delta.x) <= ATTACK_RANGE and absf(delta.y) <= ATTACK_RANGE:
            var hp := int(enemy.get_meta("hp")) - 1
            enemy.set_meta("hp", hp)
            enemy.set_meta("hit_particle_triggers", int(enemy.get_meta("hit_particle_triggers")) + 1)
            hit_particles.restart()
            cooldown_remaining = ATTACK_COOLDOWN_STEPS
            if hp == 0 and not bool(enemy.get_meta("dead")):
                enemy.set_meta("dead", true)
                enemy.set_meta("death_transitions", int(enemy.get_meta("death_transitions")) + 1)
                enemy.set_meta("death_particle_triggers", int(enemy.get_meta("death_particle_triggers")) + 1)
                death_particles.restart()

    player_hud.value = int(player.get_meta("hp"))
    enemy_hud.value = int(enemy.get_meta("hp"))
