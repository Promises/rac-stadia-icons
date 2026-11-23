# The Stadia pictures

`stadia/*.png` (a, b, x, y, l1, r1, l2, r2, l_stick, r_stick: 100 x 100, RGBA) are Google Stadia
controller prompts from **Xelu's FREE Controllers & Keyboard PROMPTS**, made by Nicolae (XELU)
Berbece and released under Creative Commons 0 (CC0, public domain dedication):
<https://thoseawesomeguys.com/prompts/>.

They were taken, unchanged, from the copy bundled in Ricardo Subtil's
[controller_icons](https://github.com/rsubtil/controller_icons) 3.1.8
(`addons/controller_icons/assets/stadia/`), whose README says the controller assets are Xelu's under
CC0. Only the pictures are used here; none of that addon's code (MIT) is.

`tools/stadia_art.py` turns them into `src/common/stadia_art.h`, which the mod is built with.
