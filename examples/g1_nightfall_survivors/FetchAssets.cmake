if(NOT DEFINED TRACE2D_G1_RUNTIME_DIR OR TRACE2D_G1_RUNTIME_DIR STREQUAL "")
    message(FATAL_ERROR "TRACE2D_G1_RUNTIME_DIR must be provided")
endif()

file(MAKE_DIRECTORY "${TRACE2D_G1_RUNTIME_DIR}/textures")
file(MAKE_DIRECTORY "${TRACE2D_G1_RUNTIME_DIR}/audio")

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
        TIMEOUT 45
    )
    list(GET download_status 0 status_code)
    list(GET download_status 1 status_message)
    if(NOT status_code EQUAL 0)
        file(REMOVE "${destination}")
        message(FATAL_ERROR "Failed to fetch ${relative_path}: ${status_message}")
    endif()
endfunction()

# Character: OpenGameArt CC0 walk cycle, mirrored through Tiddybub's CC0-only archive.
# The source sheet is 192x128, 8 columns x 4 rows (24x32 cells).
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/Tiddybub/2d-assets/e0cbe0d995554a490d4c182fe9beb8769ffbb606/fantasy/oga-2d-rpg-character-walk-spritesheet/rpg_sprite_walk.png"
    "textures/warden-walk.png"
)

# Kenney Tiny Dungeon 1.0, CC0. These exact source files are pinned through the public sample repo.
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/wyatt-raex/2d_survivors_game/72db959453fedc08409416ef60567955955f9e2b/assets/kenney_tiny-dungeon/Tiles/tile_0048.png"
    "textures/floor.png"
)
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/wyatt-raex/2d_survivors_game/72db959453fedc08409416ef60567955955f9e2b/assets/kenney_tiny-dungeon/Tiles/tile_0049.png"
    "textures/floor-alt.png"
)
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/wyatt-raex/2d_survivors_game/72db959453fedc08409416ef60567955955f9e2b/assets/kenney_tiny-dungeon/Tiles/tile_0084.png"
    "textures/ghoul.png"
)
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/wyatt-raex/2d_survivors_game/72db959453fedc08409416ef60567955955f9e2b/assets/kenney_tiny-dungeon/Tiles/tile_0085.png"
    "textures/brute.png"
)

# Kenney Puzzle Pack 2 particle sprite, CC0, from the pinned CC0-only archive.
trace2d_g1_fetch(
    "https://raw.githubusercontent.com/Tiddybub/2d-assets/e0cbe0d995554a490d4c182fe9beb8769ffbb606/effects/puzzle-pack-2/PNG/Particles%20white/particleWhite_3.png"
    "textures/particle.png"
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
