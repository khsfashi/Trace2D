if(NOT DEFINED TRACE2D_G1_RUNTIME_DIR OR TRACE2D_G1_RUNTIME_DIR STREQUAL "")
    message(FATAL_ERROR "TRACE2D_G1_RUNTIME_DIR must be provided")
endif()

file(MAKE_DIRECTORY "${TRACE2D_G1_RUNTIME_DIR}/textures")
file(MAKE_DIRECTORY "${TRACE2D_G1_RUNTIME_DIR}/audio")
file(MAKE_DIRECTORY "${TRACE2D_G1_RUNTIME_DIR}/fonts")
file(MAKE_DIRECTORY "${TRACE2D_G1_RUNTIME_DIR}/licenses")

function(trace2d_g1_fetch url relative_path)
    set(destination "${TRACE2D_G1_RUNTIME_DIR}/${relative_path}")
    if(EXISTS "${destination}")
        return()
    endif()

    message(STATUS "Nightfall Survivors asset: ${relative_path}")
    file(DOWNLOAD
        "${url}"
        "${destination}"
        STATUS download_status
        SHOW_PROGRESS
        TLS_VERIFY ON
        TIMEOUT 90
    )
    list(GET download_status 0 status_code)
    list(GET download_status 1 status_message)
    if(NOT status_code EQUAL 0)
        file(REMOVE "${destination}")
        message(FATAL_ERROR "Failed to fetch ${relative_path}: ${status_message}")
    endif()
endfunction()

set(TRACE2D_G1_KENNEY_ROOT
    "https://raw.githubusercontent.com/wyatt-raex/2d_survivors_game/72db959453fedc08409416ef60567955955f9e2b/assets/kenney_tiny-dungeon/Tiles")

# Kenney Tiny Dungeon 1.0, CC0. Nightfall deliberately assigns separate source
# resources to every playable character, enemy archetype, stage floor pair, and skill icon.
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0085.png" "textures/hero-star.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0086.png" "textures/hero-ember.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0087.png" "textures/hero-moon.png")

trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0084.png" "textures/enemy-ghoul.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0088.png" "textures/enemy-brute.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0089.png" "textures/enemy-wisp.png")

trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0048.png" "textures/stage-moon-floor-a.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0049.png" "textures/stage-moon-floor-b.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0050.png" "textures/stage-ember-floor-a.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0051.png" "textures/stage-ember-floor-b.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0052.png" "textures/stage-astral-floor-a.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0053.png" "textures/stage-astral-floor-b.png")

trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0104.png" "textures/skill-rapid.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0105.png" "textures/skill-might.png")
trace2d_g1_fetch("${TRACE2D_G1_KENNEY_ROOT}/tile_0106.png" "textures/skill-orbit.png")

# Kenney Puzzle Pack 2 particle sprite, CC0, from the pinned CC0-only archive.
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/Tiddybub/2d-assets/e0cbe0d995554a490d4c182fe9beb8769ffbb606/effects/puzzle-pack-2/PNG/Particles%20white/particleWhite_3.png"
    "textures/particle.png"
)

# Galmuri11 Bold is a Korean pixel UI font under SIL Open Font License 1.1.
# Pin the exact upstream release commit so the product typography is reproducible.
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/quiple/galmuri/71e1cacf1437a11220307120e63e30bc275312d4/dist/Galmuri11-Bold.ttf"
    "fonts/Galmuri11-Bold.ttf"
)
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/quiple/galmuri/71e1cacf1437a11220307120e63e30bc275312d4/ofl.md"
    "licenses/Galmuri-OFL.txt"
)

# Brickstorm's credits map these MP3 fallbacks to Kenney CC0 packs. Pin the exact source commit.
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/manuel-palacio/brickstorm/9327739b5a85b4819b95145ccf08d6664eab8f3c/public/audio/laser.mp3"
    "audio/laser.mp3"
)
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/manuel-palacio/brickstorm/9327739b5a85b4819b95145ccf08d6664eab8f3c/public/audio/brick-hit.mp3"
    "audio/brick-hit.mp3"
)
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/manuel-palacio/brickstorm/9327739b5a85b4819b95145ccf08d6664eab8f3c/public/audio/brick-break.mp3"
    "audio/brick-break.mp3"
)
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/manuel-palacio/brickstorm/9327739b5a85b4819b95145ccf08d6664eab8f3c/public/audio/powerup-get.mp3"
    "audio/powerup-get.mp3"
)
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/manuel-palacio/brickstorm/9327739b5a85b4819b95145ccf08d6664eab8f3c/public/audio/life-lost.mp3"
    "audio/life-lost.mp3"
)
