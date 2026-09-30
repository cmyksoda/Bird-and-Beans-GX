# Bird & Beans GX

A Wii homebrew port of **Bird & Beans / Pyoro**, with both games, original gameplay code, music and sound effects, and a new title screen and pause menu.

|Title Screen|Pause Menu|Graphics Options|
|---|---|---|
|<img width="642" height="480" alt="LULZHB_2026-09-29_19-19-06" src="https://github.com/user-attachments/assets/6aaad610-d461-4b93-9e26-b12e5edf1287" />|<img width="642" height="480" alt="LULZHB_2026-09-29_19-19-23" src="https://github.com/user-attachments/assets/acf00735-e46d-49da-8637-02e759162e1b" />|<img width="642" height="480" alt="LULZHB_2026-09-29_19-19-43" src="https://github.com/user-attachments/assets/f5986e3a-d8e1-4a2e-8c4c-38ae77b4ec90" />|


## Install

You’ll need the Homebrew Channel, an SD card, and your own **(USA)(EnFrEs)** DSiWare ROM.

1. Extract the release ZIP to the root of your SD card.
2. Put your `.nds` ROM in `sd:/apps/birdbeans/`, beside `boot.dol`. Any filename is fine. Unzip it first if it’s in a ZIP.
3. Launch **Bird & Beans GX** from the Homebrew Channel.

The app checks the ROM and prepares its data automatically on first launch. Later launches use the saved cache, so you're free to remove the ROM after first successful boot. Other regions and revisions aren’t supported.

Cute Pyoro transition voices are included for the loading[^1] screens. :3

## Controls

| Controller | Move | Tongue / spit | Pause | Back / exit |
|---|---|---|---|---|
| Wii Remote, horizontal | D-pad | 1 / 2 | Plus | Home |
| Wii Remote, vertical | D-pad | A / B | Plus | Home |
| Wii Remote + Nunchuk | Stick | C / Z | Plus | Home |
| Classic Controller | D-pad / left stick | A / B | Plus | Home |
| GameCube Controller | D-pad / stick | A / B | Start | Z |

In menus, select with A / 2 and go back with B. Press a button or move the stick on another controller to switch all controls to it, including during play.

## Display and saves

Open **Pause → Graphics Options** to toggle **240p at 60 Hz** and choose between **2× and Fill** scaling. Scaling applies to gameplay and pause menus. The title and loading screen always use the full screen. The fixed scale keeps the original proportions and picture size when switching video modes; 2× uses 384 lines in normal output and 192 in 240p. Your choices are saved when you leave the menu and restored on the next launch.

Fill detects the Wii’s 4:3 or 16:9 setting. Widescreen systems also show **Aspect Ratio** for the option to use a centered 4:3 view.

High scores and graphics options are saved in `apps/birdbeans/scores.dat`. Scores are saved on game over, restart, return to title or a clean exit. Keep that file when updating. Powering off during a run can lose an unsaved record.

## License

The port code and documentation are **GPLv3**. Nintendo game data and the included Pyoro recordings are excluded; third-party components retain their own licenses. See [license notices](licenses/README.md).

[^1]: The loading screens aren't real loading screens, they're just cute little transitions. It felt odd to have the game start instantly.
