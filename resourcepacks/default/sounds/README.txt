Sounds of the default pack (placeholders made by tools/art/make_sounds.py). [U-259][U-295]

Replace any file with your own (e.g. exported from FL Studio): WAV, mono or stereo, any sample rate.
Best: put your files in your OWN pack (resourcepacks/<name>/sounds/... same names) and pick it in
Options → Graphics; anything you did not make comes from this default pack.

fx/<shape>_<element>.wav   skill sounds
    shape:   bolt wave burst field cone tornado lantern auto towershot boulder heal steam dash
    element: Water Earth Fire Air Lightning Non-bender
hit_<element>.wav          a skill hits (Water Earth Fire Air Non-bender, Lightning)
hit_auto.wav auto_ranged.wav death.wav death_champion.wav
ui/  click buy sell gold level_up error
world/  structure_down recall footstep splash
en/announce/<id>.wav pl/announce/<id>.wav   announcer (ids: see tools/art/make_sounds.py ANNOUNCE)
music/menu.wav music/match.wav
