# Hoodie 1.0.0
by: Zarianna

<img width="710" height="1383" alt="hood-up" src="https://github.com/user-attachments/assets/24aa6abb-b327-450f-942a-50a4fb4ec2b4" />
<img width="710" height="1383" alt="hood-down" src="https://github.com/user-attachments/assets/93d3c3bf-db68-4943-8af6-6369994f86ff" />

Raise or lower your character's hood with `/displayhead`.

Hoodie works with the armor listed below on all player races. It automatically uses the right hood for the armor you are wearing. Only your view of the game changes; installing Hoodie does not change what other players see on their screens.

| Hood appearance | Armor that uses it |
|---|---|
| **Royal Footman's Tunic** | Tunic, Tunic +1, Royal Footman's Tunic, Black Tunic, Mage's Tunic, San d'Orian Tunic, Kingdom Tunic, Fomor Tunic, Mana Tunic |
| **Cloak** | Cloak, Cloak +1, Combat Caster's Cloak, Combat Caster's Cloak +1, Combat Caster's Cloak +2, Mana Cloak |
| **Silk Cloak** | Silk Cloak, Silk Cloak +1, White Cloak, White Cloak +1, Royal Knight's Cloak, Royal Knight's Cloak +1, Royal Knight's Cloak +2, Shaman's Cloak, High Mana Cloak |
| **Black Cloak** | Black Cloak, Demon's Cloak, Faerie Tunic, Vampire Cloak |
| **Vermillion Cloak** | Vermillion Cloak, Royal Cloak |

All eight player race choices are supported. Armor with a different appearance keeps its normal look and function.


## Installing

1. Close the game, or unload the old Hoodie plugin first.
2. Open the player ZIP and copy its **plugins** and **resources** folders into your Ashita folder. Merge them with the folders already there, and replace the Hoodie files when asked.
3. In game, enter `/load hoodie`.
4. Enter `/hoodie` to open the settings.

Keep both resource folders included in the ZIP. You do not need to put anything in the **addons** folder. Updating Hoodie keeps your saved **Animate change** choice.

If you are upgrading from the test version while its race/armor preview is active, click **Restore appearance** before unloading it. The release does not have race or armor swapping.

## Using your hood

| Command | What it does |
|---|---|
| `/hoodie` | Opens or closes settings. |
| `/displayhead off` | Lowers your hood. |
| `/displayhead on` | Raises your hood. |
| `/displayhead` | Toggles between raised and lowered. |

In the settings:

- **Enable Hoodie** turns the fitted hoods on or off.
- **Other players** lets you see the supported hoods on other characters.
- **Animate change** plays the hood-change animation while you are standing idle. Turn it off for an immediate change. This choice is saved between sessions.

You can also use `/hoodie enable`, `/hoodie disable`, and `/hoodie status`.

## Supported armor

This list covers the original game through **Treasures of Aht Urhgan**. Items in each row share the same hood appearance, including the listed HQ versions.

## Things to know

- **Other players must be reloaded after they change displayhead.** Their hood does not update live on your screen when they toggle `/displayhead`. Reload that character on your clientâ€”for example, by moving far enough away or zoning to see their new hood state.
- The hood-change animation is for your own character while standing idle. During movement or other actions, the hood changes without the animation.
- This is a visual change on your client. Other players need their own copy of Hoodie to see the fitted hoods on their screens.

## Updating or removing Hoodie

Unload Hoodie or close the game before replacing `hoodie.dll`. Keep your existing settings file when updating.

To stop using Hoodie, enter `/unload hoodie` and remove its plugin and resource files after closing the game. Older test-version files in `resources/hoodie-preview` are no longer used by this release.

